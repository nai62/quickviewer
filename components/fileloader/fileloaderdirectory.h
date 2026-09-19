#ifndef FILEVOLUMEDIRECTORY_H
#define FILEVOLUMEDIRECTORY_H

#include <QDir>

#include "fileloader.h"

class FileLoaderDirectory : public IFileLoader
{
public:
    enum class TraversalMode {
        CurrentDirectory,
        Recursive,
    };

    FileLoaderDirectory(QString path,
                        TraversalMode traversalMode = TraversalMode::CurrentDirectory);
    ~FileLoaderDirectory() override = default;

    bool isArchive() const override { return false; }
    bool isValid() const override { return m_valid; }
    bool hasSubDirectories() const override { return m_traversalMode == TraversalMode::Recursive; }
    QString volumePath() const override { return m_volumepath; }
    QString realVolumePath() const override { return m_directory.path(); }
    QStringList contents() override;
    QStringList subArchives() const override { return m_subArchiveList; }
    QByteArray getFile(QString filename) override;
    InflateCacheMode getCacheMode() const override { return InflateNoCached; }

private:
    void initialize();
    void initializeCurrentDirectory();
    void initializeRecursive();
    void collectRecursiveFiles(const QString &path, const QString &subpath);

    QString m_volumepath;
    QDir m_directory;
    QStringList m_imageFileList;
    QStringList m_subArchiveList;
    bool m_valid;
    TraversalMode m_traversalMode;
};

#endif // FILEVOLUMEDIRECTORY_H
