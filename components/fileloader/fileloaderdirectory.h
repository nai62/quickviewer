#ifndef FILEVOLUMEDIRECTORY_H
#define FILEVOLUMEDIRECTORY_H

#include <QDir>

#include "fileloader.h"

class FileLoaderDirectory : public IFileLoader
{
public:
    /**
     * How many single-directory wrappers a listing unwraps before it stops.
     * Deep enough for the wrapper folders this is for, and a bound no link
     * cycle can grow past.
     */
    static constexpr int MaxUnwrappedDepth = 8;

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
    /** Each read opens its own file, so reads run side by side. */
    bool supportsConcurrentReads() const override { return true; }
    InflateCacheMode getCacheMode() const override { return InflateNoCached; }

private:
    void initialize();
    void initializeCurrentDirectory();
    void initializeRecursive();
    void collectRecursiveFiles(const QString &path, const QString &subpath);

    QString m_volumepath;
    QDir m_directory;
    /**
     * The directory the pages are read from, kept as a string: the reads run
     * on several threads, and a QDir caches what it resolves.
     */
    QString m_readDirectory;
    QStringList m_imageFileList;
    QStringList m_subArchiveList;
    bool m_valid;
    TraversalMode m_traversalMode;
};

#endif // FILEVOLUMEDIRECTORY_H
