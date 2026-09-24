#include <QBuffer>
#include <QCoreApplication>
#include <QImageReader>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

#include "fileloader7zarchive.h"
#include "fileloaderdirectory.h"
#include "fileloaderrararchive.h"
#include "imageformat.h"
#include "rarextractor.h"

#define DATAPATH SRCDIR "data/"

class FileLoaderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void formatClassifiers();
    void jpegContainerNamesAreOneFormat();
    void imageFormatNamesFollowThePath();
    void zipArchives_data();
    void zipArchives();
    void rarArchives_data();
    void rarArchives();
    void directoryLinks_data();
    void directoryLinks();
    void jpeIsJpegImage();
    void heifExtensionsAreImages();
    void heifPluginDecodesImage();
    void recursiveDirectoryTraversal();
    void singleSubdirectoryIsUnwrapped();
    void directoryLinkCycleStopsUnwrapping();
    void emptyDirectoryContentsAreStable();
    void sevenZipImages();
    void sevenZipSolidModes_data();
    void sevenZipSolidModes();
    void passwordProtectedSevenZip_data();
    void passwordProtectedSevenZip();
    void passwordProtectedZip();
    void zeroByteArchiveEntry();
    void archiveOpenFailures();
};

void FileLoaderTest::initTestCase()
{
    // Archive initialization must not depend on the current working directory.
    QTemporaryDir workingDirectory;
    QVERIFY(workingDirectory.isValid());
    const QString originalDirectory = QDir::currentPath();
    QVERIFY(QDir::setCurrent(workingDirectory.path()));
    const bool initialized = FileLoader7zArchive::initializeLib();
    QVERIFY(QDir::setCurrent(originalDirectory));
    QVERIFY(initialized);
}

void FileLoaderTest::cleanupTestCase()
{
    FileLoader7zArchive::uninitializeLib();
}

void FileLoaderTest::formatClassifiers()
{
    QVERIFY(IFileLoader::isExifJpegImageFile("image.JPG"));
    QVERIFY(IFileLoader::isExifRawImageFile("image.NEF"));
    QVERIFY(IFileLoader::isAnimatedImageFile("image.GIF"));

    QVERIFY(!IFileLoader::isExifJpegImageFile("notajpg"));
    QVERIFY(!IFileLoader::isExifRawImageFile("notanef"));
    QVERIFY(!IFileLoader::isAnimatedImageFile("notagif"));
    QVERIFY(!IFileLoader::isArchiveFile("notazip"));
    QVERIFY(IFileLoader::isArchiveFile("BOOK.CBZ"));
}

void FileLoaderTest::jpegContainerNamesAreOneFormat()
{
    const QStringList suffixes{"jpg", "jpeg", "jpe", "jif", "jfif", "jfi"};
    for (const QString &suffix : suffixes) {
        const QString path = QString("image.%1").arg(suffix);
        QVERIFY(imageFormatFromPath(path) == ImageFormat::Jpeg);
        QVERIFY(imageFormatFromPath(path.toUpper()) == ImageFormat::Jpeg);
        QCOMPARE(imageFormatCanonicalName(imageFormatFromPath(path)), QString("jpeg"));
        // The EXIF gate follows the same classification.
        QVERIFY(IFileLoader::isExifJpegImageFile(path));
        QVERIFY(IFileLoader::isImageFile(path));
    }
    // JPEG is the only format with a plugin preference, and it has two names.
    const QByteArray jpegName = IFileLoader::jpegQtFormatName();
    QVERIFY(jpegName == "jpg" || jpegName == "turbojpeg");
}

void FileLoaderTest::imageFormatNamesFollowThePath()
{
    QCOMPARE(imageFormatNameForPath("book/page.jpg"), QString("jpeg"));
    QCOMPARE(imageFormatNameForPath("book/page.JFIF"), QString("jpeg"));
    QCOMPARE(imageFormatNameForPath("book/page.apng"), QString("png"));
    QCOMPARE(imageFormatNameForPath("book/page.tif"), QString("tiff"));
    // Formats QuickViewer does not model keep the suffix that identified them.
    QCOMPARE(imageFormatNameForPath("book/page.bmp"), QString("bmp"));
    // A path without a suffix has no name of its own.
    QCOMPARE(imageFormatNameForPath("book/page"), QString());
}

void FileLoaderTest::zipArchives_data()
{
    QTest::addColumn<QString>("archiveName");

    QTest::newRow("deflate-mbcs") << QString("deflate-mbcs.zip");
    QTest::newRow("deflate-utf8") << QString("deflate-utf8.zip");
    QTest::newRow("deflate64-mbcs") << QString("deflate64-mbcs.zip");
    QTest::newRow("deflate64-utf8") << QString("deflate64-utf8.zip");
}

void FileLoaderTest::zipArchives()
{
    QFETCH(QString, archiveName);
    const QString archivePath = QString(DATAPATH) + archiveName;
    FileLoader7zArchive archive(archivePath, "zip");

    QVERIFY(archive.isValid());
    QVERIFY(archive.archiveOpenError() == ArchiveOpenError::None);
    QCOMPARE(archive.volumePath(), archivePath);

    const QStringList files = archive.contents();
    QCOMPARE(files.size(), 1);
    const QString path = QDir::fromNativeSeparators(files.first());
#ifdef Q_OS_WIN
    QCOMPARE(path, QString("サンプルフォルダ/test.bmp"));
#else
    // Linux 7-Zip treats legacy ZIP names without a Unicode field as UTF-8.
    // Their original CP932 spelling is only available under Japanese Windows.
    if (archiveName.contains("mbcs")) {
        QVERIFY(path.endsWith("/test.bmp"));
    } else {
        QCOMPARE(path, QString("サンプルフォルダ/test.bmp"));
    }
#endif

    const FileLoadResult result = archive.getFileResult(files.first());
    QVERIFY(result.success);
    QVERIFY(result.error == ArchiveOpenError::None);
    QCOMPARE(result.data.size(), 1080054);
    const QImage image = QImage::fromData(result.data, "bmp");
    QCOMPARE(image.size(), QSize(600, 600));
}

static bool writeEmptyFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.close();
    return true;
}

static void verifyRarArchive(const QString &archivePath, const QString &firstName)
{
    RarExtractor rar(archivePath);
    QVERIFY2(rar.open(RarExtractor::OpenModeList), qPrintable(archivePath));
    QVERIFY(rar.archiveError() == RarArchiveError::None);
    QVERIFY(!rar.isSolid());

    const QStringList files = rar.fileNameList();
    QCOMPARE(files.size(), 6);
    QCOMPARE(QDir::fromNativeSeparators(files.first()), firstName);

    const QList<RARFileInfo> &fileInfoList = rar.fileInfoList();
    QList<int> nonEmptyFiles;
    for (int index = 0; index < fileInfoList.size(); ++index) {
        if (fileInfoList.at(index).unpSize > 0) {
            nonEmptyFiles.append(index);
        }
    }
    QVERIFY(nonEmptyFiles.size() >= 2);

    const int firstIndex = nonEmptyFiles.at(0);
    const int secondIndex = nonEmptyFiles.at(1);
    rar.resetStatistics();
    const RarFileDataResult first = rar.fileDataResult(files.at(firstIndex));
    QVERIFY(first.success);
    QCOMPARE(first.data.size(), 462336);

    const RarFileDataResult second = rar.fileDataResult(files.at(secondIndex));
    QVERIFY(second.success);
    QCOMPARE(second.data.size(), static_cast<qsizetype>(fileInfoList.at(secondIndex).unpSize));
    QCOMPARE(rar.statistics().reopenCount, quint64(0));
    QCOMPARE(rar.statistics().extractCount, quint64(2));

    rar.resetStatistics();
    const RarFileDataResult reread = rar.fileDataResult(files.at(firstIndex));
    QVERIFY(reread.success);
    QCOMPARE(reread.data, first.data);
    QCOMPARE(rar.statistics().reopenCount, quint64(1));
    QCOMPARE(rar.statistics().extractCount, quint64(1));

    FileLoaderRarArchive loader(archivePath);
    QVERIFY(loader.isValid());
    QVERIFY(loader.archiveOpenError() == ArchiveOpenError::None);
}

void FileLoaderTest::rarArchives_data()
{
    QTest::addColumn<QString>("archivePath");
    QTest::addColumn<QString>("firstName");

    QTest::newRow("rar4") << QString(SRCDIR "../../third_party/p7zip/check/test/7za433_rar4.rar")
                          << QString("7za433_rar4/bin/7za.exe");
    QTest::newRow("rar5") << QString(SRCDIR "../../third_party/p7zip/check/test/7za433_rar.rar")
                          << QString("7za433_rar/bin/7za.exe");
}

void FileLoaderTest::rarArchives()
{
    QFETCH(QString, archivePath);
    QFETCH(QString, firstName);
    verifyRarArchive(archivePath, firstName);
}

void FileLoaderTest::directoryLinks_data()
{
    QTest::addColumn<QString>("mklinkOption");
    QTest::newRow("junction") << QString("/J");
    QTest::newRow("directory-symbolic-link") << QString("/D");
}

void FileLoaderTest::directoryLinks()
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

    for (const QString &name : {QString("001.png"), QString("002.png")}) {
        QFile file(root.filePath("target/" + name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
    }

    const QString targetPath = root.filePath("target");
    const QString linkPath = root.filePath("container/link");
    QProcess mklink;
    mklink.start("cmd.exe",
                 {"/c",
                  "mklink",
                  mklinkOption,
                  QDir::toNativeSeparators(linkPath),
                  QDir::toNativeSeparators(targetPath)});
    QVERIFY(mklink.waitForFinished());
    const QByteArray output = mklink.readAllStandardOutput() + mklink.readAllStandardError();
    if (mklink.exitCode() != 0 && mklinkOption == "/D") {
        QSKIP(
            qPrintable(QString("Cannot create a directory symbolic link. Enable Windows Developer "
                               "Mode or run the test with elevated privileges. mklink output: %1")
                           .arg(QString::fromLocal8Bit(output).trimmed())));
    }
    QVERIFY2(mklink.exitCode() == 0, output.constData());

    const QFileInfo imageInfo(QDir(linkPath).filePath("002.png"));
    QVERIFY(imageInfo.exists());
    QCOMPARE(QDir::cleanPath(imageInfo.absolutePath()), QDir::cleanPath(linkPath));
    QCOMPARE(imageInfo.fileName(), QString("002.png"));

    FileLoaderDirectory loader(imageInfo.absolutePath());
    QCOMPARE(loader.volumePath(), imageInfo.absolutePath());
    QCOMPARE(loader.contents(), QStringList({"001.png", "002.png"}));

    QDir logicalDirectory(linkPath);
    QVERIFY(logicalDirectory.cdUp());
    QCOMPARE(QDir::cleanPath(logicalDirectory.absolutePath()),
             QDir::cleanPath(root.filePath("container")));
    QVERIFY(QDir(root.filePath("container")).rmdir("link"));
#endif
}

void FileLoaderTest::jpeIsJpegImage()
{
    QVERIFY(IFileLoader::isImageFile("image.jpe"));
    QVERIFY(IFileLoader::isImageFile("IMAGE.JPE"));
    QVERIFY(IFileLoader::isExifJpegImageFile("image.jpe"));
    QVERIFY(IFileLoader::isExifJpegImageFile("IMAGE.JPE"));
}

void FileLoaderTest::heifExtensionsAreImages()
{
    QVERIFY(IFileLoader::isImageFile("image.heic"));
    QVERIFY(IFileLoader::isImageFile("IMAGE.HEIC"));
    QVERIFY(IFileLoader::isImageFile("image.heif"));
    QVERIFY(IFileLoader::isImageFile("IMAGE.HEIF"));
}

void FileLoaderTest::heifPluginDecodesImage()
{
    QVERIFY2(QImageReader::supportedImageFormats().contains("heic"),
             "The HEIF image format plug-in is unavailable");

    const QByteArray bytes = QByteArray::fromBase64(
        "AAAAHGZ0eXBoZWljAAAAAG1pZjFoZWljbWlhZgAAAXttZXRhAAAAAAAAACFoZGxyAAAAAAAAAABwaWN0AAAAAAAAAA"
        "AAAAAAAAAA"
        "ACJpbG9jAAAAAERAAAEAAQAAAAABnwABAAAAAAAAAGwAAAAjaWluZgAAAAAAAQAAABVpbmZlAgAAAAABAABodmMxAA"
        "AAAA5waXRt"
        "AAAAAAABAAAA+2lwcnAAAADbaXBjbwAAAHZodmNDAQNwAAAAAAAAAAAAHvAA/P34+AAADwNgAAEAGEABDAH//"
        "wNwAAADAJAAAAMA"
        "AAMAHroCQGEAAQAqQgEBA3AAAAMAkAAAAwAAAwAeoCCBBZbq5Ka5uAhoMCAAAAMDIAAAAwAhYgABAAZEAcFzwIkAAA"
        "ATY29scm5j"
        "bHgAAQANAAaAAAAAFGlzcGUAAAAAAAAAQAAAAEAAAAAoY2xhcAAAACAAAAABAAAAIAAAAAH////gAAAAAv///"
        "+AAAAACAAAADnBp"
        "eGkAAAAAAQgAAAAYaXBtYQAAAAAAAAABAAEFgQIDBYQAAAB0bWRhdAAAAGgoAa8TgPUrAhGDczL1mz4HCRRzxqbGjn"
        "nUrr1cLTO7"
        "99zRz6nw0QjRMp+"
        "4I2Da10D3ghQEMvB53CWoI0S3qXIb99YsvLFaQ9ZLHxsJsZ9SxlvNJ5EgD4Y4miuaKu3bxPGXDHirp/9TzA==");
    QBuffer buffer;
    buffer.setData(bytes);
    QVERIFY(buffer.open(QIODevice::ReadOnly));

    QImageReader reader(&buffer, "heic");
    QVERIFY2(reader.canRead(), qPrintable(reader.errorString()));
    const QImage image = reader.read();
    QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));
    QCOMPARE(image.size(), QSize(32, 32));
}

void FileLoaderTest::recursiveDirectoryTraversal()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    QDir root(temporaryDir.path());
    QVERIFY(root.mkpath("chapter/section"));

    const QStringList paths{"003.jpe", "chapter/002.jpe", "chapter/section/001.jpe"};
    for (const QString &path : paths) {
        QFile file(root.filePath(path));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
    }

    FileLoaderDirectory flat(root.path());
    QVERIFY(!flat.hasSubDirectories());
    QCOMPARE(flat.contents(), QStringList({"003.jpe"}));

    FileLoaderDirectory recursive(root.path(), FileLoaderDirectory::TraversalMode::Recursive);
    QVERIFY(recursive.hasSubDirectories());
    QStringList normalized;
    for (const QString &path : recursive.contents()) {
        normalized.append(QDir::fromNativeSeparators(path));
    }
    QCOMPARE(normalized, QStringList({"003.jpe", "chapter/002.jpe", "chapter/section/001.jpe"}));
}

void FileLoaderTest::singleSubdirectoryIsUnwrapped()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    QDir root(temporaryDir.path());

    // A folder holding nothing but one folder is a wrapper around it, so the
    // listing descends into that one - and keeps descending through a chain of
    // wrappers.
    QVERIFY(root.mkpath("wrapper/inner"));
    QVERIFY(writeEmptyFile(root.filePath("wrapper/inner/001.png")));
    QVERIFY(writeEmptyFile(root.filePath("wrapper/inner/002.png")));
    FileLoaderDirectory unwrapped(root.filePath("wrapper"));
    QCOMPARE(unwrapped.contents(), QStringList({"001.png", "002.png"}));
    QCOMPARE(QDir::cleanPath(unwrapped.realVolumePath()),
             QDir::cleanPath(root.filePath("wrapper/inner")));

    QVERIFY(root.mkpath("chain/a/b"));
    QVERIFY(writeEmptyFile(root.filePath("chain/a/b/001.png")));
    FileLoaderDirectory chain(root.filePath("chain"));
    QCOMPARE(chain.contents(), QStringList({"001.png"}));
    QCOMPARE(QDir::cleanPath(chain.realVolumePath()), QDir::cleanPath(root.filePath("chain/a/b")));

    // Any file - an image or not - is a listing of its own, so the wrapper
    // stays where it is.
    QVERIFY(root.mkpath("withfile/sub"));
    QVERIFY(writeEmptyFile(root.filePath("withfile/sub/001.png")));
    QVERIFY(writeEmptyFile(root.filePath("withfile/notes.txt")));
    FileLoaderDirectory withFile(root.filePath("withfile"));
    QCOMPARE(withFile.contents(), QStringList());

    // Two directories are a choice the listing shows instead of making.
    QVERIFY(root.mkpath("siblings/one"));
    QVERIFY(root.mkpath("siblings/two"));
    QVERIFY(writeEmptyFile(root.filePath("siblings/one/001.png")));
    FileLoaderDirectory siblings(root.filePath("siblings"));
    QCOMPARE(siblings.contents(), QStringList());

    // A chain deeper than the limit stops at the limit rather than following
    // wrappers down for as long as the filesystem allows.
    QString deep;
    for (int level = 0; level < FileLoaderDirectory::MaxUnwrappedDepth + 4; ++level) {
        deep += QStringLiteral("/d%1").arg(level);
    }
    QVERIFY(root.mkpath(QStringLiteral("deep") + deep));
    QVERIFY(
        writeEmptyFile(root.filePath(QStringLiteral("deep") + deep + QStringLiteral("/001.png"))));
    FileLoaderDirectory deepLoader(root.filePath(QStringLiteral("deep")));
    QCOMPARE(deepLoader.contents(), QStringList());
}

void FileLoaderTest::directoryLinkCycleStopsUnwrapping()
{
#ifndef Q_OS_WIN
    QSKIP("Windows directory links are specific to Windows");
#else
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    QDir root(temporaryDir.path());
    QVERIFY(root.mkpath("outer"));
    QVERIFY(root.mkpath("target"));
    QVERIFY(writeEmptyFile(root.filePath("target/001.png")));
    QVERIFY(root.mkpath("wrapper"));

    const auto linkDirectory = [&root](const QString &link, const QString &target) {
        QProcess mklink;
        mklink.start("cmd.exe",
                     {"/c",
                      "mklink",
                      "/J",
                      QDir::toNativeSeparators(root.filePath(link)),
                      QDir::toNativeSeparators(root.filePath(target))});
        return mklink.waitForFinished() && mklink.exitCode() == 0;
    };

    // A link back to the listing's own directory: the descent has to stop at
    // the unwrap limit instead of following the link for as long as the
    // filesystem allows.
    QVERIFY2(linkDirectory(QStringLiteral("outer/self"), QStringLiteral("outer")),
             "Cannot create the directory junction");
    FileLoaderDirectory cycle(root.filePath("outer"));
    QCOMPARE(cycle.contents(), QStringList());
    const QString unwrapped = QDir(root.filePath("outer")).relativeFilePath(cycle.realVolumePath());
    QVERIFY2(unwrapped.count(QLatin1Char('/')) + 1 <= FileLoaderDirectory::MaxUnwrappedDepth,
             qPrintable(unwrapped));

    // A link to another directory is not a cycle: it stays part of the listing.
    QVERIFY2(linkDirectory(QStringLiteral("wrapper/link"), QStringLiteral("target")),
             "Cannot create the directory junction");
    FileLoaderDirectory linked(root.filePath("wrapper"));
    QCOMPARE(linked.contents(), QStringList({"001.png"}));
    FileLoaderDirectory recursiveLinked(root.filePath("wrapper"),
                                        FileLoaderDirectory::TraversalMode::Recursive);
    QCOMPARE(recursiveLinked.contents(),
             QStringList({QDir::toNativeSeparators(QStringLiteral("link/001.png"))}));

    QVERIFY(QDir(root.filePath("outer")).rmdir(QStringLiteral("self")));
    QVERIFY(QDir(root.filePath("wrapper")).rmdir(QStringLiteral("link")));
#endif
}

void FileLoaderTest::emptyDirectoryContentsAreStable()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    FileLoaderDirectory loader(temporaryDir.path());
    QVERIFY(loader.isValid());

    QCOMPARE(loader.contents(), QStringList());
    QCOMPARE(loader.contents(), QStringList());
}

void FileLoaderTest::sevenZipImages()
{
    FileLoader7zArchive archive(DATAPATH "7z/image.7z", "7z");
    QVERIFY(archive.isValid());
    QVERIFY(archive.archiveOpenError() == ArchiveOpenError::None);

    const QString unicodeName = QString("[sample＋folder] サンプル！　～フォルダ？～/red.jpg");
    const QStringList files = archive.contents();
    QVERIFY(files.contains("red.jpg"));
    QVERIFY(files.contains("yellow.png"));
    QVERIFY(files.contains(unicodeName));
    QCOMPARE(archive.getFileSize("yellow.png"), quint64(4323));

    const FileLoadResult jpeg = archive.getFileResult("red.jpg");
    QVERIFY(jpeg.success);
    const QImage jpegImage = QImage::fromData(jpeg.data);
    QCOMPARE(jpegImage.size(), QSize(225, 225));

    const FileLoadResult png = archive.getFileResult("yellow.png");
    QVERIFY(png.success);
    const QImage pngImage = QImage::fromData(png.data);
    QCOMPARE(pngImage.size(), QSize(800, 800));

    const FileLoadResult unicode = archive.getFileResult(unicodeName);
    QVERIFY(unicode.success);
    QCOMPARE(QImage::fromData(unicode.data).size(), QSize(225, 225));
}

void FileLoaderTest::sevenZipSolidModes_data()
{
    QTest::addColumn<bool>("extractToTemporaryDirectory");
    QTest::newRow("direct") << false;
    QTest::newRow("temporary-directory") << true;
}

void FileLoaderTest::sevenZipSolidModes()
{
    QFETCH(bool, extractToTemporaryDirectory);
    FileLoader7zArchive archive(DATAPATH "7z/image.7z", "7z", extractToTemporaryDirectory);
    QVERIFY(archive.isValid());
    if (extractToTemporaryDirectory) {
        QCOMPARE(archive.getCacheMode(), IFileLoader::InflateCached);
    }

    const FileLoadResult result = archive.getFileResult("yellow.png");
    QVERIFY(result.success);
    QVERIFY(result.error == ArchiveOpenError::None);
    QCOMPARE(QImage::fromData(result.data).size(), QSize(800, 800));
}

void FileLoaderTest::passwordProtectedSevenZip_data()
{
    QTest::addColumn<QString>("archiveName");
    QTest::newRow("file-encrypted") << QString("password.7z");
    QTest::newRow("header-encrypted") << QString("password-filename.7z");
}

void FileLoaderTest::passwordProtectedSevenZip()
{
    QFETCH(QString, archiveName);
    FileLoader7zArchive archive(QString(DATAPATH "7z/") + archiveName, "7z", true);
    QVERIFY(!archive.isValid());
    QVERIFY(archive.archiveOpenError() == ArchiveOpenError::PasswordProtected);
    QVERIFY(archive.contents().isEmpty());
    QCOMPARE(archive.getCacheMode(), IFileLoader::InflateNoCached);
}

void FileLoaderTest::passwordProtectedZip()
{
    FileLoader7zArchive archive(DATAPATH "zip/encrypted.zip", "zip");
    QVERIFY(!archive.isValid());
    QVERIFY(archive.archiveOpenError() == ArchiveOpenError::PasswordProtected);
    QVERIFY(archive.contents().isEmpty());
}

void FileLoaderTest::zeroByteArchiveEntry()
{
    FileLoader7zArchive archive(DATAPATH "zip/zero-byte.zip", "zip");
    QVERIFY(archive.isValid());
    QCOMPARE(archive.contents(), QStringList({"empty.png"}));
    QCOMPARE(archive.getFileSize("empty.png"), quint64(0));

    const FileLoadResult result = archive.getFileResult("empty.png");
    QVERIFY(result.success);
    QVERIFY(result.error == ArchiveOpenError::None);
    QVERIFY(result.data.isEmpty());
}

void FileLoaderTest::archiveOpenFailures()
{
    FileLoader7zArchive missing(DATAPATH "does-not-exist.7z", "7z");
    QVERIFY(!missing.isValid());
    QVERIFY(missing.archiveOpenError() == ArchiveOpenError::IoError);

    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString corruptPath = temporaryDir.filePath("corrupt.7z");
    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QCOMPARE(corrupt.write("not an archive"), qint64(14));
    corrupt.close();

    FileLoader7zArchive corruptArchive(corruptPath, "7z");
    QVERIFY(!corruptArchive.isValid());
    QVERIFY(corruptArchive.archiveOpenError() != ArchiveOpenError::None);
    QVERIFY(corruptArchive.archiveOpenError() != ArchiveOpenError::PasswordProtected);

    FileLoader7zArchive unsupported(corruptPath, "definitely-unsupported");
    QVERIFY(!unsupported.isValid());
    QVERIFY(unsupported.archiveOpenError() == ArchiveOpenError::Unsupported);
}

QTEST_MAIN(FileLoaderTest)

#include "tst_fileloadertest.moc"
