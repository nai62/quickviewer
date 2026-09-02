#include <QString>
#include <QtTest>
#include <QCoreApplication>
#include <QBuffer>
#include <QImageReader>
#include "fileloader7zarchive.h"
#include "fileloaderdirectory.h"
#include "rarextractor.h"

#define DATAPATH SRCDIR "data/"

/**
 * @brief The FileLoaderTest class
 *
 * zip loading check with FileLoader7zArchive.
 *
 * there are 4 test archive with 2 options
 * File name character encoding:
 *      mbcs:      no encoding, Depends on the system
 *      utf8:      encoding by UTF-8
 * File content codecs:
 *      deflate:   compression algorithm of regular zip archive
 *      deflate64: new algorithm added to recent zip archive
 */
class FileLoaderTest : public QObject
{
    Q_OBJECT

public:
    FileLoaderTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testCase1_deflate_mbcs();
    void testCase2_deflate_utf8();
    void testCase3_deflate64_mbcs();
    void testCase4_deflate64_utf8();
    void testCase5_rar4();
    void testCase6_rar5();
    void testCase7_directoryLinks_data();
    void testCase7_directoryLinks();
    void testCase8_jpeIsJpegImage();
    void testCase9_heifExtensionsAreImages();
    void testCase10_heifPluginDecodesImage();
};

FileLoaderTest::FileLoaderTest()
{
}

void FileLoaderTest::initTestCase()
{
    QVERIFY(FileLoader7zArchive::initializeLib());
}

void FileLoaderTest::cleanupTestCase()
{
    FileLoader7zArchive::uninitializeLib();
}

void FileLoaderTest::testCase1_deflate_mbcs()
{
    FileLoader7zArchive seven(nullptr, DATAPATH "deflate-mbcs.zip", "zip");
    QCOMPARE(seven.volumePath(), QString(DATAPATH "deflate-mbcs.zip"));

    // check filelist
    QStringList files = seven.contents();
    QCOMPARE(files.length(), 1);
    QCOMPARE(QDir::fromNativeSeparators(files[0]), QString("サンプルフォルダ/test.bmp"));

    QMutex mutex;
    QByteArray bytes = seven.getFile(files[0], mutex);
    QCOMPARE(bytes.length(), 1080054);
    QImage image = QImage::fromData(bytes, "bmp");
    QCOMPARE(image.width(), 600);
}

void FileLoaderTest::testCase2_deflate_utf8()
{
    FileLoader7zArchive seven(nullptr, DATAPATH "deflate-utf8.zip", "zip");
    QCOMPARE(seven.volumePath(), QString(DATAPATH "deflate-utf8.zip"));

    // check filelist
    QStringList files = seven.contents();
    QCOMPARE(files.length(), 1);
    QCOMPARE(QDir::fromNativeSeparators(files[0]), QString("サンプルフォルダ/test.bmp"));

    QMutex mutex;
    QByteArray bytes = seven.getFile(files[0], mutex);
    QCOMPARE(bytes.length(), 1080054);
    QImage image = QImage::fromData(bytes, "bmp");
    QCOMPARE(image.width(), 600);
}

void FileLoaderTest::testCase3_deflate64_mbcs()
{
    FileLoader7zArchive seven(nullptr, DATAPATH "deflate64-mbcs.zip", "zip");
    QCOMPARE(seven.volumePath(), QString(DATAPATH "deflate64-mbcs.zip"));

    // check filelist
    QStringList files = seven.contents();
    QCOMPARE(files.length(), 1);
    QCOMPARE(QDir::fromNativeSeparators(files[0]), QString("サンプルフォルダ/test.bmp"));

    QMutex mutex;
    QByteArray bytes = seven.getFile(files[0], mutex);
    QCOMPARE(bytes.length(), 1080054);
    QImage image = QImage::fromData(bytes, "bmp");
    QCOMPARE(image.width(), 600);
}

void FileLoaderTest::testCase4_deflate64_utf8()
{
    FileLoader7zArchive seven(nullptr, DATAPATH "deflate64-utf8.zip", "zip");
    QCOMPARE(seven.volumePath(), QString(DATAPATH "deflate64-utf8.zip"));

    // check filelist
    QStringList files = seven.contents();
    QCOMPARE(files.length(), 1);
    QCOMPARE(QDir::fromNativeSeparators(files[0]), QString("サンプルフォルダ/test.bmp"));

    QMutex mutex;
    QByteArray bytes = seven.getFile(files[0], mutex);
    QCOMPARE(bytes.length(), 1080054);
    QImage image = QImage::fromData(bytes, "bmp");
    QCOMPARE(image.width(), 600);
}

static void verifyRarArchive(const QString &archivePath, const QString &firstName)
{
    RarExtractor rar(archivePath);
    QVERIFY2(rar.open(RarExtractor::OpenModeList, ""), qPrintable(archivePath));

    const QStringList files = rar.fileNameList();
    QCOMPARE(files.length(), 6);
    QCOMPARE(QDir::fromNativeSeparators(files.first()), firstName);

    QList<int> nonEmptyFiles;
    for (int index = 0; index < rar.m_fileInfoList.size(); ++index) {
        if (rar.m_fileInfoList.at(index).unpSize > 0) {
            nonEmptyFiles.append(index);
        }
    }
    QVERIFY(nonEmptyFiles.size() >= 2);

    const int firstIndex = nonEmptyFiles.at(0);
    const int secondIndex = nonEmptyFiles.at(1);
    const QByteArray firstBytes = rar.fileData(files.at(firstIndex));
    QCOMPARE(firstBytes.length(), 462336);
    QCOMPARE(rar.m_curIndex, firstIndex + 1);

    const QByteArray secondBytes = rar.fileData(files.at(secondIndex));
    QCOMPARE(secondBytes.size(), static_cast<qsizetype>(rar.m_fileInfoList.at(secondIndex).unpSize));
    QCOMPARE(rar.m_curIndex, secondIndex + 1);

    // A cached backward read must not reset the sequential archive cursor.
    const int cursorAfterSecondFile = rar.m_curIndex;
    QCOMPARE(rar.fileData(files.at(firstIndex)), firstBytes);
    QCOMPARE(rar.m_curIndex, cursorAfterSecondFile);
}

void FileLoaderTest::testCase5_rar4()
{
    verifyRarArchive(QString(SRCDIR "../Qt7z/Qt7z/p7zip/check/test/7za433_rar4.rar"),
                     QString("7za433_rar4/bin/7za.exe"));
}

void FileLoaderTest::testCase6_rar5()
{
    verifyRarArchive(QString(SRCDIR "../Qt7z/Qt7z/p7zip/check/test/7za433_rar.rar"),
                     QString("7za433_rar/bin/7za.exe"));
}

void FileLoaderTest::testCase7_directoryLinks_data()
{
    QTest::addColumn<QString>("mklinkOption");

    QTest::newRow("junction") << QString("/J");
    QTest::newRow("directory-symbolic-link") << QString("/D");
}

void FileLoaderTest::testCase7_directoryLinks()
{
#ifndef Q_OS_WIN
    QSKIP("Windows directory links are specific to Windows");
#else
    QFETCH(QString, mklinkOption);

    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    QDir root(temporaryDir.path());
    QVERIFY(root.mkpath("target"));
    QVERIFY(root.mkpath("container"));

    QFile first(root.filePath("target/001.png"));
    QVERIFY(first.open(QIODevice::WriteOnly));
    first.close();
    QFile second(root.filePath("target/002.png"));
    QVERIFY(second.open(QIODevice::WriteOnly));
    second.close();

    const QString targetPath = root.filePath("target");
    const QString linkPath = root.filePath("container/link");
    QProcess mklink;
    mklink.start("cmd.exe", {"/c", "mklink", mklinkOption, QDir::toNativeSeparators(linkPath), QDir::toNativeSeparators(targetPath)});
    QVERIFY(mklink.waitForFinished());
    const QByteArray output = mklink.readAllStandardOutput() + mklink.readAllStandardError();
    if (mklink.exitCode() != 0 && mklinkOption == "/D") {
        QSKIP(qPrintable(QString("Cannot create a directory symbolic link. "
                                 "Enable Windows Developer Mode or run the test "
                                 "with elevated privileges. mklink output: %1")
                             .arg(QString::fromLocal8Bit(output).trimmed())));
    }
    QVERIFY2(mklink.exitCode() == 0, output.constData());

    const QFileInfo imageInfo(QDir(linkPath).filePath("002.png"));
    QVERIFY(imageInfo.exists());
    QCOMPARE(QDir::cleanPath(imageInfo.absolutePath()), QDir::cleanPath(linkPath));
    QCOMPARE(imageInfo.fileName(), QString("002.png"));

    FileLoaderDirectory loader(nullptr, imageInfo.absolutePath());
    QCOMPARE(loader.volumePath(), imageInfo.absolutePath());
    QCOMPARE(loader.contents(), QStringList({"001.png", "002.png"}));

    QDir logicalDirectory(linkPath);
    QVERIFY(logicalDirectory.cdUp());
    QCOMPARE(QDir::cleanPath(logicalDirectory.absolutePath()),
             QDir::cleanPath(root.filePath("container")));

    // Remove only the reparse point before QTemporaryDir recursively removes
    // the target directory.
    QVERIFY(QDir(root.filePath("container")).rmdir("link"));
#endif
}

void FileLoaderTest::testCase8_jpeIsJpegImage()
{
    QVERIFY(IFileLoader::isImageFile("image.jpe"));
    QVERIFY(IFileLoader::isImageFile("IMAGE.JPE"));
    QVERIFY(IFileLoader::isExifJpegImageFile("image.jpe"));
    QVERIFY(IFileLoader::isExifJpegImageFile("IMAGE.JPE"));
}

void FileLoaderTest::testCase9_heifExtensionsAreImages()
{
    QVERIFY(IFileLoader::isImageFile("image.heic"));
    QVERIFY(IFileLoader::isImageFile("IMAGE.HEIC"));
    QVERIFY(IFileLoader::isImageFile("image.heif"));
    QVERIFY(IFileLoader::isImageFile("IMAGE.HEIF"));
}

void FileLoaderTest::testCase10_heifPluginDecodesImage()
{
    if (!QImageReader::supportedImageFormats().contains("heic")) {
        QSKIP("The HEIF image format plug-in is not staged in this test environment");
    }

    const QByteArray bytes = QByteArray::fromBase64(
        "AAAAHGZ0eXBoZWljAAAAAG1pZjFoZWljbWlhZgAAAXttZXRhAAAAAAAAACFoZGxyAAAAAAAAAABwaWN0AAAAAAAAAAAAAAAAAAAA"
        "ACJpbG9jAAAAAERAAAEAAQAAAAABnwABAAAAAAAAAGwAAAAjaWluZgAAAAAAAQAAABVpbmZlAgAAAAABAABodmMxAAAAAA5waXRt"
        "AAAAAAABAAAA+2lwcnAAAADbaXBjbwAAAHZodmNDAQNwAAAAAAAAAAAAHvAA/P34+AAADwNgAAEAGEABDAH//wNwAAADAJAAAAMA"
        "AAMAHroCQGEAAQAqQgEBA3AAAAMAkAAAAwAAAwAeoCCBBZbq5Ka5uAhoMCAAAAMDIAAAAwAhYgABAAZEAcFzwIkAAAATY29scm5j"
        "bHgAAQANAAaAAAAAFGlzcGUAAAAAAAAAQAAAAEAAAAAoY2xhcAAAACAAAAABAAAAIAAAAAH////gAAAAAv///+AAAAACAAAADnBp"
        "eGkAAAAAAQgAAAAYaXBtYQAAAAAAAAABAAEFgQIDBYQAAAB0bWRhdAAAAGgoAa8TgPUrAhGDczL1mz4HCRRzxqbGjnnUrr1cLTO7"
        "99zRz6nw0QjRMp+4I2Da10D3ghQEMvB53CWoI0S3qXIb99YsvLFaQ9ZLHxsJsZ9SxlvNJ5EgD4Y4miuaKu3bxPGXDHirp/9TzA==");
    QBuffer buffer;
    buffer.setData(bytes);
    QVERIFY(buffer.open(QIODevice::ReadOnly));

    QImageReader reader(&buffer, "heic");
    QVERIFY2(reader.canRead(), qPrintable(reader.errorString()));
    const QImage image = reader.read();
    QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));
    QCOMPARE(image.size(), QSize(32, 32));
}

QTEST_MAIN(FileLoaderTest)

#include "tst_fileloadertest.moc"
