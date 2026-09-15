#include "raraccessstrategy.h"
#include "unrar/rar.hpp"

#include <utility>

namespace {

static int CALLBACK rarArchiveCallback(UINT msg, LPARAM rawArchive, LPARAM, LPARAM)
{
    RarArchive *archive = reinterpret_cast<RarArchive *>(rawArchive);
    if (!archive) {
        return -1;
    }
    if (msg == UCM_NEEDPASSWORD || msg == UCM_NEEDPASSWORDW) {
        archive->markPasswordRequested();
        return -1;
    }
    return 1;
}

struct RarFileWriter
{
    RarFileWriter()
    {
        buffer.open(QIODevice::ReadWrite);
    }

    static int CALLBACK callback(UINT msg, LPARAM rawWriter, LPARAM p1, LPARAM p2)
    {
        RarFileWriter *writer = reinterpret_cast<RarFileWriter *>(rawWriter);
        if (!writer) {
            return -1;
        }
        if (msg == UCM_PROCESSDATA) {
            const char *data = reinterpret_cast<const char *>(p1);
            const qint64 size = static_cast<qint64>(p2);
            return writer->buffer.write(data, size) == size ? 1 : -1;
        }
        if (msg == UCM_NEEDPASSWORD || msg == UCM_NEEDPASSWORDW) {
            writer->passwordRequested = true;
            return -1;
        }
        return 1;
    }

    QByteArray data()
    {
        buffer.seek(0);
        return buffer.readAll();
    }

    QBuffer buffer;
    bool passwordRequested = false;
};

}

RarArchive::RarArchive(QString archiveName, RarAccessStatistics *statistics)
    : m_archiveName(std::move(archiveName)),
      m_handle(nullptr),
      m_error(RarArchiveError::None),
      m_statistics(statistics),
      m_passwordRequested(false),
      m_isSolid(false),
      m_headersEncrypted(false),
      m_filesEncrypted(false)
{}

RarArchive::~RarArchive()
{
    close();
}

RarArchiveError RarArchive::mapUnrarError(int error, bool passwordRequested)
{
    if (passwordRequested || error == ERAR_MISSING_PASSWORD || error == ERAR_BAD_PASSWORD) {
        return RarArchiveError::PasswordProtected;
    }
    switch (error) {
    case ERAR_SUCCESS:
    case ERAR_END_ARCHIVE:
        return RarArchiveError::None;
    case ERAR_UNKNOWN_FORMAT:
        return RarArchiveError::Unsupported;
    case ERAR_BAD_DATA:
    case ERAR_BAD_ARCHIVE:
        return RarArchiveError::Corrupt;
    case ERAR_EOPEN:
    case ERAR_EREAD:
    case ERAR_EWRITE:
    case ERAR_ECLOSE:
    case ERAR_ECREATE:
        return RarArchiveError::IoError;
    default:
        return RarArchiveError::Corrupt;
    }
}

void RarArchive::setError(RarArchiveError error)
{
    m_error = error == RarArchiveError::None ? RarArchiveError::Corrupt : error;
}

bool RarArchive::open(OpenMode mode)
{
    close();
    m_error = RarArchiveError::None;
    m_comment.clear();
    m_passwordRequested = false;
    m_isSolid = false;
    m_headersEncrypted = false;
    m_filesEncrypted = false;

    RAROpenArchiveDataEx archiveData = {};
    wchar_t archiveName[RarExtractor::MAX_ARC_NAME_SIZE] = {};
    m_archiveName.left(RarExtractor::MAX_ARC_NAME_SIZE - 1).toWCharArray(archiveName);

    QByteArray commentBuffer(RarExtractor::MAX_COMMENT_SIZE, '\0');
    archiveData.ArcNameW = archiveName;
    archiveData.ArcName = nullptr;
    archiveData.CmtBuf = commentBuffer.data();
    archiveData.CmtBufSize = static_cast<unsigned int>(commentBuffer.size());
    archiveData.Callback = rarArchiveCallback;
    archiveData.UserData = reinterpret_cast<LPARAM>(this);
    archiveData.OpenMode = mode == OpenMode::List ? RAR_OM_LIST : RAR_OM_EXTRACT;

    m_handle = RAROpenArchiveEx(&archiveData);
    if (!m_handle || archiveData.OpenResult != ERAR_SUCCESS) {
        setError(mapUnrarError(static_cast<int>(archiveData.OpenResult), m_passwordRequested));
        close();
        return false;
    }

    m_isSolid = (archiveData.Flags & ROADF_SOLID) != 0;
    m_headersEncrypted = (archiveData.Flags & ROADF_ENCHEADERS) != 0;
    if (m_headersEncrypted || m_passwordRequested) {
        setError(RarArchiveError::PasswordProtected);
        close();
        return false;
    }

    if (archiveData.CmtSize > 0) {
        int size = qMin(static_cast<int>(archiveData.CmtSize), commentBuffer.size());
        if (size > 0 && commentBuffer.at(size - 1) == '\0') {
            --size;
        }
        m_comment = QString::fromLocal8Bit(commentBuffer.constData(), size);
    }
    return true;
}

void RarArchive::close()
{
    if (m_handle) {
        RARCloseArchive(m_handle);
        m_handle = nullptr;
    }
}

RarArchive::HeaderReadResult RarArchive::readHeader(RARFileInfo *info)
{
    if (!m_handle) {
        setError(RarArchiveError::IoError);
        return HeaderReadResult::Error;
    }

    if (m_statistics) {
        ++m_statistics->readHeaderCount;
    }

    RARHeaderDataEx header = {};
    m_passwordRequested = false;
    const int headerResult = RARReadHeaderEx(m_handle, &header);
    if (headerResult == ERAR_END_ARCHIVE) {
        return HeaderReadResult::End;
    }
    if (headerResult != ERAR_SUCCESS) {
        setError(mapUnrarError(headerResult, m_passwordRequested));
        return HeaderReadResult::Error;
    }
    if ((header.Flags & RHDF_ENCRYPTED) != 0 || m_passwordRequested) {
        m_filesEncrypted = true;
        setError(RarArchiveError::PasswordProtected);
        return HeaderReadResult::Error;
    }

    if (info) {
        info->fileName = QString::fromWCharArray(header.FileNameW);
        info->arcName = m_archiveName;
        info->flags = header.Flags;
        info->packSize = header.PackSize;
        info->unpSize = header.UnpSize;
        info->hostOS = header.HostOS;
        info->fileCRC = header.FileCRC;
        info->fileTime = header.FileTime;
        info->unpVer = header.UnpVer;
        info->method = header.Method;
        info->fileAttr = header.FileAttr;
        info->comment = m_comment;
    }
    return HeaderReadResult::Entry;
}

bool RarArchive::skipCurrent()
{
    if (!m_handle) {
        setError(RarArchiveError::IoError);
        return false;
    }

    if (m_statistics) {
        ++m_statistics->skipCount;
    }

    m_passwordRequested = false;
    const int processResult = RARProcessFile(m_handle, RAR_SKIP, nullptr, nullptr);
    if (processResult != ERAR_SUCCESS || m_passwordRequested) {
        setError(mapUnrarError(processResult, m_passwordRequested));
        return false;
    }
    return true;
}

RarFileDataResult RarArchive::extractCurrent()
{
    if (!m_handle) {
        setError(RarArchiveError::IoError);
        return {{}, m_error, false};
    }

    if (m_statistics) {
        ++m_statistics->extractCount;
    }

    RarFileWriter writer;
    RARSetCallback(m_handle, RarFileWriter::callback, reinterpret_cast<LPARAM>(&writer));
    const int processResult = RARProcessFile(m_handle, RAR_TEST, nullptr, nullptr);
    RARSetCallback(m_handle, rarArchiveCallback, reinterpret_cast<LPARAM>(this));
    if (processResult != ERAR_SUCCESS || writer.passwordRequested) {
        setError(mapUnrarError(processResult, writer.passwordRequested));
        return {{}, m_error, false};
    }
    return {writer.data(), RarArchiveError::None, true};
}

NonSolidRarAccessStrategy::NonSolidRarAccessStrategy(
    QString archiveName, QStringList physicalEntries, RarAccessStatistics *statistics)
    : m_archive(new RarArchive(std::move(archiveName), statistics)),
      m_physicalEntries(std::move(physicalEntries)),
      m_statistics(statistics),
      m_cursor(0)
{
    for (int index = 0; index < m_physicalEntries.size(); ++index) {
        const QString &fileName = m_physicalEntries.at(index);
        if (!m_physicalEntryIndex.contains(fileName)) {
            m_physicalEntryIndex.insert(fileName, index);
        }
    }
}

bool NonSolidRarAccessStrategy::open()
{
    m_cursor = 0;
    return m_archive->open(RarArchive::OpenMode::Extract);
}

bool NonSolidRarAccessStrategy::reopen()
{
    if (m_statistics) {
        ++m_statistics->reopenCount;
    }
    m_cursor = 0;
    return m_archive->open(RarArchive::OpenMode::Extract);
}

RarFileDataResult NonSolidRarAccessStrategy::read(const QString &fileName)
{
    if (m_archive->error() != RarArchiveError::None) {
        return {{}, m_archive->error(), false};
    }

    const auto target = m_physicalEntryIndex.constFind(fileName);
    if (target == m_physicalEntryIndex.cend()) {
        return {{}, RarArchiveError::Unsupported, false};
    }
    const int targetIndex = target.value();

    if (targetIndex < m_cursor && !reopen()) {
        return {{}, m_archive->error(), false};
    }

    while (m_cursor <= targetIndex) {
        RARFileInfo info;
        const RarArchive::HeaderReadResult readResult = m_archive->readHeader(&info);
        if (readResult == RarArchive::HeaderReadResult::End) {
            return {{}, RarArchiveError::Corrupt, false};
        }
        if (readResult == RarArchive::HeaderReadResult::Error) {
            return {{}, m_archive->error(), false};
        }
        if (m_cursor >= m_physicalEntries.size() || info.fileName != m_physicalEntries.at(m_cursor)) {
            return {{}, RarArchiveError::Corrupt, false};
        }

        if (m_cursor < targetIndex) {
            if (!m_archive->skipCurrent()) {
                return {{}, m_archive->error(), false};
            }
            ++m_cursor;
            continue;
        }

        RarFileDataResult result = m_archive->extractCurrent();
        if (!result.success) {
            return result;
        }
        ++m_cursor;
        return result;
    }

    return {{}, RarArchiveError::Corrupt, false};
}

RarArchiveError NonSolidRarAccessStrategy::error() const
{
    return m_archive->error();
}

SolidRarAccessStrategy::SolidRarAccessStrategy(
    QString archiveName, QStringList physicalEntries, RarAccessStatistics *statistics)
    : m_archive(new RarArchive(std::move(archiveName), statistics)),
      m_physicalEntries(std::move(physicalEntries)),
      m_dataCache(RarExtractor::MAX_DATA_CACHE_KIB),
      m_statistics(statistics),
      m_cursor(0)
{
    for (int index = 0; index < m_physicalEntries.size(); ++index) {
        m_physicalEntryIndex.insert(m_physicalEntries.at(index), index);
    }
}

bool SolidRarAccessStrategy::open()
{
    m_cursor = 0;
    m_dataCache.clear();
    return m_archive->open(RarArchive::OpenMode::Extract);
}

bool SolidRarAccessStrategy::reopen()
{
    if (m_statistics) {
        ++m_statistics->reopenCount;
    }
    m_cursor = 0;
    return m_archive->open(RarArchive::OpenMode::Extract);
}

RarFileDataResult SolidRarAccessStrategy::read(const QString &fileName)
{
    const auto target = m_physicalEntryIndex.constFind(fileName);
    if (target == m_physicalEntryIndex.cend()) {
        return {{}, RarArchiveError::Unsupported, false};
    }
    const int targetIndex = target.value();

    if (const QByteArray *cached = m_dataCache.object(targetIndex)) {
        if (m_statistics) {
            ++m_statistics->cacheHitCount;
        }
        return {*cached, RarArchiveError::None, true};
    }

    if (targetIndex < m_cursor && !reopen()) {
        return {{}, m_archive->error(), false};
    }

    while (m_cursor <= targetIndex) {
        RARFileInfo info;
        const RarArchive::HeaderReadResult readResult = m_archive->readHeader(&info);
        if (readResult == RarArchive::HeaderReadResult::End) {
            return {{}, RarArchiveError::Corrupt, false};
        }
        if (readResult == RarArchive::HeaderReadResult::Error) {
            return {{}, m_archive->error(), false};
        }
        if (m_cursor >= m_physicalEntries.size() || info.fileName != m_physicalEntries.at(m_cursor)) {
            return {{}, RarArchiveError::Corrupt, false};
        }

        if (m_cursor < targetIndex) {
            if (!m_archive->skipCurrent()) {
                return {{}, m_archive->error(), false};
            }
            ++m_cursor;
            continue;
        }

        RarFileDataResult result = m_archive->extractCurrent();
        if (!result.success) {
            return result;
        }
        ++m_cursor;

        const qsizetype cacheCost = qMax<qsizetype>(1, (result.data.size() + 1023) / 1024);
        if (cacheCost <= RarExtractor::MAX_DATA_CACHE_KIB) {
            m_dataCache.insert(targetIndex, new QByteArray(result.data), static_cast<int>(cacheCost));
        }
        return result;
    }

    return {{}, RarArchiveError::Corrupt, false};
}

RarArchiveError SolidRarAccessStrategy::error() const
{
    return m_archive->error();
}
