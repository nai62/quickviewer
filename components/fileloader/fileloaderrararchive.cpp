#include "fileloaderrararchive.h"
#include "rarextractor.h"

static ArchiveOpenError mapRarError(RarArchiveError error)
{
    switch (error) {
    case RarArchiveError::None:
        return ArchiveOpenError::None;
    case RarArchiveError::PasswordProtected:
        return ArchiveOpenError::PasswordProtected;
    case RarArchiveError::Unsupported:
        return ArchiveOpenError::Unsupported;
    case RarArchiveError::Corrupt:
        return ArchiveOpenError::Corrupt;
    case RarArchiveError::IoError:
        return ArchiveOpenError::IoError;
    }
    return ArchiveOpenError::Corrupt;
}

FileLoaderRarArchive::FileLoaderRarArchive(QObject *parent, QString rarpath)
    : IFileLoader(parent),
      m_volumepath(std::move(rarpath)),
      m_valid(false),
      m_archiveOpenError(ArchiveOpenError::None),
      d(new RarExtractor(m_volumepath))
{
    if (!d->open(RarExtractor::OpenModeList)) {
        rejectArchive(mapRarError(d->archiveError()));
        return;
    }
    initialize();
}

FileLoaderRarArchive::~FileLoaderRarArchive()
{
    delete d;
    d = nullptr;
}

void FileLoaderRarArchive::rejectArchive(ArchiveOpenError error)
{
    m_archiveOpenError = error;
    m_valid = false;
    m_imageFileList.clear();
    m_subArchiveList.clear();
}

QStringList FileLoaderRarArchive::contents()
{
    return m_imageFileList;
}

void FileLoaderRarArchive::initialize()
{
    if (!d || d->archiveError() != RarArchiveError::None) {
        rejectArchive(d ? mapRarError(d->archiveError()) : ArchiveOpenError::Corrupt);
        return;
    }

    m_imageFileList.clear();
    m_subArchiveList.clear();
    for (const QString &name : d->fileNameList()) {
        const QString filename = QDir::toNativeSeparators(name);
        if (IFileLoader::isImageFile(filename)) {
            m_imageFileList.append(filename);
        } else if (IFileLoader::isArchiveFile(filename)) {
            m_subArchiveList.append(filename);
        }
    }
    IFileLoader::sortFiles(m_imageFileList);
    IFileLoader::sortFiles(m_subArchiveList);
    m_archiveOpenError = ArchiveOpenError::None;
    m_valid = true;
}

FileLoadResult FileLoaderRarArchive::getFileResult(QString name, QMutex &mutex)
{
    QMutexLocker locker(&mutex);
    if (m_archiveOpenError != ArchiveOpenError::None || !m_valid) {
        return {{}, m_archiveOpenError, false};
    }
    if (!m_imageFileList.contains(name)) {
        return {{}, ArchiveOpenError::Unsupported, false};
    }

    const RarFileDataResult result = d->fileDataResult(name);
    if (!result.success) {
        const ArchiveOpenError error = mapRarError(result.error);
        rejectArchive(error);
        return {result.data, error, false};
    }
    return {result.data, ArchiveOpenError::None, true};
}

QByteArray FileLoaderRarArchive::getFile(QString name, QMutex &mutex)
{
    return getFileResult(std::move(name), mutex).data;
}
