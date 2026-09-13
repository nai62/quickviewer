#ifndef FILEVOLUME7ZARCHIVE_H
#define FILEVOLUME7ZARCHIVE_H

#include <QtCore>
#include "fileloader.h"

class FileLoader7zArchivePrivate;

class FileLoader7zArchive : public IFileLoader
{
public:
    FileLoader7zArchive(QObject *parent, QString sevenzippath, QString extensionOfFile, bool extractSolidArchiveToTemporaryDir = false);
    ~FileLoader7zArchive();

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

    quint64 getFileSize(QString filename) const override;
    QDateTime getFileModified(QString filename) const override;

    /**
     * @brief getCacheMode
     *
     * Indicates the state when Volume created or has already been decompressed.
     */
    InflateCacheMode getCacheMode() const override { return m_cacheMode; }

    static bool initializeLib();
    static bool isInitialized();
    static void uninitializeLib();
    static QStringList st_supportedArchiveFormats;

protected:
    FileLoader7zArchivePrivate *d;
    QString m_volumepath;
    QString m_extensionOfFile;
    QStringList m_imageFileList;
    QStringList m_subArchiveList;
    bool m_valid;
    InflateCacheMode m_cacheMode;
    ArchiveOpenError m_archiveOpenError;
    bool m_extractSolidArchiveToTemporaryDir;

    void initialize();
    void rejectArchive(ArchiveOpenError error);
};

#endif // FILEVOLUME7ZARCHIVE_H
