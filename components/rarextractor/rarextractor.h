#ifndef RAREXTRACTOR_H
#define RAREXTRACTOR_H

#include <QtCore>

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
    bool isOpen() const { return m_mode != OpenModeNotOpen && m_hArc != nullptr; }
    QStringList fileNameList() const;
    RARFileInfo &getFileInfo(QString filename);
    bool contains(QString filename) const;

    QByteArray fileData(QString filename);
    RarFileDataResult fileDataResult(QString filename);
    RarArchiveError archiveError() const { return m_archiveError; }
    void markPasswordRequested() { m_passwordRequested = true; }

    Qt::HANDLE m_hArc;
    int m_error;
    QString m_arcName;
    QString m_comment;
    bool m_isHeadersEncrypted;
    bool m_isFilesEncrypted;
    bool m_hasScaned;
    int m_curIndex;
    QList<RARFileInfo> m_fileInfoList;
    OpenMode m_mode;

    QHash<QString, int> m_fileNameToIndexSensitive;
    QHash<QString, int> m_fileNameToIndexInsensitive;
    QCache<int, QByteArray> m_dataCache;

private:
    bool openHandle(OpenMode mode);
    void closeHandle();
    void reject(RarArchiveError error);
    static RarArchiveError mapUnrarError(int error, bool passwordRequested);
    RarArchiveError m_archiveError;
    bool m_passwordRequested;
};

#endif // RAREXTRACTOR_H
