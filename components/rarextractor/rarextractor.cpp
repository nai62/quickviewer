#include "rarextractor.h"
#include "unrar/rar.hpp"

namespace {

static int CALLBACK rarOpenCallback(UINT msg, LPARAM rawSelf, LPARAM, LPARAM)
{
    RarExtractor *self = reinterpret_cast<RarExtractor *>(rawSelf);
    if (!self) {
        return -1;
    }
    if (msg == UCM_NEEDPASSWORD || msg == UCM_NEEDPASSWORDW) {
        self->markPasswordRequested();
        return -1;
    }
    return 1;
}

struct RARFileWriter
{
    explicit RARFileWriter(RARFileInfo *fileInfo)
        : info(fileInfo)
    {
        buffer.open(QIODevice::ReadWrite);
    }

    static int CALLBACK callback(UINT msg, LPARAM rawSelf, LPARAM p1, LPARAM p2)
    {
        RARFileWriter *self = reinterpret_cast<RARFileWriter *>(rawSelf);
        if (!self) {
            return -1;
        }
        if (msg == UCM_PROCESSDATA) {
            const char *data = reinterpret_cast<const char *>(p1);
            const qint64 size = static_cast<qint64>(p2);
            return self->buffer.write(data, size) == size ? 1 : -1;
        }
        if (msg == UCM_NEEDPASSWORD || msg == UCM_NEEDPASSWORDW) {
            self->passwordRequested = true;
            return -1;
        }
        return 1;
    }

    void commit()
    {
        buffer.seek(0);
        info->data = buffer.readAll();
    }

    RARFileInfo *info;
    QBuffer buffer;
    bool passwordRequested = false;
};
}

RarExtractor::RarExtractor()
    : m_hArc(nullptr),
      m_error(ERAR_SUCCESS),
      m_isHeadersEncrypted(false),
      m_isFilesEncrypted(false),
      m_hasScaned(false),
      m_curIndex(0),
      m_mode(OpenModeNotOpen),
      m_dataCache(MAX_DATA_CACHE_KIB),
      m_archiveError(RarArchiveError::None),
      m_passwordRequested(false)
{}

RarExtractor::RarExtractor(const QString &arcName)
    : m_hArc(nullptr),
      m_error(ERAR_SUCCESS),
      m_arcName(arcName),
      m_isHeadersEncrypted(false),
      m_isFilesEncrypted(false),
      m_hasScaned(false),
      m_curIndex(0),
      m_mode(OpenModeNotOpen),
      m_dataCache(MAX_DATA_CACHE_KIB),
      m_archiveError(RarArchiveError::None),
      m_passwordRequested(false)
{}

RarExtractor::~RarExtractor()
{
    closeHandle();
}

RarArchiveError RarExtractor::mapUnrarError(int error, bool passwordRequested)
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

void RarExtractor::closeHandle()
{
    if (m_hArc) {
        RARCloseArchive(m_hArc);
        m_hArc = nullptr;
    }
    m_mode = OpenModeNotOpen;
    m_curIndex = 0;
}

void RarExtractor::reject(RarArchiveError error)
{
    m_archiveError = error;
    m_fileInfoList.clear();
    m_fileNameToIndexSensitive.clear();
    m_fileNameToIndexInsensitive.clear();
    m_dataCache.clear();
    m_comment.clear();
    m_curIndex = 0;
    closeHandle();
}

bool RarExtractor::openHandle(OpenMode mode)
{
    RAROpenArchiveDataEx arcData = {};
    wchar_t archiveName[RarExtractor::MAX_ARC_NAME_SIZE] = {};
    m_arcName.left(RarExtractor::MAX_ARC_NAME_SIZE - 1).toWCharArray(archiveName);

    QByteArray commentBuffer(RarExtractor::MAX_COMMENT_SIZE, '\0');
    arcData.ArcNameW = archiveName;
    arcData.ArcName = nullptr;
    arcData.CmtBuf = commentBuffer.data();
    arcData.CmtBufSize = static_cast<unsigned int>(commentBuffer.size());
    arcData.Callback = rarOpenCallback;
    arcData.UserData = reinterpret_cast<LPARAM>(this);
    // Keep the archive handle extraction-capable even when the initial scan only lists entries.
    arcData.OpenMode = RAR_OM_EXTRACT;

    m_passwordRequested = false;
    m_hArc = RAROpenArchiveEx(&arcData);
    m_error = static_cast<int>(arcData.OpenResult);
    if (!m_hArc || m_error != ERAR_SUCCESS) {
        const RarArchiveError openError = mapUnrarError(m_error, m_passwordRequested);
        closeHandle();
        m_archiveError = openError == RarArchiveError::None ? RarArchiveError::Corrupt : openError;
        return false;
    }

    m_mode = mode;
    m_curIndex = 0;
    m_isHeadersEncrypted = (arcData.Flags & ROADF_ENCHEADERS) != 0;
    if (m_isHeadersEncrypted || m_passwordRequested) {
        reject(RarArchiveError::PasswordProtected);
        return false;
    }

    m_comment.clear();
    if (arcData.CmtSize > 0) {
        int size = qMin(static_cast<int>(arcData.CmtSize), commentBuffer.size());
        if (size > 0 && commentBuffer.at(size - 1) == '\0') {
            --size;
        }
        m_comment = QString::fromLocal8Bit(commentBuffer.constData(), size);
    }
    return true;
}

bool RarExtractor::open(OpenMode mode)
{
    reset();
    if (!openHandle(mode)) {
        return false;
    }
    if (!scanFileInfo()) {
        return false;
    }

    closeHandle();
    if (!openHandle(mode)) {
        return false;
    }
    m_hasScaned = true;
    return true;
}

void RarExtractor::reset()
{
    closeHandle();
    m_fileInfoList.clear();
    m_fileNameToIndexSensitive.clear();
    m_fileNameToIndexInsensitive.clear();
    m_dataCache.clear();
    m_comment.clear();
    m_isHeadersEncrypted = false;
    m_isFilesEncrypted = false;
    m_hasScaned = false;
    m_curIndex = 0;
    m_error = ERAR_SUCCESS;
    m_archiveError = RarArchiveError::None;
    m_passwordRequested = false;
}

bool RarExtractor::reopen()
{
    if (m_archiveError != RarArchiveError::None) {
        return false;
    }
    const OpenMode mode = m_mode;
    closeHandle();
    return openHandle(mode);
}

bool RarExtractor::scanFileInfo()
{
    if (!isOpen()) {
        return false;
    }

    m_fileInfoList.clear();
    m_fileNameToIndexSensitive.clear();
    m_fileNameToIndexInsensitive.clear();
    m_dataCache.clear();
    m_isFilesEncrypted = false;

    int index = 0;
    for (;;) {
        RARHeaderDataEx header = {};
        m_passwordRequested = false;
        const int headerResult = RARReadHeaderEx(m_hArc, &header);
        if (headerResult == ERAR_END_ARCHIVE) {
            break;
        }
        if (headerResult != ERAR_SUCCESS) {
            reject(mapUnrarError(headerResult, m_passwordRequested));
            return false;
        }
        if ((header.Flags & RHDF_ENCRYPTED) != 0 || m_passwordRequested) {
            m_isFilesEncrypted = true;
            reject(RarArchiveError::PasswordProtected);
            return false;
        }

        RARFileInfo info;
        info.fileName = QString::fromWCharArray(header.FileNameW);
        info.arcName = m_arcName;
        info.flags = header.Flags;
        info.packSize = header.PackSize;
        info.unpSize = header.UnpSize;
        info.hostOS = header.HostOS;
        info.fileCRC = header.FileCRC;
        info.fileTime = header.FileTime;
        info.unpVer = header.UnpVer;
        info.method = header.Method;
        info.fileAttr = header.FileAttr;
        info.comment = m_comment;

        int processResult = ERAR_SUCCESS;
        bool passwordRequested = false;
        if (m_mode == OpenModeExtract) {
            RARFileWriter writer(&info);
            RARSetCallback(m_hArc, RARFileWriter::callback, reinterpret_cast<LPARAM>(&writer));
            processResult = RARProcessFile(m_hArc, RAR_TEST, nullptr, nullptr);
            RARSetCallback(m_hArc, rarOpenCallback, reinterpret_cast<LPARAM>(this));
            passwordRequested = writer.passwordRequested;
            if (processResult == ERAR_SUCCESS && !passwordRequested) {
                writer.commit();
            }
        } else {
            processResult = RARProcessFile(m_hArc, RAR_SKIP, nullptr, nullptr);
            passwordRequested = m_passwordRequested;
        }
        if (processResult != ERAR_SUCCESS || passwordRequested) {
            reject(mapUnrarError(processResult, passwordRequested));
            return false;
        }

        m_fileInfoList.append(info);
        m_fileNameToIndexSensitive.insert(info.fileName, index);
        m_fileNameToIndexInsensitive.insert(info.fileName.toLower(), index);
        ++index;
    }
    return true;
}

QStringList RarExtractor::fileNameList() const
{
    QStringList list;
    for (const RARFileInfo &info : m_fileInfoList) {
        list.append(info.fileName);
    }
    return list;
}

RARFileInfo &RarExtractor::getFileInfo(QString filename)
{
    return m_fileInfoList[m_fileNameToIndexInsensitive.value(filename.toLower())];
}

bool RarExtractor::contains(QString filename) const
{
    return m_fileNameToIndexInsensitive.contains(filename.toLower());
}

RarFileDataResult RarExtractor::fileDataResult(QString fileName)
{
    if (m_archiveError != RarArchiveError::None) {
        return {{}, m_archiveError, false};
    }

    const auto sensitive = m_fileNameToIndexSensitive.constFind(fileName);
    const auto insensitive = m_fileNameToIndexInsensitive.constFind(fileName.toLower());
    if (sensitive == m_fileNameToIndexSensitive.cend() && insensitive == m_fileNameToIndexInsensitive.cend()) {
        return {{}, RarArchiveError::Unsupported, false};
    }
    const int targetIndex = sensitive != m_fileNameToIndexSensitive.cend() ? sensitive.value() : insensitive.value();

    if (m_mode == OpenModeExtract) {
        return {m_fileInfoList.at(targetIndex).data, RarArchiveError::None, true};
    }
    if (const QByteArray *cached = m_dataCache.object(targetIndex)) {
        return {*cached, RarArchiveError::None, true};
    }

    if (targetIndex < m_curIndex && !reopen()) {
        return {{}, m_archiveError == RarArchiveError::None ? RarArchiveError::IoError : m_archiveError, false};
    }

    while (m_curIndex < targetIndex) {
        RARHeaderDataEx header = {};
        m_passwordRequested = false;
        const int headerResult = RARReadHeaderEx(m_hArc, &header);
        if (headerResult != ERAR_SUCCESS) {
            const RarArchiveError readError = mapUnrarError(headerResult, m_passwordRequested);
            reject(readError == RarArchiveError::None ? RarArchiveError::Corrupt : readError);
            return {{}, m_archiveError, false};
        }
        if ((header.Flags & RHDF_ENCRYPTED) != 0 || m_passwordRequested) {
            reject(RarArchiveError::PasswordProtected);
            return {{}, m_archiveError, false};
        }
        const int processResult = RARProcessFile(m_hArc, RAR_SKIP, nullptr, nullptr);
        if (processResult != ERAR_SUCCESS || m_passwordRequested) {
            reject(mapUnrarError(processResult, m_passwordRequested));
            return {{}, m_archiveError, false};
        }
        ++m_curIndex;
    }

    RARHeaderDataEx header = {};
    m_passwordRequested = false;
    const int headerResult = RARReadHeaderEx(m_hArc, &header);
    if (headerResult != ERAR_SUCCESS) {
        const RarArchiveError readError = mapUnrarError(headerResult, m_passwordRequested);
        reject(readError == RarArchiveError::None ? RarArchiveError::Corrupt : readError);
        return {{}, m_archiveError, false};
    }
    if ((header.Flags & RHDF_ENCRYPTED) != 0 || m_passwordRequested) {
        reject(RarArchiveError::PasswordProtected);
        return {{}, m_archiveError, false};
    }

    RARFileInfo info;
    info.fileName = QString::fromWCharArray(header.FileNameW);
    info.arcName = m_arcName;
    info.flags = header.Flags;
    info.packSize = header.PackSize;
    info.unpSize = header.UnpSize;
    info.hostOS = header.HostOS;
    info.fileCRC = header.FileCRC;
    info.fileTime = header.FileTime;
    info.unpVer = header.UnpVer;
    info.method = header.Method;
    info.fileAttr = header.FileAttr;
    info.comment = m_comment;

    RARFileWriter writer(&info);
    RARSetCallback(m_hArc, RARFileWriter::callback, reinterpret_cast<LPARAM>(&writer));
    const int processResult = RARProcessFile(m_hArc, RAR_TEST, nullptr, nullptr);
    RARSetCallback(m_hArc, rarOpenCallback, reinterpret_cast<LPARAM>(this));
    if (processResult != ERAR_SUCCESS || writer.passwordRequested) {
        reject(mapUnrarError(processResult, writer.passwordRequested));
        return {{}, m_archiveError, false};
    }
    writer.commit();
    ++m_curIndex;

    const qsizetype cacheCost = qMax<qsizetype>(1, (info.data.size() + 1023) / 1024);
    if (cacheCost <= MAX_DATA_CACHE_KIB) {
        m_dataCache.insert(targetIndex, new QByteArray(info.data), static_cast<int>(cacheCost));
    }
    return {info.data, RarArchiveError::None, true};
}

QByteArray RarExtractor::fileData(QString fileName)
{
    return fileDataResult(std::move(fileName)).data;
}
