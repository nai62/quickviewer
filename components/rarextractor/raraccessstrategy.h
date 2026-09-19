#ifndef RARACCESSSTRATEGY_H
#define RARACCESSSTRATEGY_H

#include "rarextractor.h"

#include <memory>

class RarArchive
{
public:
    enum class OpenMode {
        List,
        Extract,
    };

    enum class HeaderReadResult {
        Entry,
        End,
        Error,
    };

    RarArchive(QString archiveName, RarAccessStatistics *statistics);
    ~RarArchive();

    bool open(OpenMode mode);
    void close();
    bool isOpen() const { return m_handle != nullptr; }
    HeaderReadResult readHeader(RARFileInfo *info);
    bool skipCurrent();
    RarFileDataResult extractCurrent();

    RarArchiveError error() const { return m_error; }
    QString comment() const { return m_comment; }
    bool isSolid() const { return m_isSolid; }
    bool headersEncrypted() const { return m_headersEncrypted; }
    bool filesEncrypted() const { return m_filesEncrypted; }
    void markPasswordRequested() { m_passwordRequested = true; }

private:
    static RarArchiveError mapUnrarError(int error, bool passwordRequested);
    void setError(RarArchiveError error);

    QString m_archiveName;
    Qt::HANDLE m_handle;
    QString m_comment;
    RarArchiveError m_error;
    RarAccessStatistics *m_statistics;
    bool m_passwordRequested;
    bool m_isSolid;
    bool m_headersEncrypted;
    bool m_filesEncrypted;
};

class IRarAccessStrategy
{
public:
    virtual ~IRarAccessStrategy() = default;

    virtual bool open() = 0;
    virtual bool reopen() = 0;
    virtual RarFileDataResult read(const QString &fileName) = 0;
    virtual RarArchiveError error() const = 0;
};

class NonSolidRarAccessStrategy final : public IRarAccessStrategy
{
public:
    NonSolidRarAccessStrategy(QString archiveName,
                              QStringList physicalEntries,
                              RarAccessStatistics *statistics);

    bool open() override;
    bool reopen() override;
    RarFileDataResult read(const QString &fileName) override;
    RarArchiveError error() const override;

private:
    std::unique_ptr<RarArchive> m_archive;
    QStringList m_physicalEntries;
    QHash<QString, int> m_physicalEntryIndex;
    RarAccessStatistics *m_statistics;
    int m_cursor;
};

class SolidRarAccessStrategy final : public IRarAccessStrategy
{
public:
    SolidRarAccessStrategy(QString archiveName,
                           QStringList physicalEntries,
                           RarAccessStatistics *statistics);

    bool open() override;
    bool reopen() override;
    RarFileDataResult read(const QString &fileName) override;
    RarArchiveError error() const override;

private:
    std::unique_ptr<RarArchive> m_archive;
    QStringList m_physicalEntries;
    QHash<QString, int> m_physicalEntryIndex;
    QCache<int, QByteArray> m_dataCache;
    RarAccessStatistics *m_statistics;
    int m_cursor;
};

#endif // RARACCESSSTRATEGY_H
