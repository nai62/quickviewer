#ifndef RAREXTRACTOR_H
#define RAREXTRACTOR_H

#include <QtCore>

#include <memory>

struct RARFileInfo
{
    QString fileName;
    QString arcName;
    unsigned int flags = 0;
    unsigned int packSize = 0;
    unsigned int unpSize = 0;
    unsigned int hostOS = 0;
    unsigned int fileCRC = 0;
    unsigned int fileTime = 0;
    unsigned int unpVer = 0;
    unsigned int method = 0;
    unsigned int fileAttr = 0;
    QString comment;
    QByteArray data;

    bool isEncrypted() const { return flags & 0x04; }
    bool isDirectory() const { return flags & 0x20; }
};

enum class RarArchiveError {
    None,
    PasswordProtected,
    Unsupported,
    Corrupt,
    IoError,
};

struct RarFileDataResult
{
    QByteArray data;
    RarArchiveError error = RarArchiveError::None;
    bool success = false;
};

struct RarAccessStatistics
{
    quint64 reopenCount = 0;
    quint64 readHeaderCount = 0;
    quint64 skipCount = 0;
    quint64 extractCount = 0;
    quint64 cacheHitCount = 0;
};

class IRarAccessStrategy;
class RarArchive;

class RarExtractor
{
public:
    enum OpenMode {
        OpenModeNotOpen,
        OpenModeList,
        OpenModeExtract,
    };

    static const int MAX_COMMENT_SIZE = 64 * 1024;
    static const int MAX_ARC_NAME_SIZE = 2048;
    static const int MAX_DATA_CACHE_KIB = 64 * 1024;

    RarExtractor();
    explicit RarExtractor(const QString &arcName);
    ~RarExtractor();

    bool open(OpenMode mode);
    void reset();
    bool reopen();
    bool scanFileInfo();
    bool isOpen() const { return m_accessStrategy != nullptr; }
    bool isSolid() const { return m_isSolid; }
    QStringList fileNameList() const;
    const QList<RARFileInfo> &fileInfoList() const { return m_fileInfoList; }
    RARFileInfo &getFileInfo(QString filename);
    bool contains(QString filename) const;

    QByteArray fileData(QString filename);
    RarFileDataResult fileDataResult(QString filename);
    RarArchiveError archiveError() const { return m_archiveError; }
    const RarAccessStatistics &statistics() const { return m_statistics; }
    void resetStatistics();

private:
    bool scanFileInfo(RarArchive &archive);
    void reject(RarArchiveError error);

    QString m_arcName;
    QString m_comment;
    bool m_isHeadersEncrypted;
    bool m_isFilesEncrypted;
    bool m_hasScaned;
    bool m_isSolid;
    QList<RARFileInfo> m_fileInfoList;
    OpenMode m_mode;

    QHash<QString, int> m_fileNameToIndexSensitive;
    QHash<QString, int> m_fileNameToIndexInsensitive;
    std::unique_ptr<IRarAccessStrategy> m_accessStrategy;
    RarArchiveError m_archiveError;
    RarAccessStatistics m_statistics;
};

#endif // RAREXTRACTOR_H
