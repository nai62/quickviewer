#ifndef CATALOGDATABASE_H
#define CATALOGDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QtConcurrent>
#include <QImage>

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

class VolumeOrder
{
public:
    int id;
    int parent_id;
    std::wstring realname;
};

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

    /**
     * @brief isImageFile check the file will be a image file
     * @param path
     * @return return true, if path of file maybe a image file
     */
    static bool isImageFile(QString path);
    /**
     * @brief fileSort sort the filenames as current sorting policy
     * @param filenames
     * @return
     */
    static void sortFiles(QStringList &filenames);
    static bool caseInsensitiveLessThan(const QString &s1, const QString &s2);
    static bool caseInsensitiveLessThanWString(const std::wstring &s1, const std::wstring &s2);

    static QString DateTimeToIsoString(QDateTime datetime);

signals:
    void catalogCreated(CatalogRecord catalog);

private:
    QSqlDatabase m_db;
    bool m_transaction;
    QFutureWatcher<QList<CatalogRecord>> m_catalogWatcher;
    int m_catalogWorkProgress;
    int m_catalogWorkMax;
    QList<VolumeThumbRecord> m_volumesCacne;
    bool m_volumesDurty;
    QMap<QString, TagRecord> m_tags; // key is 'type_id:lower(name)' e.g. "0:tagname"
    QMap<int, TagRecord *> m_tags2;

    static QList<QByteArray> st_supportedImageFormats;

    /* Basical */
    bool execInsertQuery(QSqlQuery &query, const QString &tablename);
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
