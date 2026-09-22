#ifndef CATALOGDATABASE_H
#define CATALOGDATABASE_H

#include <QAtomicInt>
#include <QDateTime>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QSqlDatabase>
#include <QString>

#include "catalogrecords.h"

class CatalogDatabase : public QObject
{
    Q_OBJECT
public:
    CatalogDatabase(QObject *parent, QString dbpath);
    ~CatalogDatabase() override;

    /**
     * Opens the catalog database, creating it from the database the
     * application bundles when the file does not exist yet. Only that missing
     * file is written: a database that is already there is never replaced.
     *
     * @return true while the catalog schema is available.
     */
    bool ensureReady();
    /**
     * Why opening the catalog database or its most recent write failed.
     */
    QString errorMessage() const { return m_errorMessage; }

    void vacuum();

    /* Catalogs */
    QMap<int, CatalogRecord> catalogs();

    CatalogRecord createCatalog(QString name, QString path);
    QFutureWatcher<QList<CatalogRecord>> *createCatalogAsync(QList<CatalogRecord> newers);
    void cancelCreateCatalogAsync();
    QFutureWatcher<QList<CatalogRecord>> *catalogWatcher() { return &m_catalogWatcher; }

    bool deleteCatalog(int id);
    bool updateCatalogName(int id, QString name);
    bool deleteAllCatalogs();

    /* Volumes */
    QList<VolumeThumbRecord> volumes();
    /**
     * The volumes of one catalog, in the order the catalog lists them, with
     * the tags each of them carries. The manager shows every volume this way,
     * whether or not it has a cover to show in the catalog list.
     */
    QList<QPair<VolumeThumbRecord, QStringList>> catalogVolumes(int catalog_id);
    /**
     * Paths of volumes whose folder or archive is no longer on disk. The
     * catalog keeps them until the user removes them.
     */
    QStringList missingVolumePaths();
    /**
     * Removes every volume which missingVolumePaths() reports, with the rows
     * that belong to it.
     *
     * @return how many paths were removed.
     */
    int removeMissingVolumes();
    /**
     * Sets the title a catalog shows for \a volume_id. The volume keeps its
     * real name: which of the two the list shows is a view option.
     */
    /** Stores the title and tags together, or leaves both unchanged on failure. */
    bool setVolumeDetails(int volume_id, const QString &name, const QStringList &tags);
    bool setVolumeDisplayName(int volume_id, const QString &name);

    /* Tags */
    void loadTags();
    QMap<int, TagRecord *> tagsByCount();
    QList<TagRecord> getTagsFromVolumeId(int volume_id);

signals:
    void catalogCreated(CatalogRecord catalog);
    void catalogProgressRangeChanged(int minimum, int maximum);
    void catalogProgressValueChanged(int value);
    void catalogProgressTextChanged(const QString &text);

private:
    bool writeBundledDatabase(const QFileInfo &file);
    bool hasCatalogSchema(QString *problem);
    void closeDatabase();
    /** Row of the tag \a name, which is created as a normal tag when missing. */
    int findOrCreateTag(const QString &name);
    /** Drops the tags that no volume carries any more. */
    bool removeUnusedTags();
    bool editVolume(int volume_id, const QString &name, const QStringList &tags);

    QString m_dbPath;
    QString m_connectionName;
    QSqlDatabase m_db;
    bool m_connectionRegistered;
    bool m_transaction;
    QFutureWatcher<QList<CatalogRecord>> m_catalogWatcher;
    QAtomicInt m_catalogCanceled;
    bool m_ready;
    QString m_errorMessage;
    QList<VolumeThumbRecord> m_volumesCache;
    bool m_volumesDirty;
    QMap<QString, TagRecord> m_tags; // key is 'type_id:lower(name)' e.g. "0:tagname"
    QMap<int, TagRecord *> m_tags2;

    /* Basical */
    bool execQuery(QSqlQuery &query, const QString &statement);
    bool transaction();
    bool commit();
    void rollback();

    /* Volumes/Files */
    int createVolume(const QString &dirpath, int catalog_id, int parent_id);
    bool updateVolumeOrders();
    int buildCatalogVolumes(const QString &dirpath, int catalog_id, const QAtomicInt *canceled);

    /* Catalogs */
    /**
     * Creates one catalog for the batch that \a canceled belongs to, or for a
     * caller that cannot cancel the build when it is nullptr.
     */
    CatalogRecord createCatalog(QString name, QString path, const QAtomicInt *canceled);
};

#endif // CATALOGDATABASE_H
