#include <QtTest>

#include "storagelocation.h"

namespace {

bool writeFile(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("[General]\n") > 0;
}

QString normalized(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

} // namespace

class StorageLocationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void markerSelectsApplicationDirectory();
    void withoutMarkerUserDataDirectoryIsUsed();
    void markerWinsOverExistingUserData();
    void directoryNamedLikeMarkerIsNoMarker();
    void absoluteSubPathIsKept();
#ifdef Q_OS_WIN
    void upstreamLayoutIsRead_data();
    void upstreamLayoutIsRead();
    void nativeApplicationDirectoryIsApplicationDirPath();
    void nativeUserDataDirectoryIsAppDataLocation();
#endif
};

void StorageLocationTest::markerSelectsApplicationDirectory()
{
    QTemporaryDir application;
    QTemporaryDir user;
    QVERIFY(application.isValid() && user.isValid());
    QVERIFY(writeFile(application.filePath(StorageLocation::portableMarkerName())));

    const StorageLocation location = StorageLocation::resolve(application.path(), user.path());
    QVERIFY(location.isPortable());
    QCOMPARE(normalized(location.directory()), normalized(application.path()));
}

void StorageLocationTest::withoutMarkerUserDataDirectoryIsUsed()
{
    // A settings file beside the executable is not a marker: a portable copy
    // updated by replacing the executable alone reads the user's settings
    // until the marker is put back.
    QTemporaryDir application;
    QTemporaryDir user;
    QVERIFY(application.isValid() && user.isValid());
    QVERIFY(writeFile(application.filePath(StorageLocation::settingsSubPath())));

    const StorageLocation location = StorageLocation::resolve(application.path(), user.path());
    QVERIFY(!location.isPortable());
    QCOMPARE(normalized(location.directory()), normalized(user.path()));
}

void StorageLocationTest::markerWinsOverExistingUserData()
{
    QTemporaryDir application;
    QTemporaryDir user;
    QVERIFY(application.isValid() && user.isValid());
    QVERIFY(writeFile(application.filePath(StorageLocation::portableMarkerName())));
    QVERIFY(writeFile(user.filePath(StorageLocation::settingsSubPath())));

    const StorageLocation location = StorageLocation::resolve(application.path(), user.path());
    QVERIFY(location.isPortable());
    QCOMPARE(normalized(location.filePath(StorageLocation::settingsSubPath())),
             normalized(application.filePath(StorageLocation::settingsSubPath())));
}

void StorageLocationTest::directoryNamedLikeMarkerIsNoMarker()
{
    QTemporaryDir application;
    QTemporaryDir user;
    QVERIFY(application.isValid() && user.isValid());
    QVERIFY(QDir(application.path()).mkdir(StorageLocation::portableMarkerName()));

    QVERIFY(!StorageLocation::resolve(application.path(), user.path()).isPortable());
}

void StorageLocationTest::absoluteSubPathIsKept()
{
    // The settings may name the catalog database by an absolute path.
    QTemporaryDir application;
    QTemporaryDir user;
    QTemporaryDir elsewhere;
    QVERIFY(application.isValid() && user.isValid() && elsewhere.isValid());
    const QString database = elsewhere.filePath(QStringLiteral("catalog.db"));

    const StorageLocation location = StorageLocation::resolve(application.path(), user.path());
    QCOMPARE(normalized(location.filePath(database)), normalized(database));
}

#ifdef Q_OS_WIN
void StorageLocationTest::upstreamLayoutIsRead_data()
{
    QTest::addColumn<bool>("portable");
    QTest::addColumn<QString>("catalogDatabase");

    // Where the last upstream release left the files of each package.
    QTest::newRow("portable package") << true << QStringLiteral("database/thumbnail.sqlite3.db");
    QTest::newRow("installed copy") << false << QStringLiteral("thumbnail.sqlite3.db");
}

void StorageLocationTest::upstreamLayoutIsRead()
{
    QFETCH(bool, portable);
    QFETCH(QString, catalogDatabase);

    QTemporaryDir application;
    QTemporaryDir user;
    QVERIFY(application.isValid() && user.isValid());
    const QDir data(portable ? application.path() : user.path());
    const QString settings = data.filePath(QStringLiteral("quickviewer.ini"));
    const QString progress = data.filePath(QStringLiteral("progress.ini"));
    const QString database = data.filePath(catalogDatabase);
    QVERIFY(writeFile(settings));
    QVERIFY(writeFile(progress));
    QVERIFY(writeFile(database));
    if (portable) {
        QVERIFY(writeFile(application.filePath(StorageLocation::portableMarkerName())));
    }

    const StorageLocation location = StorageLocation::resolve(application.path(), user.path());
    QCOMPARE(location.isPortable(), portable);
    QCOMPARE(normalized(location.filePath(StorageLocation::settingsSubPath())),
             normalized(settings));
    QCOMPARE(normalized(location.filePath(StorageLocation::readProgressSubPath())),
             normalized(progress));
    QCOMPARE(normalized(location.filePath(location.defaultCatalogDatabaseSubPath())),
             normalized(database));
}

void StorageLocationTest::nativeApplicationDirectoryIsApplicationDirPath()
{
    QCOMPARE(normalized(StorageLocation::nativeApplicationDirectory()),
             normalized(QCoreApplication::applicationDirPath()));
}

void StorageLocationTest::nativeUserDataDirectoryIsAppDataLocation()
{
    // QVApplication finds the directory through QStandardPaths, and the
    // startup code before it through Windows; both must name the same one, or
    // an installed copy reads its startup settings from another file.
    const QString expected = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(!expected.isEmpty());
    QCOMPARE(
        normalized(StorageLocation::nativeUserDataDirectory(QCoreApplication::applicationName())),
        normalized(expected));
}
#endif

QTEST_GUILESS_MAIN(StorageLocationTest)

#include "tst_storagelocationtest.moc"
