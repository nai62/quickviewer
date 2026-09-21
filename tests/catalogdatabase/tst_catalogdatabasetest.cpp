#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtSql>
#include <QtTest>

#include "catalogbuilder.h"
#include "catalogdatabase.h"

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
    CatalogFixture() { m_ready = m_directory.isValid() && writeShippedDatabase(databasePath()); }

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

private:
    QTemporaryDir m_directory;
    bool m_ready = false;
};

class CatalogDatabaseTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storesCoversUnderTheVolumeThatOwnsThem();
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
    QCOMPARE(probe.count(QStringLiteral("t_files")), 2);

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
    QCOMPARE(volumesWithCover, 2);
}

void CatalogDatabaseTest::parsesVolumeNames_data()
{
    QTest::addColumn<QString>("realname");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("tag");
    QTest::addColumn<int>("type");

    QTest::newRow("plain") << QStringLiteral("Book Title") << QStringLiteral("Book Title")
                           << QString() << -1;
    QTest::newRow("year") << QStringLiteral("Star Wars (2017)") << QStringLiteral("Star Wars")
                          << QStringLiteral("2017") << 0;
    QTest::newRow("author") << QStringLiteral("[Author] Book Title")
                            << QStringLiteral("[Author] Book Title") << QStringLiteral("Author")
                            << 2;
}

void CatalogDatabaseTest::parsesVolumeNames()
{
    QFETCH(QString, realname);
    QFETCH(QString, title);
    QFETCH(QString, tag);
    QFETCH(int, type);

    const TaggedName parsed = CatalogBuilder::parseVolumeName(realname);
    QCOMPARE(parsed.name, title);
    QCOMPARE(parsed.realname, realname);
    QCOMPARE(parsed.tags.size(), tag.isEmpty() ? 0 : 1);
    if (!tag.isEmpty()) {
        QCOMPARE(parsed.tags.first().name, tag);
        QCOMPARE(parsed.tags.first().type_id, type);
    }
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
