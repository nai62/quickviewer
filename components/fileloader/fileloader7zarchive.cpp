#ifdef _WIN32
#    include <windows.h>
#endif

#include <QtCore>
#include <memory>

#include "fileloader7zarchive.h"
#include <lib7zip.h>

using namespace lib7zip;

static C7ZipLibrary *c7zipLib = nullptr;
static bool c7ziplibAlive = false;
QStringList FileLoader7zArchive::st_supportedArchiveFormats;

struct Qt7zFileInfo
{
    QString fileName;
    QString arcName;
    quint64 size = 0;
    QDateTime modified;
    bool isDir = false;
    bool isEncrypted = false;
};

static ArchiveOpenError mapLib7zipError(ErrorCodeEnum error)
{
    switch (error) {
    case LIB7ZIP_NEED_PASSWORD:
        return ArchiveOpenError::PasswordProtected;
    case LIB7ZIP_NOT_SUPPORTED_ARCHIVE:
    case LIB7ZIP_NOT_INITIALIZE:
        return ArchiveOpenError::Unsupported;
    case LIB7ZIP_NO_ERROR:
        return ArchiveOpenError::None;
    case LIB7ZIP_UNKNOWN_ERROR:
    default:
        return ArchiveOpenError::Corrupt;
    }
}

class Qt7zStreamReader : public C7ZipInStream
{
public:
    Qt7zStreamReader(QIODevice *stream, const QString &extension)
        : m_stream(stream),
          m_extension(extension.toStdWString())
    {}

    wstring GetExt() const override { return m_extension; }

    int Read(void *data, unsigned int size, unsigned int *processedSize) override
    {
        if (!m_stream || !m_stream->isOpen()) {
            return 1;
        }
        const qint64 readBytes = m_stream->read(static_cast<char *>(data), size);
        if (readBytes < 0) {
            return 1;
        }
        if (processedSize) {
            *processedSize = static_cast<unsigned int>(readBytes);
        }
        return 0;
    }

    int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition) override
    {
        if (!m_stream || !m_stream->isOpen()) {
            return 1;
        }

        qint64 target = 0;
        switch (seekOrigin) {
        case SEEK_SET:
            target = offset;
            break;
        case SEEK_CUR:
            target = m_stream->pos() + offset;
            break;
        case SEEK_END:
            target = m_stream->size() + offset;
            break;
        default:
            return 1;
        }
        if (!m_stream->seek(target)) {
            return 1;
        }
        if (newPosition) {
            *newPosition = static_cast<unsigned __int64>(m_stream->pos());
        }
        return 0;
    }

    int GetSize(unsigned __int64 *size) override
    {
        if (!m_stream || !size) {
            return 1;
        }
        *size = static_cast<unsigned __int64>(m_stream->size());
        return 0;
    }
private:
    QIODevice *m_stream;
    wstring m_extension;
};

class Qt7zStreamWriter : public C7ZipOutStream
{
public:
    explicit Qt7zStreamWriter(QIODevice *stream)
        : m_stream(stream)
    {}

    int Write(const void *data, unsigned int size, unsigned int *processedSize) override
    {
        if (!m_stream || !m_stream->isWritable()) {
            return 1;
        }
        const qint64 written = m_stream->write(static_cast<const char *>(data), size);
        if (written < 0 || static_cast<unsigned int>(written) != size) {
            return 1;
        }
        if (processedSize) {
            *processedSize = static_cast<unsigned int>(written);
        }
        return 0;
    }

    int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition) override
    {
        if (!m_stream || !m_stream->isWritable()) {
            return 1;
        }
        qint64 target = 0;
        switch (seekOrigin) {
        case SEEK_SET:
            target = offset;
            break;
        case SEEK_CUR:
            target = m_stream->pos() + offset;
            break;
        case SEEK_END:
            target = m_stream->size() + offset;
            break;
        default:
            return 1;
        }
        if (!m_stream->seek(target)) {
            return 1;
        }
        if (newPosition) {
            *newPosition = static_cast<unsigned __int64>(m_stream->pos());
        }
        return 0;
    }

    int SetSize(unsigned __int64 size) override
    {
        QFile *file = qobject_cast<QFile *>(m_stream);
        if (file && !file->resize(static_cast<qint64>(size))) {
            return 1;
        }
        return 0;
    }

private:
    QIODevice *m_stream;
};

static Qt7zFileInfo fileInfoFromItem(C7ZipArchiveItem &item, const QString &archivePath)
{
    Qt7zFileInfo info;
    info.arcName = archivePath;

    std::wstring path;
    item.GetStringProperty(PropertyIndexEnum::kpidPath, path);
    info.fileName = QDir::fromNativeSeparators(QString::fromStdWString(path));

    unsigned __int64 value = 0;
    if (item.GetUInt64Property(PropertyIndexEnum::kpidSize, value)) {
        info.size = static_cast<quint64>(value);
    }
    if (item.GetFileTimeProperty(PropertyIndexEnum::kpidMTime, value)) {
        info.modified = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(value));
    }
    bool encryptedProperty = false;
    item.GetBoolProperty(PropertyIndexEnum::kpidEncrypted, encryptedProperty);
    info.isEncrypted = item.IsEncrypted() || encryptedProperty;
    item.GetBoolProperty(PropertyIndexEnum::kpidIsDir, info.isDir);
    return info;
}

class FileLoader7zArchivePrivate
{
public:
    FileLoader7zArchivePrivate(const QString &archivePath, const QString &extension)
        : m_packagePath(archivePath),
          m_pArchive(nullptr),
          m_archiveFile(archivePath),
          m_stream(&m_archiveFile, extension)
    {
        if (!c7zipLib || !c7ziplibAlive) {
            m_error = ArchiveOpenError::Unsupported;
            return;
        }
        if (!m_archiveFile.open(QIODevice::ReadOnly)) {
            m_error = ArchiveOpenError::IoError;
            return;
        }
        if (!c7zipLib->OpenArchive(&m_stream, &m_pArchive)) {
            m_error = mapLib7zipError(c7zipLib->GetLastError());
            if (m_error == ArchiveOpenError::None) {
                m_error = ArchiveOpenError::Corrupt;
            }
            m_pArchive = nullptr;
            return;
        }

        bool archiveEncrypted = false;
        if (m_pArchive->GetBoolProperty(PropertyIndexEnum::kpidEncrypted, archiveEncrypted) && archiveEncrypted) {
            invalidate(ArchiveOpenError::PasswordProtected);
            return;
        }

        unsigned int itemCount = 0;
        if (!m_pArchive->GetItemCount(&itemCount)) {
            invalidate(ArchiveOpenError::Corrupt);
            return;
        }

        for (unsigned int index = 0; index < itemCount; ++index) {
            C7ZipArchiveItem *item = nullptr;
            if (!m_pArchive->GetItemInfo(index, &item) || !item) {
                invalidate(ArchiveOpenError::Corrupt);
                return;
            }
            Qt7zFileInfo info = fileInfoFromItem(*item, m_packagePath);
            if (info.isEncrypted) {
                invalidate(ArchiveOpenError::PasswordProtected);
                return;
            }
            m_fileInfoList.append(info);
            m_fileNameToIndex.insert(info.fileName, index);
        }
    }

    ~FileLoader7zArchivePrivate()
    {
        closeArchive();
    }

    ArchiveOpenError error() const { return m_error; }

    bool isSolid() const
    {
        if (!m_pArchive) {
            return false;
        }
        bool solid = false;
        return m_pArchive->GetBoolProperty(PropertyIndexEnum::kpidSolid, solid) && solid;
    }

    bool hasExtractedFiles() const { return m_hasExtractedFiles; }

    FileLoadResult extractFile(const QString &name, QIODevice *outStream)
    {
        if (m_error != ArchiveOpenError::None) {
            return {{}, m_error, false};
        }
        if (!m_pArchive || !outStream || !outStream->isWritable()) {
            return {{}, ArchiveOpenError::IoError, false};
        }

        const auto indexIt = m_fileNameToIndex.constFind(name);
        if (indexIt == m_fileNameToIndex.cend()) {
            return {{}, ArchiveOpenError::Unsupported, false};
        }
        const uint32_t index = indexIt.value();
        const Qt7zFileInfo &info = m_fileInfoList.at(static_cast<int>(index));
        if (info.isEncrypted) {
            invalidate(ArchiveOpenError::PasswordProtected);
            return {{}, ArchiveOpenError::PasswordProtected, false};
        }

        if (m_hasExtractedFiles && m_tempDir) {
            const QString path = QDir(m_tempDir->path()).filePath(QString::number(index));
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                return {{}, ArchiveOpenError::IoError, false};
            }
            const QByteArray bytes = file.readAll();
            if (file.error() != QFile::NoError || outStream->write(bytes) != bytes.size()) {
                return {{}, ArchiveOpenError::IoError, false};
            }
            return {bytes, ArchiveOpenError::None, true};
        }

        Qt7zStreamWriter writer(outStream);
        if (!m_pArchive->Extract(index, &writer)) {
            ArchiveOpenError extractError = mapLib7zipError(c7zipLib->GetLastError());
            if (extractError == ArchiveOpenError::None) {
                extractError = info.isEncrypted ? ArchiveOpenError::PasswordProtected : ArchiveOpenError::Corrupt;
            }
            invalidate(extractError);
            return {{}, extractError, false};
        }
        return {{}, ArchiveOpenError::None, true};
    }

    ArchiveOpenError extractToTemporaryDirectory(const QString &pathTemplate)
    {
        if (m_error != ArchiveOpenError::None) {
            return m_error;
        }
        if (!m_pArchive) {
            return ArchiveOpenError::Corrupt;
        }

        std::unique_ptr<QTemporaryDir> candidate(new QTemporaryDir(pathTemplate));
        if (!candidate->isValid()) {
            return ArchiveOpenError::IoError;
        }
        candidate->setAutoRemove(true);

        for (int i = 0; i < m_fileInfoList.size(); ++i) {
            const Qt7zFileInfo &info = m_fileInfoList.at(i);
            if (info.isDir) {
                continue;
            }
            if (info.isEncrypted) {
                invalidate(ArchiveOpenError::PasswordProtected);
                return ArchiveOpenError::PasswordProtected;
            }
            QFile file(QDir(candidate->path()).filePath(QString::number(i)));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                return ArchiveOpenError::IoError;
            }
            Qt7zStreamWriter writer(&file);
            if (!m_pArchive->Extract(static_cast<unsigned int>(i), &writer)) {
                ArchiveOpenError extractError = mapLib7zipError(c7zipLib->GetLastError());
                if (extractError == ArchiveOpenError::None) {
                    extractError = info.isEncrypted ? ArchiveOpenError::PasswordProtected : ArchiveOpenError::Corrupt;
                }
                invalidate(extractError);
                return extractError;
            }
            if (!file.flush() || file.error() != QFile::NoError) {
                return ArchiveOpenError::IoError;
            }
        }

        m_tempDir = std::move(candidate);
        m_hasExtractedFiles = true;
        return ArchiveOpenError::None;
    }

    void invalidate(ArchiveOpenError error)
    {
        m_error = error;
        m_hasExtractedFiles = false;
        m_tempDir.reset();
        m_fileInfoList.clear();
        m_fileNameToIndex.clear();
        m_fileInfoMap.clear();
        closeArchive();
    }

    QString m_packagePath;
    C7ZipArchive *m_pArchive;
    QList<Qt7zFileInfo> m_fileInfoList;
    QHash<QString, uint32_t> m_fileNameToIndex;
    QMap<QString, Qt7zFileInfo> m_fileInfoMap;

private:
    void closeArchive()
    {
        if (m_pArchive) {
            m_pArchive->Close();
            m_pArchive = nullptr;
        }
    }

    QFile m_archiveFile;
    Qt7zStreamReader m_stream;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    bool m_hasExtractedFiles = false;
    ArchiveOpenError m_error = ArchiveOpenError::None;
};

bool FileLoader7zArchive::initializeLib()
{
    static QMutex initializeMutex;
    QMutexLocker locker(&initializeMutex);
    if (c7zipLib) {
        return c7ziplibAlive;
    }

    c7zipLib = new C7ZipLibrary();
    c7ziplibAlive = c7zipLib->Initialize();
    st_supportedArchiveFormats.clear();
    if (!c7ziplibAlive) {
        return false;
    }

    std::vector<wstring> formats;
    c7zipLib->GetSupportedExts(formats);
    for (const wstring &format : formats) {
        const QString extension = QString::fromStdWString(format).toLower();
        if (!st_supportedArchiveFormats.contains(extension)) {
            st_supportedArchiveFormats.append(extension);
        }
    }
    return true;
}

bool FileLoader7zArchive::isInitialized()
{
    return c7zipLib && c7ziplibAlive;
}

void FileLoader7zArchive::uninitializeLib()
{
    if (c7zipLib) {
        delete c7zipLib;
        c7zipLib = nullptr;
    }
    c7ziplibAlive = false;
    st_supportedArchiveFormats.clear();
}

FileLoader7zArchive::FileLoader7zArchive(QObject *parent,
                                         QString sevenzippath,
                                         QString extensionOfFile,
                                         bool extractSolidArchiveToTemporaryDir)
    : IFileLoader(parent),
      d(new FileLoader7zArchivePrivate(sevenzippath, extensionOfFile)),
      m_volumepath(std::move(sevenzippath)),
      m_extensionOfFile(std::move(extensionOfFile)),
      m_valid(false),
      m_cacheMode(InflateNoCached),
      m_archiveOpenError(d->error()),
      m_extractSolidArchiveToTemporaryDir(extractSolidArchiveToTemporaryDir)
{
    initialize();
}

FileLoader7zArchive::~FileLoader7zArchive()
{
    delete d;
    d = nullptr;
}

void FileLoader7zArchive::rejectArchive(ArchiveOpenError error)
{
    m_archiveOpenError = error;
    m_valid = false;
    m_cacheMode = InflateNoCached;
    m_imageFileList.clear();
    m_subArchiveList.clear();
    if (d && d->error() == ArchiveOpenError::None) {
        d->invalidate(error);
    }
}

void FileLoader7zArchive::initialize()
{
    if (!d || d->error() != ArchiveOpenError::None) {
        rejectArchive(d ? d->error() : ArchiveOpenError::Corrupt);
        return;
    }

    m_imageFileList.clear();
    m_subArchiveList.clear();
    d->m_fileInfoMap.clear();
    for (const Qt7zFileInfo &info : d->m_fileInfoList) {
        if (info.isDir) {
            continue;
        }
        if (info.isEncrypted) {
            rejectArchive(ArchiveOpenError::PasswordProtected);
            return;
        }
        const QString filename = QDir::fromNativeSeparators(info.fileName);
        if (IFileLoader::isImageFile(filename)) {
            m_imageFileList.append(filename);
            d->m_fileInfoMap.insert(filename, info);
        } else if (IFileLoader::isArchiveFile(filename)) {
            m_subArchiveList.append(filename);
            d->m_fileInfoMap.insert(filename, info);
        }
    }
    IFileLoader::sortFiles(m_imageFileList);
    IFileLoader::sortFiles(m_subArchiveList);

    if (m_extractSolidArchiveToTemporaryDir && d->isSolid()) {
        const QString pathTemplate = QDir(QDir::tempPath()).filePath(QStringLiteral("qv_XXXXXX"));
        const ArchiveOpenError extractionError = d->extractToTemporaryDirectory(pathTemplate);
        if (extractionError != ArchiveOpenError::None) {
            rejectArchive(extractionError);
            return;
        }
        m_cacheMode = InflateCached;
    }

    m_archiveOpenError = ArchiveOpenError::None;
    m_valid = true;
}

QStringList FileLoader7zArchive::contents()
{
    return m_imageFileList;
}

FileLoadResult FileLoader7zArchive::getFileResult(QString name, QMutex &mutex)
{
    QMutexLocker locker(&mutex);
    if (m_archiveOpenError != ArchiveOpenError::None || !m_valid) {
        return {{}, m_archiveOpenError, false};
    }
    const auto infoIt = d->m_fileInfoMap.constFind(name);
    if (infoIt == d->m_fileInfoMap.cend() || !m_imageFileList.contains(name)) {
        return {{}, ArchiveOpenError::Unsupported, false};
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return {{}, ArchiveOpenError::IoError, false};
    }
    FileLoadResult extraction = d->extractFile(infoIt.value().fileName, &buffer);
    if (!extraction.success) {
        rejectArchive(extraction.error);
        return extraction;
    }
    extraction.data = bytes;
    return extraction;
}

QByteArray FileLoader7zArchive::getFile(QString name, QMutex &mutex)
{
    return getFileResult(std::move(name), mutex).data;
}

quint64 FileLoader7zArchive::getFileSize(QString filename) const
{
    const auto info = d->m_fileInfoMap.constFind(filename);
    return info == d->m_fileInfoMap.cend() ? 0 : info.value().size;
}

QDateTime FileLoader7zArchive::getFileModified(QString filename) const
{
    const auto info = d->m_fileInfoMap.constFind(filename);
    return info == d->m_fileInfoMap.cend() ? QDateTime() : info.value().modified;
}
