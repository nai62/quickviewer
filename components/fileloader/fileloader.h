#ifndef FILELOADER_H
#define FILELOADER_H

#include <QtCore>

enum class ArchiveOpenError {
    None,
    PasswordProtected,
    Unsupported,
    Corrupt,
    IoError,
};
Q_DECLARE_METATYPE(ArchiveOpenError)

struct FileLoadResult
{
    QByteArray data;
    ArchiveOpenError error = ArchiveOpenError::None;
    bool success = false;
};

/**
 * @brief The FileLoader class
 * Abstract file reading processing.
 * It can be external plugin.
 */
class IFileLoader
{
public:
    enum ScanMode {
        Normal,
        ToFastImage, // header scan, and return an image, and end(without worker)
        ToAllFilesExtract, // header scan, and read all archived file under worker thread
    };

    enum InflateCacheMode {
        InflateNoCached,
        InflateCaching,
        InflateCached
    };

    virtual ~IFileLoader() = default;

    static bool isImageFile(QString path);
    static bool supportsImageFormat(const QByteArray &format);
    /**
     * Name QImageReader reports for the TurboJPEG JPEG decoder plugin.
     */
    static constexpr const char *turboJpegFormatName() { return "turbojpeg"; }
    /**
     * Qt format name to read JPEG bytes with: the TurboJPEG plugin when it is
     * registered, the plain JPEG handler otherwise.
     */
    static QByteArray jpegQtFormatName();
    static bool isArchiveFile(QString path);
    static bool isExifJpegImageFile(QString path);
    static bool isExifRawImageFile(QString path);
    static bool isAnimatedImageFile(QString path);
    static void sortFiles(QStringList &filenames);
    static bool caseInsensitiveLessThan(const QString &s1, const QString &s2);

    virtual QString volumePath() const = 0;
    virtual QString realVolumePath() const = 0;
    virtual bool isArchive() const = 0;
    virtual bool isValid() const = 0;
    virtual bool hasSubDirectories() const = 0;
    virtual QStringList contents() = 0;
    virtual QStringList subArchives() const = 0;
    virtual QByteArray getFile(QString filename) = 0;
    virtual FileLoadResult getFileResult(QString filename)
    {
        return {getFile(filename), ArchiveOpenError::None, true};
    }
    virtual ArchiveOpenError archiveOpenError() const { return ArchiveOpenError::None; }
    virtual quint64 getFileSize(QString filename) const;
    virtual QDateTime getFileModified(QString filename) const;
    virtual InflateCacheMode getCacheMode() const = 0;
};

class FileLoaderPluginInterface
{
public:
    virtual ~FileLoaderPluginInterface() {}
    virtual IFileLoader *getFileLoader(QString path) = 0;
    virtual bool isSupported(QString path) = 0;
};

// Q_DECLARE_INTERFACE(FileLoaderPluginInterface,
//                     "com.quickviewer.FileLoaderPlugin/1.0")

#endif // FILELOADER_H
