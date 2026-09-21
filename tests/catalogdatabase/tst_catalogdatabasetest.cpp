#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtSql>
#include <QtTest>

#include "catalogbuilder.h"
#include "catalogdatabase.h"
#include "managedatabasedialog.h"
#include "volumenameparser.h"

namespace {

/** Writes an image the catalog can store as a front page. */
bool writeImage(const QString &path, const QSize &size)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::red);
    return image.save(path);
}

/** Copies the catalog database the application ships to \a dbPath. */
bool writeShippedDatabase(const QString &dbPath)
{
    QFile source(QStringLiteral(":/databases/thumbnail_database"));
    if (!source.open(QIODevice::ReadOnly)) {
        return false;
    }
    QFile target(dbPath);
    if (!target.open(QIODevice::WriteOnly)) {
        return false;
    }
    return target.write(source.readAll()) > 0;
}

/** One catalog to create, the way the manage dialog asks for it. */
CatalogRecord catalogRequest(const QString &name, const QString &path)
{
    CatalogRecord request = {0};
    request.name = name;
    request.path = path;
    return request;
}

} // namespace

/**
 * A second connection to the catalog database, used to read what the database
 * stored and to break the schema on purpose.
 */
class CatalogProbe
{
public:
    CatalogProbe(const QString &dbPath, const QString &name)
        : m_connectionName(name)
    {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_db.setDatabaseName(dbPath);
        m_db.open();
    }

    ~CatalogProbe()
    {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    bool isOpen() const { return m_db.isOpen(); }

    bool exec(const QString &statement)
    {
        QSqlQuery query(m_db);
        return query.exec(statement);
    }

    QVariant scalar(const QString &statement) const
    {
        QSqlQuery query(m_db);
        if (!query.exec(statement) || !query.next()) {
            return QVariant();
        }
        return query.value(0);
    }

    int count(const QString &table) const
    {
        return scalar(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)).toInt();
    }

private:
    QString m_connectionName;
    QSqlDatabase m_db;
};

/** Temporary catalog database and volume tree for one test function. */
class CatalogFixture
{
public:
    /**
     * With \a seedDatabase false the catalog database is left missing, so a
     * test can watch it being created on first use.
     */
    explicit CatalogFixture(bool seedDatabase = true)
    {
        m_ready = m_directory.isValid() && (!seedDatabase || writeShippedDatabase(databasePath()));
    }

    bool isReady() const { return m_ready; }
    QString databasePath() const { return m_directory.filePath(QStringLiteral("catalog.db")); }
    QString rootPath() const { return m_directory.filePath(QStringLiteral("Library")); }

    QString folder(const QString &name)
    {
        const QString path = QDir(rootPath()).filePath(name);
        QDir().mkpath(path);
        return path;
    }

    bool addImage(const QString &folderName, const QString &fileName, const QSize &size)
    {
        return writeImage(QDir(folder(folderName)).filePath(fileName), size);
    }

    bool addRootImage(const QString &fileName, const QSize &size)
    {
        QDir().mkpath(rootPath());
        return writeImage(QDir(rootPath()).filePath(fileName), size);
    }

private:
    QTemporaryDir m_directory;
    bool m_ready = false;
};

class CatalogDatabaseTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storesCoversUnderTheVolumeThatOwnsThem();
    void createsTheCatalogDatabaseOnFirstUse();
    void keepsAnUnreadableCatalogDatabase();
    void refusesADatabaseWithoutTheCatalogSchema();
    void removesVolumesWhoseFoldersAreGone();
    void catalogsAnArchiveOnItsOwn();
    void registersADroppedArchiveAsItsOwnCatalog();
    void parsesVolumeNames_data();
    void parsesVolumeNames();
    void finishesAnEmptyCatalogRequest();
    void cancelledBuildLeavesNoHalfBuiltCatalog();
    void failedCatalogBuildReleasesTheDatabase();
    void failedCatalogRemovalKeepsTheStoredRows();
    void orphanVolumeTagsDoNotBreakTagQueries();
};

void CatalogDatabaseTest::storesCoversUnderTheVolumeThatOwnsThem()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    // The folder the catalog is created from holds an image of its own, so it
    // is a volume like every folder below it.
    QVERIFY(fixture.addRootImage(QStringLiteral("00.png"), QSize(180, 120)));
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(200, 100)));
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("02.png"), QSize(80, 80)));
    QVERIFY(fixture.addImage(QStringLiteral("Beta"), QStringLiteral("cover.png"), QSize(50, 60)));
    // Gamma holds no image, so its volume is stored without a front page.
    fixture.folder(QStringLiteral("Gamma"));

    CatalogDatabase database(nullptr, fixture.databasePath());
    const CatalogRecord catalog =
        database.createCatalog(QStringLiteral("Library"), fixture.rootPath());
    QVERIFY(catalog.created);

    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.isOpen());
    QCOMPARE(probe.count(QStringLiteral("t_catalogs")), 1);
    QCOMPARE(probe.count(QStringLiteral("t_volumes")), 4);

    const int baseVolumeId =
        probe.scalar(QStringLiteral("SELECT basevolume_id FROM t_catalogs WHERE name = 'Library'"))
            .toInt();
    QCOMPARE(
        probe
            .scalar(
                QStringLiteral("SELECT realname FROM t_volumes WHERE id = %1").arg(baseVolumeId))
            .toString(),
        QStringLiteral("Library"));
    QCOMPARE(
        probe
            .scalar(
                QStringLiteral("SELECT name FROM t_files WHERE volume_id = %1").arg(baseVolumeId))
            .toString(),
        QStringLiteral("00.png"));

    const int alphaId =
        probe.scalar(QStringLiteral("SELECT id FROM t_volumes WHERE realname = 'Alpha'")).toInt();
    const int betaId =
        probe.scalar(QStringLiteral("SELECT id FROM t_volumes WHERE realname = 'Beta'")).toInt();
    QVERIFY(alphaId > 0);
    QVERIFY(betaId > 0);
    QVERIFY(alphaId != baseVolumeId);

    // The front page of a volume belongs to that volume, and it is the first
    // image of the folder in display order.
    QCOMPARE(
        probe.scalar(QStringLiteral("SELECT name FROM t_files WHERE volume_id = %1").arg(alphaId))
            .toString(),
        QStringLiteral("01.png"));
    QCOMPARE(
        probe.scalar(QStringLiteral("SELECT name FROM t_files WHERE volume_id = %1").arg(betaId))
            .toString(),
        QStringLiteral("cover.png"));
    QCOMPARE(probe.count(QStringLiteral("t_files")), 3);

    // The stored thumbnail keeps the shape of the image it was made from.
    const int thumbnailWidth =
        probe
            .scalar(QStringLiteral("SELECT width FROM t_thumbnails WHERE id = "
                                   "(SELECT thumb_id FROM t_files WHERE volume_id = %1)")
                        .arg(alphaId))
            .toInt();
    const int thumbnailHeight =
        probe
            .scalar(QStringLiteral("SELECT height FROM t_thumbnails WHERE id = "
                                   "(SELECT thumb_id FROM t_files WHERE volume_id = %1)")
                        .arg(alphaId))
            .toInt();
    QVERIFY(thumbnailWidth > 0);
    QVERIFY(thumbnailWidth < 200);
    QVERIFY(qAbs(double(thumbnailWidth) / thumbnailHeight - 2.0) < 0.05);

    // The folder without an image has no front page.
    const QVariant gammaFrontPage =
        probe.scalar(QStringLiteral("SELECT frontpage_id FROM t_volumes WHERE realname = 'Gamma'"));
    QVERIFY(gammaFrontPage.isNull() || gammaFrontPage.toInt() == 0);

    const QList<VolumeThumbRecord> volumes = database.volumes();
    QCOMPARE(volumes.size(), 4);
    int volumesWithCover = 0;
    for (const VolumeThumbRecord &volume : volumes) {
        if (!volume.thumbnail.isEmpty()) {
            ++volumesWithCover;
        }
    }
    QCOMPARE(volumesWithCover, 3);
}

void CatalogDatabaseTest::createsTheCatalogDatabaseOnFirstUse()
{
    CatalogFixture fixture(false);
    QVERIFY(fixture.isReady());
    QVERIFY(!QFileInfo::exists(fixture.databasePath()));
    QVERIFY(fixture.addRootImage(QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    const CatalogRecord catalog =
        database.createCatalog(QStringLiteral("Library"), fixture.rootPath());
    QVERIFY(catalog.created);
    QVERIFY(QFileInfo::exists(fixture.databasePath()));
    QCOMPARE(database.catalogs().size(), 1);
    QCOMPARE(database.volumes().size(), 1);
}

void CatalogDatabaseTest::keepsAnUnreadableCatalogDatabase()
{
    CatalogFixture fixture(false);
    QVERIFY(fixture.isReady());
    const QByteArray contents("this file is not a catalog database");
    {
        QFile file(fixture.databasePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(contents) == contents.size());
    }

    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(!database.ensureReady());
    QVERIFY(!database.errorMessage().isEmpty());
    QVERIFY(database.catalogs().isEmpty());
    QVERIFY(!database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);

    // Whatever the file is, it is left where it is for the user to move aside.
    QFile file(fixture.databasePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), contents);
}

void CatalogDatabaseTest::refusesADatabaseWithoutTheCatalogSchema()
{
    CatalogFixture fixture(false);
    QVERIFY(fixture.isReady());
    {
        const QString connectionName = QStringLiteral("catalog-schema-probe");
        QSqlDatabase other = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        other.setDatabaseName(fixture.databasePath());
        QVERIFY(other.open());
        {
            QSqlQuery create(other);
            QVERIFY(create.exec(QStringLiteral("CREATE TABLE unrelated (id INTEGER)")));
        }
        other.close();
        other = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }

    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(!database.ensureReady());
    QVERIFY(!database.errorMessage().isEmpty());
    QCOMPARE(database.catalogs().size(), 0);
}

void CatalogDatabaseTest::removesVolumesWhoseFoldersAreGone()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));
    QVERIFY(fixture.addImage(QStringLiteral("Beta"), QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);
    // The folder the catalog was created from and the two folders below it.
    QCOMPARE(database.volumes().size(), 3);
    QVERIFY(database.missingVolumePaths().isEmpty());

    // One folder goes away, and the catalog still holds its volume.
    const QString gone = fixture.folder(QStringLiteral("Beta"));
    QVERIFY(QDir(gone).removeRecursively());
    QCOMPARE(database.missingVolumePaths().size(), 1);
    QCOMPARE(QFileInfo(database.missingVolumePaths().first()).fileName(), QStringLiteral("Beta"));
    QCOMPARE(database.volumes().size(), 3);

    QCOMPARE(database.removeMissingVolumes(), 1);
    QVERIFY(database.missingVolumePaths().isEmpty());
    QCOMPARE(database.volumes().size(), 2);
    for (const VolumeThumbRecord &volume : database.volumes()) {
        QVERIFY(volume.realname != QStringLiteral("Beta"));
    }

    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.isOpen());
    QCOMPARE(probe.count(QStringLiteral("t_volumes")), 2);
    QCOMPARE(probe.count(QStringLiteral("t_files")), 1);
    QCOMPARE(probe.count(QStringLiteral("t_thumbnails")), 1);
    QCOMPARE(probe.count(QStringLiteral("t_volumeorders")), 2);
    QCOMPARE(probe.count(QStringLiteral("t_fileorders")), 1);
}

void CatalogDatabaseTest::catalogsAnArchiveOnItsOwn()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    const QString archivePath = QDir(fixture.rootPath()).filePath(QStringLiteral("Book.zip"));
    QVERIFY(QDir().mkpath(fixture.rootPath()));
    QVERIFY(QFile::copy(
        QStringLiteral(CATALOGDATABASE_SRCDIR "../fileloader/data/deflate-utf8.zip"), archivePath));

    CatalogDatabase database(nullptr, fixture.databasePath());
    // A catalog whose folder is an archive holds that archive as its volume.
    const CatalogRecord catalog = database.createCatalog(QStringLiteral("Book"), archivePath);
    QVERIFY(catalog.created);
    QCOMPARE(database.volumes().size(), 1);
    const VolumeThumbRecord volume = database.volumes().first();
    QCOMPARE(volume.realname, QStringLiteral("Book.zip"));
    QVERIFY(!volume.thumbnail.isEmpty());
}

void CatalogDatabaseTest::registersADroppedArchiveAsItsOwnCatalog()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    const QString archivePath = QDir(fixture.rootPath()).filePath(QStringLiteral("Book.zip"));
    QVERIFY(QDir().mkpath(fixture.rootPath()));
    QVERIFY(QFile::copy(
        QStringLiteral(CATALOGDATABASE_SRCDIR "../fileloader/data/deflate-utf8.zip"), archivePath));

    CatalogDatabase database(nullptr, fixture.databasePath());
    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);

    // Dropping a book asks for that book, not for the folder that holds it.
    QMimeData mimeData;
    mimeData.setUrls({QUrl::fromLocalFile(archivePath)});
    QDropEvent event(QPointF(0, 0), Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
    dialog.dropEvent(&event);

    QTreeWidget *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"));
    QVERIFY(tree);
    QCOMPARE(tree->topLevelItemCount(), 1);
    QTreeWidgetItem *item = tree->topLevelItem(0);
    QVERIFY(item);
    QCOMPARE(item->text(0), QStringLiteral("* Book"));
    QCOMPARE(item->text(2), QDir::toNativeSeparators(archivePath));
}

void CatalogDatabaseTest::parsesVolumeNames_data()
{
    QTest::addColumn<QString>("realname");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QStringList>("tags"); // "name(type)"

    QTest::newRow("plain") << QStringLiteral("Book Title") << QStringLiteral("Book Title")
                           << QStringList();

    // A parenthesized field is a tag and leaves the title.
    QTest::newRow("trailing-field")
        << QStringLiteral("Sample Series (2017)") << QStringLiteral("Sample Series")
        << QStringList{QStringLiteral("2017(0)")};

    QTest::newRow("bracketed-field")
        << QStringLiteral("[Sample] Book Title") << QStringLiteral("Book Title")
        << QStringList{QStringLiteral("Sample(0)")};

    // The bracketed publisher and author are the one field that stays in the
    // title, and it counts as three tags.
    QTest::newRow("publisher-and-author") << QStringLiteral("[Publisher (Author)] Book Title")
                                          << QStringLiteral("[Publisher (Author)] Book Title")
                                          << QStringList{QStringLiteral("Publisher(2)"),
                                                         QStringLiteral("Author(3)"),
                                                         QStringLiteral("Publisher (Author)(1)")};

    QTest::newRow("leading-and-trailing-fields")
        << QStringLiteral("(First) [Publisher (Author)] Book Title (Last)")
        << QStringLiteral("[Publisher (Author)] Book Title")
        << QStringList{QStringLiteral("First(0)"),
                       QStringLiteral("Publisher(2)"),
                       QStringLiteral("Author(3)"),
                       QStringLiteral("Publisher (Author)(1)"),
                       QStringLiteral("Last(0)")};

    // A word after a leading "#" is a tag and stays whole: the parser buffers
    // it without brackets, so neither end may be trimmed.
    QTest::newRow("hash-word") << QStringLiteral("#Series Book Title")
                               << QStringLiteral("Book Title")
                               << QStringList{QStringLiteral("Series(0)")};

    QTest::newRow("hash-bracketed-field")
        << QStringLiteral("# [Series] Book Title") << QStringLiteral("Book Title")
        << QStringList{QStringLiteral("Series(0)")};

    QTest::newRow("hash-with-every-field")
        << QStringLiteral("# [First] [Second] [Publisher (Author)] Book Title (Last) [Third]")
        << QStringLiteral("[Publisher (Author)] Book Title")
        << QStringList{QStringLiteral("First(0)"),
                       QStringLiteral("Second(0)"),
                       QStringLiteral("Publisher(2)"),
                       QStringLiteral("Author(3)"),
                       QStringLiteral("Publisher (Author)(1)"),
                       QStringLiteral("Last(0)"),
                       QStringLiteral("Third(0)")};
}

void CatalogDatabaseTest::parsesVolumeNames()
{
    QFETCH(QString, realname);
    QFETCH(QString, title);
    QFETCH(QStringList, tags);

    const TaggedName parsed = parseVolumeName(realname);
    QStringList parsedTags;
    for (const TagRecord &tag : parsed.tags) {
        parsedTags << QStringLiteral("%1(%2)").arg(tag.name).arg(tag.type_id);
    }
    QCOMPARE(parsed.name, title);
    QCOMPARE(parsed.realname, realname);
    QCOMPARE(parsedTags, tags);
}

void CatalogDatabaseTest::finishesAnEmptyCatalogRequest()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());

    CatalogDatabase database(nullptr, fixture.databasePath());
    QFutureWatcher<QList<CatalogRecord>> *watcher =
        database.createCatalogAsync(QList<CatalogRecord>());
    watcher->waitForFinished();

    QVERIFY(watcher->result().isEmpty());
    QVERIFY(database.catalogs().isEmpty());
}

void CatalogDatabaseTest::cancelledBuildLeavesNoHalfBuiltCatalog()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    // Enough folders that the build is still running when it is cancelled.
    for (int index = 0; index < 25; ++index) {
        QVERIFY(fixture.addImage(
            QStringLiteral("Volume%1").arg(index), QStringLiteral("01.png"), QSize(120, 160)));
    }

    CatalogDatabase database(nullptr, fixture.databasePath());
    QList<CatalogRecord> requests;
    requests << catalogRequest(QStringLiteral("Library"), fixture.rootPath());
    QFutureWatcher<QList<CatalogRecord>> *watcher = database.createCatalogAsync(requests);
    database.cancelCreateCatalogAsync();
    watcher->waitForFinished();
    // A cancelled future keeps no result, so what the build stored is what
    // shows what it did. The catalog is stored whole or not at all.
    QVERIFY(watcher->isCanceled());

    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.isOpen());
    const int stored = probe.count(QStringLiteral("t_catalogs"));
    QCOMPARE(database.catalogs().size(), stored);
    QCOMPARE(probe.count(QStringLiteral("t_volumes")), 26 * stored);
    QCOMPARE(probe.count(QStringLiteral("t_files")), 25 * stored);

    // The database is usable after the cancelled build, and the catalog built
    // next is complete.
    const CatalogRecord after = database.createCatalog(QStringLiteral("After"), fixture.rootPath());
    QVERIFY(after.created);
    QCOMPARE(database.catalogs().size(), stored + 1);
    QCOMPARE(database.volumes().size(), 26 * (stored + 1));
}

void CatalogDatabaseTest::failedCatalogBuildReleasesTheDatabase()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.isOpen());

    // A catalog cannot store its covers while the thumbnail table is missing.
    QVERIFY(probe.exec(QStringLiteral("DROP TABLE t_thumbnails")));
    const CatalogRecord failed =
        database.createCatalog(QStringLiteral("Broken"), fixture.rootPath());
    QVERIFY(!failed.created);
    QVERIFY(database.catalogs().isEmpty());
    QCOMPARE(probe.count(QStringLiteral("t_volumes")), 0);

    // The failed build released the database: another connection can write to
    // it, and the next build succeeds.
    QVERIFY(probe.exec(QStringLiteral("CREATE TABLE t_thumbnails (id INTEGER PRIMARY KEY "
                                      "AUTOINCREMENT, width INTEGER NOT NULL, height INTEGER NOT "
                                      "NULL, thumbnail BLOB, created_at DATETIME)")));
    const CatalogRecord retried =
        database.createCatalog(QStringLiteral("Library"), fixture.rootPath());
    QVERIFY(retried.created);
    QCOMPARE(database.catalogs().size(), 1);
}

void CatalogDatabaseTest::failedCatalogRemovalKeepsTheStoredRows()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    const CatalogRecord catalog =
        database.createCatalog(QStringLiteral("Library"), fixture.rootPath());
    QVERIFY(catalog.created);

    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.isOpen());
    const int files = probe.count(QStringLiteral("t_files"));
    const int volumes = probe.count(QStringLiteral("t_volumes"));
    QVERIFY(files > 0);
    QVERIFY(volumes > 0);

    // Removing the catalog cannot finish while one of its tables is missing,
    // so it must not leave a part of the catalog behind.
    QVERIFY(probe.exec(QStringLiteral("DROP TABLE t_fileorders")));
    database.deleteCatalog(catalog.id);

    QCOMPARE(probe.count(QStringLiteral("t_catalogs")), 1);
    QCOMPARE(probe.count(QStringLiteral("t_files")), files);
    QCOMPARE(probe.count(QStringLiteral("t_volumes")), volumes);
}

void CatalogDatabaseTest::orphanVolumeTagsDoNotBreakTagQueries()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(
        QStringLiteral("Star Wars (2017)"), QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    const CatalogRecord catalog =
        database.createCatalog(QStringLiteral("Library"), fixture.rootPath());
    QVERIFY(catalog.created);

    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.isOpen());
    const int volumeId =
        probe.scalar(QStringLiteral("SELECT id FROM t_volumes WHERE realname = 'Star Wars (2017)'"))
            .toInt();
    QVERIFY(volumeId > 0);

    // A volume-tag row whose tag is gone must not take the tag queries down.
    QVERIFY(probe.exec(QStringLiteral("INSERT INTO t_volumetags (volume_id, tag_id, catalog_id) "
                                      "VALUES (%1, 99999, %2)")
                           .arg(volumeId)
                           .arg(catalog.id)));

    database.loadTags();
    const QMap<int, TagRecord *> byCount = database.tagsByCount();
    QCOMPARE(byCount.size(), 1);
    QCOMPARE(byCount.constBegin().value()->name, QStringLiteral("2017"));

    const QList<TagRecord> tags = database.getTagsFromVolumeId(volumeId);
    QCOMPARE(tags.size(), 1);
    QCOMPARE(tags.first().name, QStringLiteral("2017"));
}

QTEST_MAIN(CatalogDatabaseTest)

#include "tst_catalogdatabasetest.moc"
