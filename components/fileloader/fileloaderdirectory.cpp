#include "fileloaderdirectory.h"

FileLoaderDirectory::FileLoaderDirectory(QObject *parent, QString path, TraversalMode traversalMode)
    : IFileLoader(parent),
      m_volumepath(path),
      m_valid(false),
      m_traversalMode(traversalMode)
{
    m_directory.setPath(path);
    if (!(m_valid = m_directory.exists())) {
        return;
    }
    initialize();
}

QStringList FileLoaderDirectory::contents()
{
    return m_imageFileList;
}

void FileLoaderDirectory::initialize()
{
    m_imageFileList.clear();
    m_subArchiveList.clear();

    if (m_traversalMode == TraversalMode::Recursive) {
        initializeRecursive();
    } else {
        initializeCurrentDirectory();
    }

    m_valid = true;
    emit loadFinished();
}

void FileLoaderDirectory::initializeCurrentDirectory()
{
    QStringList files;
    do {
        files = m_directory.entryList(QDir::Files, QDir::Name);
        m_subArchiveList = m_directory.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        if (!files.isEmpty() || m_subArchiveList.isEmpty()) {
            break;
        }
        if (m_subArchiveList.size() > 1) {
            return;
        }
        m_directory.setPath(m_directory.absoluteFilePath(m_subArchiveList.constFirst()));
    } while (true);

    for (const QString &name : files) {
        if (IFileLoader::isImageFile(name)) {
            m_imageFileList.append(name);
        } else if (IFileLoader::isArchiveFile(name)) {
            m_subArchiveList.append(name);
        }
    }
    IFileLoader::sortFiles(m_imageFileList);
    IFileLoader::sortFiles(m_subArchiveList);
}

void FileLoaderDirectory::initializeRecursive()
{
    collectRecursiveFiles(m_directory.path(), QString());
}

void FileLoaderDirectory::collectRecursiveFiles(const QString &path, const QString &subpath)
{
    const QDir relativeDirectory(subpath);
    const QDir directory(path);

    QStringList images;
    const QStringList files = directory.entryList(QDir::Files, QDir::Name);
    for (const QString &name : files) {
        if (IFileLoader::isImageFile(name)) {
            const QString relativePath = subpath.isEmpty() ? name : relativeDirectory.filePath(name);
            images.append(QDir::toNativeSeparators(relativePath));
        }
    }

    QStringList subdirectories = directory.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    IFileLoader::sortFiles(images);
    IFileLoader::sortFiles(subdirectories);
    m_imageFileList.append(images);

    for (const QString &subdirectory : subdirectories) {
        const QString childSubpath =
            subpath.isEmpty() ? subdirectory : relativeDirectory.filePath(subdirectory);
        collectRecursiveFiles(directory.filePath(subdirectory), childSubpath);
    }
}

QByteArray FileLoaderDirectory::getFile(QString name, QMutex &mutex)
{
    Q_UNUSED(mutex);

    QFile file(m_directory.absoluteFilePath(name));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}
