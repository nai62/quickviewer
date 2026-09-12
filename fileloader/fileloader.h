#ifndef FILELOADER_H
#define FILELOADER_H

#include <QtCore>

/**
 * @brief The FileLoader class
 * Abstract file reading processing.
 * It can be external plugin.
 */
class IFileLoader : public QObject
{
    Q_OBJECT

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

    explicit IFileLoader(QObject *parent)
        : QObject(parent)
    {
    }
    virtual ~IFileLoader() {}

    static bool isImageFile(QString path);
    static bool supportsImageFormat(const QByteArray &format);
    static bool isArchiveFile(QString path);
    static bool isExifJpegImageFile(QString path);
    static bool isExifRawImageFile(QString path);
    static bool isAnimatedImageFile(QString path);
    static void sortFiles(QStringList &filenames);
    static bool caseInsensitiveLessThan(const QString &s1, const QString &s2);

    virtual QString volumePath() = 0;
    virtual QString realVolumePath() = 0;
    virtual bool isArchive() = 0;
    virtual bool isValid() = 0;
    virtual bool hasSubDirectories() = 0;
    virtual QStringList contents() = 0;
    virtual QStringList subArchives() = 0;
    virtual QByteArray getFile(QString filename, QMutex &mutex) = 0;
    virtual quint64 getFileSize(QString filename);
    virtual QDateTime getFileModified(QString filename);
    virtual InflateCacheMode getCacheMode() = 0;

signals:
    void imageLoaded(QString name, QByteArray data);
    void loadFinished();
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
