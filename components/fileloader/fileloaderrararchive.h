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
