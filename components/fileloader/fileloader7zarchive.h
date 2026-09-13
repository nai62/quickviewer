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

    bool isArchive() const override { return true; }
    bool isValid() const override { return m_valid; }
    bool hasSubDirectories() const override { return true; }
    QString volumePath() const override { return m_volumepath; }
    QString realVolumePath() const override { return volumePath(); }
    QStringList contents() override;
    QStringList subArchives() const override { return m_subArchiveList; }
    QByteArray getFile(QString filename, QMutex &mutex) override;
    FileLoadResult getFileResult(QString filename, QMutex &mutex) override;
    ArchiveOpenError archiveOpenError() const override { return m_archiveOpenError; }

    quint64 getFileSize(QString filename) const override;
    QDateTime getFileModified(QString filename) const override;
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
