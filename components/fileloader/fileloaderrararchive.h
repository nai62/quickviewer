#ifndef FILELOADERRARARCHIVE_H
#define FILELOADERRARARCHIVE_H

#include <QObject>
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
    QStringList contents() override;
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
    QByteArray getFile(QString filename, QMutex &mutex) override;
    FileLoadResult getFileResult(QString filename, QMutex &mutex) override;
    ArchiveOpenError archiveOpenError() const override { return m_archiveOpenError; }

    /**
     * @brief getCacheMode
     *
     * Indicates the state when Volume created or has already been decompressed.
     */
    InflateCacheMode getCacheMode() const override { return InflateNoCached; }

protected:
    QString m_volumepath;
    QStringList m_imageFileList;
    QStringList m_subArchiveList;
    bool m_valid;
    ArchiveOpenError m_archiveOpenError;
    RarExtractor *d;

    void initialize();
    void rejectArchive(ArchiveOpenError error);
};

#endif // FILELOADERRARARCHIVE_H
