#ifndef FILELOADERRARARCHIVE_H
#define FILELOADERRARARCHIVE_H

#include <QObject>
#include <QMap>
#include "fileloader.h"

class RarExtractor;

class FileLoaderRarArchive : public IFileLoader
{
public:
    FileLoaderRarArchive(QObject *parent, QString rarpath);

    ~FileLoaderRarArchive();
    /**
     * @brief isArchive
     * @return return true, if the instance treates an archive file
     */
    bool isArchive() const override { return true; }
    /**
     * @brief isValid
     * @return return true, if the instance can load images
     */
    bool isValid() const override { return m_valid; }
    bool hasSubDirectories() const override { return true; }
    /**
     * @brief volumePath
     * @return the path of the instance
     */
    QString volumePath() const override { return m_volumepath; }
    QString realVolumePath() const override { return volumePath(); }
    /**
     * @brief contents
     * @return all image files without parent path(filename only)
     */
    QStringList contents();
    /**
     * @brief subArchives
     * @return all archive files with in the instance
     */
    QStringList subArchives() const override { return m_subArchiveList; }
    /**
     * @brief getFile get a file specified by filename
     * @param filename
     * @param mutex if the method needs to lock resource, must be use the mutex
     * @return file binary data
     */
    QByteArray getFile(QString filename, QMutex &mutex);

    /**
     * @brief getCacheMode
     *
     * Indicates the state when Volume created or has already been decompressed.
     */
    InflateCacheMode getCacheMode() const override { return InflateNoCached; }

protected:
    QString m_volumepath;
//    QMap<QString, Qt7zFileInfo> m_fileinfomap;
    QStringList m_imageFileList;
    QStringList m_subArchiveList;
    bool m_valid;
    RarExtractor *d;

    void initialize();
};

//class FileLoaderRarPlugin : public QObject, public FileLoaderPluginInterface
//{
//    Q_OBJECT
////    Q_PLUGIN_METADATA(IID "com.quickviewer.FileLoader7zPlugin" FILE "fileloader7zplugin.json")
////    Q_INTERFACES(FileLoaderPluginInterface)
//public:
//    ~FileLoaderRarPlugin() {}
//    IFileLoader* getFileLoader(QString path) { return new FileLoaderRarArchive(this, path); }
//    /**
//     * @brief isSupported
//     * @return true, if the file is supported as the archive
//     */
//    bool isSupported(QString path) { return path.toLower().endsWith(".rar"); }
//};
#endif // FILELOADERRARARCHIVE_H
