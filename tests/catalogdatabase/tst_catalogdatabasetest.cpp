#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QToolButton>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QProcess>
#include <QDialogButtonBox>
#include "databasesettingdialog.h"
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtSql>
#include <QtTest>

#include "catalogbuilder.h"
#include "catalogwindow.h"
#include "catalogdatabase.h"
#include "managedatabasedialog.h"
#include "volumenameparser.h"
#include "volumetagdialog.h"

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

/** The shape (width over height) of the cover the dialog shows, or 0 for none. */
qreal shownCoverShape(const QLabel *cover)
{
    const QPixmap pixmap = cover->pixmap();
    return pixmap.isNull() ? 0.0 : qreal(pixmap.width()) / pixmap.height();
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
    void failedCommitLeavesNoCatalog();
    void failedBuildDoesNotReuseRolledBackTags();
    void deletingAllCatalogsDoesNotReuseDeletedTags();
    void failedTagEditKeepsTheTitleAndTags();
    void keepsAnExistingTemporaryFile();
    void rejectsInvalidCatalogSources();
    void ignoresInvalidDroppedUrls();
    void searchesForALiteralHyphen();
    void catalogsAFolderWithAnArchiveExtension();
    void doesNotFollowAFolderCycle();

    void createsTheCatalogDatabaseOnFirstUse();
    void keepsAnUnreadableCatalogDatabase();
    void refusesADatabaseWithoutTheCatalogSchema();
    void removesVolumesWhoseFoldersAreGone();
    void catalogsAnArchiveOnItsOwn();
    void registersADroppedArchiveAsItsOwnCatalog();
    void editsTheTitleAndTagsOfAVolume();
    void offersTheTitleAndTagsOfAVolumeForEditing();
    void addingATagWithEnterKeepsTheEditorOpen();
    void showsTheBooksOfTheSelectedCatalog();
    void listsTheFolderOfEachCatalog();
    void showsTheCoverOfTheSelectedBook();
    void parsesVolumeNames_data();
    void parsesVolumeNames();
    void finishesAnEmptyCatalogRequest();
    void buildsCatalogsOnAWorkerConnection();
    void keepsTheManagerLockedUntilCancellationFinishes();
    void managerBuildsPendingCatalogsWithoutACompletionDialog();
    void managerContextMenuTargetsTheClickedCatalog();
    void managerKeepsTheSelectedBookAfterEditing();
    void closingManagerCanKeepPendingCatalogs();
    void deletingAllCatalogsCanBeCancelled();
    void cancelledBuildLeavesNoHalfBuiltCatalog();
    void failedCatalogBuildReleasesTheDatabase();
    void failedCatalogRemovalKeepsTheStoredRows();
    void orphanVolumeTagsDoNotBreakTagQueries();
};

void CatalogDatabaseTest::searchesForALiteralHyphen()
{
    SearchWords words(QStringLiteral("-"));
    QVERIFY(words.match(QStringLiteral("a-b")));
    QVERIFY(!words.match(QStringLiteral("ab")));
    SearchWords exclude(QStringLiteral("book -draft"));
    QVERIFY(exclude.match(QStringLiteral("book final")));
    QVERIFY(!exclude.match(QStringLiteral("book draft")));
}

void CatalogDatabaseTest::rejectsInvalidCatalogSources()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addRootImage(QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    const QStringList invalid{QString(),
                              QDir(fixture.rootPath()).filePath(QStringLiteral("missing")),
                              QDir(fixture.rootPath()).filePath(QStringLiteral("01.png"))};
    for (const QString &path : invalid) {
        QVERIFY(!database.createCatalog(QStringLiteral("Invalid"), path).created);
        QVERIFY(!database.errorMessage().isEmpty());
        QVERIFY(database.catalogs().isEmpty());
        DatabaseSettingDialog dialog;
        dialog.setName(QStringLiteral("Invalid"));
        dialog.setPath(path);
        dialog.checkAcceptable();
        QVERIFY(!dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
    }
}

void CatalogDatabaseTest::ignoresInvalidDroppedUrls()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    CatalogDatabase database(nullptr, fixture.databasePath());
    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    QMimeData mime;
    mime.setUrls({QUrl(QStringLiteral("https://example.com/book")),
                  QUrl::fromLocalFile(fixture.rootPath())});
    QDropEvent drop(QPointF(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    dialog.dropEvent(&drop);
    QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"))->topLevelItemCount(), 0);
}

void CatalogDatabaseTest::catalogsAFolderWithAnArchiveExtension()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Book.zip"), QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);
    int covers = 0;
    for (const auto &volume : database.volumes()) {
        covers += !volume.thumbnail.isEmpty();
    }
    QCOMPARE(covers, 1);
}

void CatalogDatabaseTest::doesNotFollowAFolderCycle()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Book"), QStringLiteral("01.png"), QSize(60, 90)));
    const QString link =
        QDir(fixture.folder(QStringLiteral("Book"))).filePath(QStringLiteral("Loop"));
#ifdef Q_OS_WIN
    QProcess process;
    process.start(QStringLiteral("cmd.exe"),
                  {QStringLiteral("/d"),
                   QStringLiteral("/c"),
                   QStringLiteral("mklink"),
                   QStringLiteral("/J"),
                   QDir::toNativeSeparators(link),
                   QDir::toNativeSeparators(fixture.rootPath())});
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
#else
    QVERIFY(QFile::link(fixture.rootPath(), link));
#endif
    CatalogDatabase database(nullptr, fixture.databasePath());
    const bool created =
        database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created;
#ifdef Q_OS_WIN
    QVERIFY(QDir().rmdir(link));
#else
    QVERIFY(QFile::remove(link));
#endif
    QVERIFY(created);
    QCOMPARE(database.volumes().size(), 2);
}

void CatalogDatabaseTest::failedCommitLeavesNoCatalog()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(
        fixture.addImage(QStringLiteral("Book (Tag)"), QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.ensureReady());
    CatalogProbe reader(fixture.databasePath(), QStringLiteral("catalog-reader"));
    QVERIFY(reader.exec(QStringLiteral("BEGIN")));
    QCOMPARE(reader.count(QStringLiteral("t_catalogs")), 0);
    QSignalSpy created(&database, &CatalogDatabase::catalogCreated);
    // A reader can coexist with the writes, but prevents COMMIT in SQLite.
    QVERIFY(!database.createCatalog(QStringLiteral("Blocked"), fixture.rootPath()).created);
    QCOMPARE(created.size(), 0);
    QVERIFY(!database.errorMessage().isEmpty());
    QVERIFY(reader.exec(QStringLiteral("ROLLBACK")));
    QCOMPARE(reader.count(QStringLiteral("t_catalogs")), 0);
    QCOMPARE(reader.count(QStringLiteral("t_tags")), 0);
    QVERIFY(database.createCatalog(QStringLiteral("Retry"), fixture.rootPath()).created);
    QCOMPARE(reader.count(QStringLiteral("t_catalogs")), 1);
}

void CatalogDatabaseTest::failedBuildDoesNotReuseRolledBackTags()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(
        fixture.addImage(QStringLiteral("Book (Tag)"), QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.ensureReady());
    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.exec(QStringLiteral("CREATE TRIGGER fail_cover BEFORE INSERT ON t_thumbnails "
                                      "BEGIN SELECT RAISE(ABORT, 'cover failure'); END")));
    QVERIFY(!database.createCatalog(QStringLiteral("Broken"), fixture.rootPath()).created);
    QCOMPARE(probe.count(QStringLiteral("t_tags")), 0);
    QVERIFY(probe.exec(QStringLiteral("DROP TRIGGER fail_cover")));
    QVERIFY(database.createCatalog(QStringLiteral("Retry"), fixture.rootPath()).created);
    QCOMPARE(probe.count(QStringLiteral("t_tags")), 1);
    QCOMPARE(probe
                 .scalar(QStringLiteral("SELECT COUNT(*) FROM t_volumetags v "
                                        "JOIN t_tags t ON t.id = v.tag_id"))
                 .toInt(),
             1);
}

void CatalogDatabaseTest::deletingAllCatalogsDoesNotReuseDeletedTags()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(
        QStringLiteral("Book (Tag) (Tag)"), QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("First"), fixture.rootPath()).created);
    QVERIFY(database.deleteAllCatalogs());
    QVERIFY(database.createCatalog(QStringLiteral("Second"), fixture.rootPath()).created);
    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QCOMPARE(probe.count(QStringLiteral("t_tags")), 1);
    // Repeated fields in a name still assign the tag only once.
    QCOMPARE(probe.count(QStringLiteral("t_volumetags")), 1);
    QCOMPARE(probe
                 .scalar(QStringLiteral("SELECT COUNT(*) FROM t_volumetags v "
                                        "JOIN t_tags t ON t.id = v.tag_id"))
                 .toInt(),
             1);
}

void CatalogDatabaseTest::failedTagEditKeepsTheTitleAndTags()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(
        fixture.addImage(QStringLiteral("Book (Tag)"), QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);
    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    const int id =
        probe.scalar(QStringLiteral("SELECT id FROM t_volumes WHERE name = 'Book'")).toInt();
    QVERIFY(id > 0);
    QVERIFY(
        probe.exec(QStringLiteral("CREATE TRIGGER fail_title BEFORE UPDATE OF name ON t_volumes "
                                  "BEGIN SELECT RAISE(ABORT, 'title failure'); END")));
    QVERIFY(
        !database.setVolumeDetails(id, QStringLiteral("New title"), {QStringLiteral("New tag")}));
    QCOMPARE(database.getTagsFromVolumeId(id).first().name, QStringLiteral("Tag"));
    QCOMPARE(
        probe.scalar(QStringLiteral("SELECT name FROM t_volumes WHERE id = %1").arg(id)).toString(),
        QStringLiteral("Book"));
    QCOMPARE(probe.count(QStringLiteral("t_tags")), 1);
    QVERIFY(probe.exec(QStringLiteral("DROP TRIGGER fail_title")));
    QVERIFY(
        database.setVolumeDetails(id, QStringLiteral("New title"), {QStringLiteral("New tag")}));
    QCOMPARE(database.getTagsFromVolumeId(id).first().name, QStringLiteral("New tag"));
}

void CatalogDatabaseTest::keepsAnExistingTemporaryFile()
{
    CatalogFixture fixture(false);
    QVERIFY(fixture.isReady());
    QFile temporary(fixture.databasePath() + QStringLiteral(".tmp"));
    QVERIFY(temporary.open(QIODevice::WriteOnly));
    QCOMPARE(temporary.write("keep this file"), qint64(14));
    temporary.close();
    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.ensureReady());
    QVERIFY(temporary.open(QIODevice::ReadOnly));
    QCOMPARE(temporary.readAll(), QByteArray("keep this file"));
}

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
            // The record names the row that holds its cover: that is what the
            // catalog list keys the cover it has already read on.
            QVERIFY(volume.thumb_id > 0);
            QCOMPARE(probe
                         .scalar(QStringLiteral("SELECT COUNT(*) FROM t_thumbnails WHERE id = %1")
                                     .arg(volume.thumb_id))
                         .toInt(),
                     1);
        } else {
            QCOMPARE(volume.thumb_id, 0);
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

void CatalogDatabaseTest::editsTheTitleAndTagsOfAVolume()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);

    int alphaId = -1;
    const QList<VolumeThumbRecord> created = database.volumes();
    for (const VolumeThumbRecord &volume : created) {
        if (volume.realname == QStringLiteral("Alpha")) {
            alphaId = volume.id;
        }
    }
    QVERIFY(alphaId > 0);

    // The user gives the volume a title and two tags of their own.
    QVERIFY(database.setVolumeDisplayName(alphaId, QStringLiteral("Edited Title")));
    QVERIFY(database.setVolumeTags(alphaId, {QStringLiteral("First"), QStringLiteral("Second")}));

    QString title;
    for (const VolumeThumbRecord &volume : database.volumes()) {
        if (volume.id == alphaId) {
            title = volume.name;
        }
    }
    QCOMPARE(title, QStringLiteral("Edited Title"));

    QStringList names;
    for (const TagRecord &tag : database.getTagsFromVolumeId(alphaId)) {
        names << tag.name;
    }
    QCOMPARE(names, QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
    QCOMPARE(database.tagsByCount().size(), 2);

    // Editing again reuses the tag the catalog already knows, whatever the
    // case the user typed, and adds the new one.
    QVERIFY(database.setVolumeTags(alphaId, {QStringLiteral("second"), QStringLiteral("Third")}));
    names.clear();
    for (const TagRecord &tag : database.getTagsFromVolumeId(alphaId)) {
        names << tag.name;
    }
    QCOMPARE(names, QStringList({QStringLiteral("Second"), QStringLiteral("Third")}));

    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QVERIFY(probe.isOpen());
    // The tag the volume no longer carries is gone with it.
    QCOMPARE(probe.count(QStringLiteral("t_tags")), 2);
    QCOMPARE(probe.count(QStringLiteral("t_volumetags")), 2);

    // A volume with no tags left has none of them.
    QVERIFY(database.setVolumeTags(alphaId, QStringList()));
    QVERIFY(database.getTagsFromVolumeId(alphaId).isEmpty());
    QVERIFY(database.tagsByCount().isEmpty());
}

void CatalogDatabaseTest::addingATagWithEnterKeepsTheEditorOpen()
{
    VolumeTagDialog dialog;
    dialog.setVolume(QStringLiteral("Book"), QStringLiteral("Book"), {}, {});
    dialog.show();
    auto *field = dialog.findChild<QLineEdit *>(QStringLiteral("newTagEdit"));
    QVERIFY(field);
    field->setFocus();
    QApplication::processEvents();
    QSignalSpy accepted(&dialog, &QDialog::accepted);
    field->setText(QStringLiteral("New tag"));
    QTest::keyClick(field, Qt::Key_Return);
    QCOMPARE(dialog.tags(), QStringList{QStringLiteral("New tag")});
    QVERIFY(field->text().isEmpty());
    QCOMPARE(accepted.size(), 0);
    QVERIFY(dialog.isVisible());
    field->setText(QStringLiteral("Another"));
    QTest::keyClick(field, Qt::Key_Enter);
    QCOMPARE(dialog.tags().size(), 2);
    QCOMPARE(accepted.size(), 0);
    dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    QCOMPARE(accepted.size(), 1);
}

void CatalogDatabaseTest::offersTheTitleAndTagsOfAVolumeForEditing()
{
    VolumeTagDialog dialog;
    dialog.setVolume(QStringLiteral("Stored Title"),
                     QStringLiteral("Folder Name"),
                     {QStringLiteral("First"), QStringLiteral("Second")},
                     {QStringLiteral("Second")});
    QCOMPARE(dialog.displayName(), QStringLiteral("Stored Title"));

    // The tags the catalog knows are offered, and the ones the volume carries
    // are ticked.
    QListWidget *list = dialog.findChild<QListWidget *>(QStringLiteral("tagList"));
    QVERIFY(list);
    QCOMPARE(list->count(), 2);
    QCOMPARE(dialog.tags(), QStringList({QStringLiteral("Second")}));

    // The user types a tag the catalog does not know yet.
    QLineEdit *newTag = dialog.findChild<QLineEdit *>(QStringLiteral("newTagEdit"));
    QPushButton *addTag = dialog.findChild<QPushButton *>(QStringLiteral("addTagButton"));
    QVERIFY(newTag);
    QVERIFY(addTag);
    newTag->setText(QStringLiteral("Third"));
    addTag->click();
    QCOMPARE(dialog.tags(), QStringList({QStringLiteral("Second"), QStringLiteral("Third")}));
    QVERIFY(newTag->text().isEmpty());

    // Taking a suggestion back is unticking it.
    list->item(1)->setCheckState(Qt::Unchecked);
    QCOMPARE(dialog.tags(), QStringList({QStringLiteral("Third")}));

    // The title the catalog shows is editable as well.
    QLineEdit *name = dialog.findChild<QLineEdit *>(QStringLiteral("nameEdit"));
    QVERIFY(name);
    name->setText(QStringLiteral("  Edited Title  "));
    QCOMPARE(dialog.displayName(), QStringLiteral("Edited Title"));
}

void CatalogDatabaseTest::showsTheBooksOfTheSelectedCatalog()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));
    QVERIFY(fixture.addImage(QStringLiteral("Beta"), QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);

    // One of the books carries a tag of its own.
    int alphaId = -1;
    for (const VolumeThumbRecord &volume : database.volumes()) {
        if (volume.realname == QStringLiteral("Alpha")) {
            alphaId = volume.id;
        }
    }
    QVERIFY(alphaId > 0);
    QVERIFY(database.setVolumeTags(alphaId, {QStringLiteral("Sample")}));

    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    QTreeWidget *catalogs = dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"));
    QTreeWidget *books = dialog.findChild<QTreeWidget *>(QStringLiteral("booksTree"));
    QPushButton *editTags = dialog.findChild<QPushButton *>(QStringLiteral("editTagsButton"));
    QVERIFY(catalogs);
    QVERIFY(books);
    QVERIFY(editTags);
    // Opening the manager selects a catalog and shows its books immediately.
    QVERIFY(catalogs->currentItem());
    // The folder the catalog was created from and the two folders below it.
    QCOMPARE(books->topLevelItemCount(), 3);
    // The editor is one press away as soon as a book is in the list.
    QVERIFY(editTags->isEnabled());
    bool found = false;
    for (int row = 0; row < books->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = books->topLevelItem(row);
        if (item->text(0) == QStringLiteral("Alpha")) {
            QCOMPARE(item->text(1), QStringLiteral("Sample"));
            found = true;
        }
    }
    QVERIFY(found);
}

void CatalogDatabaseTest::listsTheFolderOfEachCatalog()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);

    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    dialog.show();
    QApplication::processEvents();

    QTreeWidget *catalogs = dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"));
    QVERIFY(catalogs);
    QVERIFY(catalogs->topLevelItem(0));

    // The folder the catalog was created from is in the list, with the whole
    // path under the pointer for the width the column cannot show.
    const QTreeWidgetItem *item = catalogs->topLevelItem(0);
    QCOMPARE(item->text(2), QDir::toNativeSeparators(fixture.rootPath()));
    QCOMPARE(item->toolTip(2), item->text(2));
    // And the column is inside the room the list has rather than scrolled off
    // the right edge of it.
    QVERIFY(catalogs->columnViewportPosition(2) < catalogs->viewport()->width());
}

void CatalogDatabaseTest::showsTheCoverOfTheSelectedBook()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    // Two books whose pages ask for opposite shapes.
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));
    QVERIFY(fixture.addImage(QStringLiteral("Beta"), QStringLiteral("01.png"), QSize(90, 60)));

    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);

    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    dialog.show();
    QApplication::processEvents();

    QTreeWidget *catalogs = dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"));
    QTreeWidget *books = dialog.findChild<QTreeWidget *>(QStringLiteral("booksTree"));
    QLabel *cover = dialog.findChild<QLabel *>(QStringLiteral("coverLabel"));
    QAction *openInExplorer = dialog.findChild<QAction *>(QStringLiteral("openInExplorerAction"));
    QVERIFY(catalogs);
    QVERIFY(books);
    QVERIFY(cover);
    QVERIFY(openInExplorer);

    // Clearing selection also clears the cover and disables folder actions.
    catalogs->setCurrentItem(nullptr);
    QCOMPARE(shownCoverShape(cover), 0.0);
    QVERIFY(!openInExplorer->isEnabled());

    catalogs->setCurrentItem(catalogs->topLevelItem(0));
    QApplication::processEvents();

    // The folder is there to be shown as soon as a catalog is.
    QVERIFY(openInExplorer->isEnabled());
    // The first book of the catalog is selected, and its cover is shown with
    // the shape of the page it came from.
    QVERIFY(books->currentItem());
    QCOMPARE(books->currentItem()->text(0), QStringLiteral("Alpha"));
    QVERIFY(qAbs(shownCoverShape(cover) - 2.0 / 3.0) < 0.05);

    // Choosing another book shows that book's cover instead.
    QTreeWidgetItem *beta = nullptr;
    for (int row = 0; row < books->topLevelItemCount(); ++row) {
        if (books->topLevelItem(row)->text(0) == QStringLiteral("Beta")) {
            beta = books->topLevelItem(row);
        }
    }
    QVERIFY(beta);
    books->setCurrentItem(beta);
    QApplication::processEvents();
    QCOMPARE(books->currentItem()->text(0), QStringLiteral("Beta"));
    QVERIFY(qAbs(shownCoverShape(cover) - 3.0 / 2.0) < 0.05);

    // A book whose folder holds no image has no cover to show.
    QTreeWidgetItem *library = nullptr;
    for (int row = 0; row < books->topLevelItemCount(); ++row) {
        if (books->topLevelItem(row)->text(0) == QStringLiteral("Library")) {
            library = books->topLevelItem(row);
        }
    }
    QVERIFY(library);
    books->setCurrentItem(library);
    QApplication::processEvents();
    QCOMPARE(shownCoverShape(cover), 0.0);
}

void CatalogDatabaseTest::parsesVolumeNames_data()
{
    QTest::addColumn<QString>("realname");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QStringList>("tags"); // "name(type)"

    QTest::newRow("unfinished-parenthesis") << QStringLiteral("Book (unfinished")
                                            << QStringLiteral("Book (unfinished") << QStringList();
    QTest::newRow("unfinished-bracket") << QStringLiteral("Book [unfinished")
                                        << QStringLiteral("Book [unfinished") << QStringList();

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

void CatalogDatabaseTest::managerBuildsPendingCatalogsWithoutACompletionDialog()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Book"), QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    auto *start = dialog.findChild<QPushButton *>(QStringLiteral("cancelButton"));
    auto *catalogs = dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"));
    auto *close = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close);
    QVERIFY(start && catalogs && close);
    QVERIFY(!start->isEnabled());
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(fixture.rootPath())});
    QDropEvent drop(QPointF(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    dialog.dropEvent(&drop);
    QVERIFY(start->isEnabled());
    QVERIFY(close->isEnabled());
    QVERIFY(catalogs->currentItem());
    QVERIFY(catalogs->currentItem()->data(0, Qt::UserRole).toInt() < 0);
    start->click();
    QVERIFY(!close->isEnabled());
    // A surprise modal completion dialog must fail the test instead of hanging it.
    QTimer dismiss;
    bool showedMessage = false;
    connect(&dismiss, &QTimer::timeout, &dialog, [&] {
        if (auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            showedMessage = true;
            message->accept();
        }
    });
    dismiss.start(10);
    QTRY_VERIFY(close->isEnabled());
    QVERIFY(!showedMessage);
    QCOMPARE(database.catalogs().size(), 1);
    QVERIFY(!start->isEnabled());
    QVERIFY(catalogs->currentItem()->data(0, Qt::UserRole).toInt() > 0);
    QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("statusLabel"))->text().isEmpty());
}

void CatalogDatabaseTest::managerContextMenuTargetsTheClickedCatalog()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    const QString first = fixture.folder(QStringLiteral("First"));
    const QString second = fixture.folder(QStringLiteral("Second"));
    CatalogDatabase database(nullptr, fixture.databasePath());
    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});
    QDropEvent drop(QPointF(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    dialog.dropEvent(&drop);
    dialog.show();
    QApplication::processEvents();
    auto *catalogs = dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"));
    QVERIFY(catalogs->currentItem());
    QCOMPARE(catalogs->currentItem()->text(2), QDir::toNativeSeparators(second));
    auto *target = catalogs->topLevelItem(0) == catalogs->currentItem() ? catalogs->topLevelItem(1)
                                                                        : catalogs->topLevelItem(0);
    const QString targetPath = target->text(2);
    bool menuShown = false;
    QTimer::singleShot(0, &dialog, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        menuShown = true;
        QCOMPARE(catalogs->currentItem()->text(2), targetPath);
        auto *remove = dialog.findChild<QAction *>(QStringLiteral("deleteAction"));
        QVERIFY(menu->actions().contains(remove));
        QVERIFY(remove->isEnabled());
        // Removing an unbuilt request only removes that row.
        menu->close();
        remove->trigger();
    });
    dialog.handleCatalogContextMenu(catalogs->visualItemRect(target).center());
    QVERIFY(menuShown);
    QCOMPARE(catalogs->topLevelItemCount(), 1);
    QVERIFY(catalogs->currentItem()->text(2) != targetPath);
    bool keyboardMenuShown = false;
    QTimer::singleShot(0, &dialog, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        keyboardMenuShown = true;
        menu->close();
    });
    QContextMenuEvent keyboardMenu(QContextMenuEvent::Keyboard, QPoint(-1, -1), QPoint(-1, -1));
    QApplication::sendEvent(catalogs, &keyboardMenu);
    QVERIFY(keyboardMenuShown);
    dialog.findChild<QAction *>(QStringLiteral("deleteAction"))->trigger();
    QCOMPARE(catalogs->topLevelItemCount(), 0);
    QVERIFY(!dialog.findChild<QAction *>(QStringLiteral("editAction"))->isEnabled());
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("cancelButton"))->isEnabled());
}

void CatalogDatabaseTest::managerKeepsTheSelectedBookAfterEditing()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Alpha"), QStringLiteral("01.png"), QSize(60, 90)));
    QVERIFY(fixture.addImage(QStringLiteral("Beta"), QStringLiteral("01.png"), QSize(90, 60)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    const auto catalog = database.createCatalog(QStringLiteral("Library"), fixture.rootPath());
    QVERIFY(catalog.created);
    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    auto *books = dialog.findChild<QTreeWidget *>(QStringLiteral("booksTree"));
    for (int row = 0; row < books->topLevelItemCount(); ++row) {
        if (books->topLevelItem(row)->text(0) == QStringLiteral("Beta")) {
            books->setCurrentItem(books->topLevelItem(row));
        }
    }
    QVERIFY(books->currentItem());
    const int bookId = books->currentItem()->data(0, Qt::UserRole).toInt();
    QTimer::singleShot(0, &dialog, [&] {
        auto *editor = dialog.findChild<VolumeTagDialog *>();
        QVERIFY(editor);
        editor->findChild<QLineEdit *>(QStringLiteral("nameEdit"))
            ->setText(QStringLiteral("Edited"));
        editor->accept();
    });
    dialog.handleEditTagsButtonClicked();
    QCOMPARE(books->currentItem()->data(0, Qt::UserRole).toInt(), bookId);
    QCOMPARE(books->currentItem()->text(0), QStringLiteral("Edited"));
    dialog.resetCatalogList();
    QCOMPARE(books->currentItem()->data(0, Qt::UserRole).toInt(), bookId);
}

void CatalogDatabaseTest::closingManagerCanKeepPendingCatalogs()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    fixture.folder(QStringLiteral("Book"));
    CatalogDatabase database(nullptr, fixture.databasePath());
    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(fixture.rootPath())});
    QDropEvent drop(QPointF(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    dialog.dropEvent(&drop);
    dialog.show();
    QApplication::processEvents();
    auto answer = [&dialog](QMessageBox::StandardButton button) {
        QTimer::singleShot(0, &dialog, [&dialog, button] {
            auto *message = dialog.findChild<QMessageBox *>();
            QVERIFY(message);
            message->button(button)->click();
        });
    };
    answer(QMessageBox::No);
    dialog.close();
    QVERIFY(dialog.isVisible());
    QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"))->topLevelItemCount(), 1);
    answer(QMessageBox::Yes);
    dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close)->click();
    QVERIFY(!dialog.isVisible());
    QVERIFY(database.catalogs().isEmpty());
}

void CatalogDatabaseTest::deletingAllCatalogsCanBeCancelled()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    fixture.folder(QStringLiteral("Book"));
    CatalogDatabase database(nullptr, fixture.databasePath());
    QVERIFY(database.createCatalog(QStringLiteral("Library"), fixture.rootPath()).created);
    ManageDatabaseDialog dialog;
    dialog.setCatalogDatabase(&database);
    auto *menu = dialog.findChild<QToolButton *>(QStringLiteral("moreButton"))->menu();
    auto *remove = dialog.findChild<QAction *>(QStringLiteral("deleteAllAction"));
    QVERIFY(menu->actions().contains(remove));
    QTimer::singleShot(0, &dialog, [&] {
        auto *message = dialog.findChild<QMessageBox *>();
        QVERIFY(message);
        message->button(QMessageBox::No)->click();
    });
    remove->trigger();
    QCOMPARE(database.catalogs().size(), 1);
    QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"))->topLevelItemCount(), 1);
}

void CatalogDatabaseTest::keepsTheManagerLockedUntilCancellationFinishes()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(fixture.addImage(QStringLiteral("Book"), QStringLiteral("01.png"), QSize(60, 90)));
    CatalogDatabase database(nullptr, fixture.databasePath());
    {
        ManageDatabaseDialog dialog;
        dialog.setCatalogDatabase(&database);
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(fixture.rootPath())});
        QDropEvent drop(QPointF(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        dialog.dropEvent(&drop);
        dialog.handleCancelButtonClicked();
        dialog.handleCancelButtonClicked();
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("addButton"))->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("cancelButton"))->isEnabled());
        QVERIFY(!dialog.findChild<QTreeWidget *>(QStringLiteral("treeWidget"))->isEnabled());
        QVERIFY(!dialog.acceptDrops());
        // Destruction also waits for the worker; it cannot outlive the dialog.
    }
    QVERIFY(database.catalogWatcher()->isFinished());
    QVERIFY(database.createCatalog(QStringLiteral("After"), fixture.rootPath()).created);
}

void CatalogDatabaseTest::buildsCatalogsOnAWorkerConnection()
{
    CatalogFixture fixture;
    QVERIFY(fixture.isReady());
    QVERIFY(
        fixture.addImage(QStringLiteral("Book (Shared)"), QStringLiteral("01.png"), QSize(60, 90)));
    {
        CatalogDatabase first(nullptr, fixture.databasePath());
        QVERIFY(first.createCatalog(QStringLiteral("First"), fixture.rootPath()).created);
    }
    CatalogDatabase database(nullptr, fixture.databasePath());
    QCOMPARE(database.volumes().size(), 2);
    QSignalSpy created(&database, &CatalogDatabase::catalogCreated);
    auto *watcher = database.catalogWatcher();
    QSignalSpy finished(watcher, &QFutureWatcher<QList<CatalogRecord>>::finished);
    database.createCatalogAsync({catalogRequest(QStringLiteral("Second"), fixture.rootPath())});
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(watcher->result().size(), 1);
    QVERIFY(watcher->result().first().created);
    QCOMPARE(created.size(), 1);
    QCOMPARE(database.volumes().size(), 4);
    CatalogProbe probe(fixture.databasePath(), QStringLiteral("catalog-probe"));
    QCOMPARE(probe.count(QStringLiteral("t_tags")), 1);
    QCOMPARE(probe.count(QStringLiteral("t_volumetags")), 2);
    // The main connection still reads and writes after the worker closed its own.
    database.deleteCatalog(watcher->result().first().id);
    QCOMPARE(database.volumes().size(), 2);
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
