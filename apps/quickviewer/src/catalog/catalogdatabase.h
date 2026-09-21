#ifndef CATALOGDATABASE_H
#define CATALOGDATABASE_H

#include <QAtomicInt>
#include <QDateTime>
#include <QFutureWatcher>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include "volume.h"

// t_catalogs
class CatalogRecord
{
public:
    int id;
    int basevolume_id;
    QString name;
    QString description;
    QString path;
    QDateTime created_at;
    QDateTime updated_at;
    bool created;
    bool operator==(const CatalogRecord &rhs) { return id == rhs.id; }
};
Q_DECLARE_METATYPE(CatalogRecord)

class VolumeThumbRecord
{
public:
    int id;
    QString name;
    QString nameNoCase;
    QString realname;
    QString realnameNoCase;
    QString path;
    int frontpage_id;
    int thumb_id;
    int parent_id;
    int catalog_id;
    QByteArray thumbnail;
};

class TagRecord
{
public:
    int id;
    QString name;
    QString nameNoCase;
    int type_id; // (0:Normal, 1:Publisher(Author), 2:Publisher, 3:Author, 4:Rate)
    int count;
    TagRecord()
        : id(-1),
          type_id(0),
          count(0)
    {
    }
    TagRecord(QString nm, int tpid)
        : id(-1),
          name(nm),
          type_id(tpid),
          count(0)
    {
    }
    inline const TagRecord &operator=(const TagRecord &rhs)
    {
        id = rhs.id;
        name = rhs.name;
        nameNoCase = rhs.nameNoCase;
        type_id = rhs.type_id;
        count = rhs.count;
        return rhs;
    }
};

class TaggedName
{
public:
    QString name;
    QString realname;
    QList<TagRecord> tags;
};

class FileWorker
{
public:
    QString filename;
    QString filepath;
    QFileInfo info;
    QSize imagesize;
    QImage thumb;
    QByteArray thumbbytes;
    QDateTime created_at;
    int asc;
    QByteArray alternated;
};

class VolumeWorker
{
public:
    QString dirpath;
    int volume_id;
    int parent_id;
    int catalog_id;
    FileWorker frontPage;
    QStringList subpaths;
};

class CatalogDatabase : public QObject
{
    Q_OBJECT
public:
    CatalogDatabase(QObject *parent, QString dbpath);
    ~CatalogDatabase() override;
    void vacuum();

    /* Catalogs */
    QMap<int, CatalogRecord> catalogs();

    CatalogRecord createCatalog(QString name, QString path);
    QFutureWatcher<QList<CatalogRecord>> *createCatalogAsync(QList<CatalogRecord> newers);
    void cancelCreateCatalogAsync();

    void deleteCatalog(int id);
    void updateCatalogName(int id, QString name);
    void deleteAllCatalogs();

    /* Volumes */
    QList<VolumeThumbRecord> volumes();

    /* Tags */
    void loadTags();
    QMap<int, TagRecord *> tagsByCount();
    QList<TagRecord> getTagsFromVolumeId(int volume_id);

    /** Formatted the way the catalog list shows a date. */
    static QString DateTimeToIsoString(QDateTime datetime);

signals:
    void catalogCreated(CatalogRecord catalog);
    void catalogProgressRangeChanged(int minimum, int maximum);
    void catalogProgressValueChanged(int value);
    void catalogProgressTextChanged(const QString &text);

private:
    QString m_connectionName;
    QSqlDatabase m_db;
    bool m_transaction;
    QFutureWatcher<QList<CatalogRecord>> m_catalogWatcher;
    QAtomicInt m_catalogCanceled;
    QList<VolumeThumbRecord> m_volumesCache;
    bool m_volumesDirty;
    QMap<QString, TagRecord> m_tags; // key is 'type_id:lower(name)' e.g. "0:tagname"
    QMap<int, TagRecord *> m_tags2;

    /* Basical */
    bool execQuery(QSqlQuery &query, const QString &statement);
    bool isCatalogCreationCanceled() const { return m_catalogCanceled.loadAcquire() != 0; }
    void transaction();
    void commit();
    void rollback();

    /* Volumes/Files */
    int createVolumeInternal(QString dirpath, int catalog_id, int parent_id = -1);
    void updateVolumeOrders();

    int createVolumesFrontPageOnly(QString dirpath, int catalog_id);
    VolumeWorker createSubVolumesConcurrent(QString dirpath, int volume_id, int parent_id);
    FileWorker createFileRecord(QString filename, QString filepath, int filename_asc);
    FileWorker createFileRecordFromArchive(QString archivePath, ImageContent &ic, int filename_asc);

    /* Catalogs */
    QList<CatalogRecord> callCreateCatalog(const QList<CatalogRecord> &newers);
};

#endif // CATALOGDATABASE_H
