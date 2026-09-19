#include "rarextractor.h"
#include "raraccessstrategy.h"

#include <utility>

RarExtractor::RarExtractor()
    : m_isHeadersEncrypted(false),
      m_isFilesEncrypted(false),
      m_hasScaned(false),
      m_isSolid(false),
      m_mode(OpenModeNotOpen),
      m_archiveError(RarArchiveError::None)
{
}

RarExtractor::RarExtractor(const QString &arcName)
    : m_arcName(arcName),
      m_isHeadersEncrypted(false),
      m_isFilesEncrypted(false),
      m_hasScaned(false),
      m_isSolid(false),
      m_mode(OpenModeNotOpen),
      m_archiveError(RarArchiveError::None)
{
}

RarExtractor::~RarExtractor() = default;

void RarExtractor::reject(RarArchiveError error)
{
    m_archiveError = error == RarArchiveError::None ? RarArchiveError::Corrupt : error;
    m_fileInfoList.clear();
    m_fileNameToIndexSensitive.clear();
    m_fileNameToIndexInsensitive.clear();
    m_accessStrategy.reset();
    m_comment.clear();
    m_hasScaned = false;
    m_mode = OpenModeNotOpen;
}

bool RarExtractor::open(OpenMode mode)
{
    reset();
    if (mode == OpenModeNotOpen) {
        reject(RarArchiveError::Unsupported);
        return false;
    }

    RarArchive listArchive(m_arcName, &m_statistics);
    if (!listArchive.open(RarArchive::OpenMode::List)) {
        m_isHeadersEncrypted = listArchive.headersEncrypted();
        reject(listArchive.error());
        return false;
    }

    m_isHeadersEncrypted = listArchive.headersEncrypted();
    m_isSolid = listArchive.isSolid();
    m_comment = listArchive.comment();
    if (!scanFileInfo(listArchive)) {
        return false;
    }

    listArchive.close();

    QStringList physicalEntries;
    physicalEntries.reserve(m_fileInfoList.size());
    for (const RARFileInfo &info : m_fileInfoList) {
        physicalEntries.append(info.fileName);
    }

    if (m_isSolid) {
        m_accessStrategy.reset(
            new SolidRarAccessStrategy(m_arcName, std::move(physicalEntries), &m_statistics));
    } else {
        m_accessStrategy.reset(
            new NonSolidRarAccessStrategy(m_arcName, std::move(physicalEntries), &m_statistics));
    }

    ++m_statistics.reopenCount;
    if (!m_accessStrategy->open()) {
        reject(m_accessStrategy->error());
        return false;
    }

    m_hasScaned = true;
    m_mode = mode;
    return true;
}

void RarExtractor::reset()
{
    m_accessStrategy.reset();
    m_fileInfoList.clear();
    m_fileNameToIndexSensitive.clear();
    m_fileNameToIndexInsensitive.clear();
    m_comment.clear();
    m_isHeadersEncrypted = false;
    m_isFilesEncrypted = false;
    m_hasScaned = false;
    m_isSolid = false;
    m_mode = OpenModeNotOpen;
    m_archiveError = RarArchiveError::None;
    m_statistics = {};
}

bool RarExtractor::reopen()
{
    if (!m_accessStrategy || m_archiveError != RarArchiveError::None) {
        return false;
    }
    if (!m_accessStrategy->reopen()) {
        reject(m_accessStrategy->error());
        return false;
    }
    return true;
}

bool RarExtractor::scanFileInfo()
{
    const OpenMode mode = m_mode == OpenModeNotOpen ? OpenModeList : m_mode;
    return open(mode);
}

bool RarExtractor::scanFileInfo(RarArchive &archive)
{
    m_fileInfoList.clear();
    m_fileNameToIndexSensitive.clear();
    m_fileNameToIndexInsensitive.clear();
    m_isFilesEncrypted = false;

    int index = 0;
    for (;;) {
        RARFileInfo info;
        const RarArchive::HeaderReadResult readResult = archive.readHeader(&info);
        if (readResult == RarArchive::HeaderReadResult::End) {
            break;
        }
        if (readResult == RarArchive::HeaderReadResult::Error) {
            m_isFilesEncrypted = archive.filesEncrypted();
            reject(archive.error());
            return false;
        }

        m_fileInfoList.append(info);
        m_fileNameToIndexSensitive.insert(info.fileName, index);
        m_fileNameToIndexInsensitive.insert(info.fileName.toLower(), index);
        ++index;

        if (!archive.skipCurrent()) {
            m_isFilesEncrypted = archive.filesEncrypted();
            reject(archive.error());
            return false;
        }
    }
    return true;
}

QStringList RarExtractor::fileNameList() const
{
    QStringList list;
    list.reserve(m_fileInfoList.size());
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
    if (!m_accessStrategy) {
        return {{}, RarArchiveError::IoError, false};
    }

    const auto sensitive = m_fileNameToIndexSensitive.constFind(fileName);
    const auto insensitive = m_fileNameToIndexInsensitive.constFind(fileName.toLower());
    if (sensitive == m_fileNameToIndexSensitive.cend() &&
        insensitive == m_fileNameToIndexInsensitive.cend()) {
        return {{}, RarArchiveError::Unsupported, false};
    }

    const int targetIndex =
        sensitive != m_fileNameToIndexSensitive.cend() ? sensitive.value() : insensitive.value();
    const QString resolvedFileName = m_fileInfoList.at(targetIndex).fileName;
    RarFileDataResult result = m_accessStrategy->read(resolvedFileName);
    if (!result.success && result.error != RarArchiveError::Unsupported) {
        reject(result.error);
    }
    return result;
}

void RarExtractor::resetStatistics()
{
    m_statistics = {};
}

QByteArray RarExtractor::fileData(QString fileName)
{
    return fileDataResult(std::move(fileName)).data;
}
