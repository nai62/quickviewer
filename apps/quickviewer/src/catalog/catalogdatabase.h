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
    /**
     * True while an asynchronous build is still running, including the
     * rollback of one that was cancelled.
     */
    bool isBuilding() const { return m_catalogWatcher.isRunning(); }

    bool deleteCatalog(int id);
    bool updateCatalogName(int id, QString name);
    bool deleteAllCatalogs();

    /* Volumes */
    QList<VolumeThumbRecord> volumes();
    /**
     * The volumes of one catalog, in the order the catalog lists them, with
     * the tags each of them carries. The manager shows every volume this way,
     * whether or not it has a cover to show in the catalog list; the cover
     * itself is read for the one volume the user selects.
     */
    QList<QPair<VolumeThumbRecord, QStringList>> catalogVolumes(int catalog_id);
    /**
     * The cover stored for \a volume_id, as the JPEG the catalog keeps. Empty
     * for a volume the catalog stored without one.
     */
    QByteArray volumeThumbnail(int volume_id);
    /**
     * Every path a volume is registered under, in the order the catalog holds
     * them. Whether a path is still on disk is not asked here: telling a slow
     * path from a gone one is the caller's to do off the window's thread.
     */
    QStringList volumePaths();
    /**
     * Removes the volumes at \a paths, with the rows that belong to them.
     *
     * @return how many paths were removed.
     */
    int removeVolumes(const QStringList &paths);
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
    /**
     * An asynchronous build is over: every catalog it stored is committed, or
     * a cancelled or failed build has been rolled back. Emitted on the thread
     * that owns this connection, after the worker has closed its own.
     */
    void buildFinished();
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
     * caller that cannot cancel the build when it is nullptr. The result
     * carries \a requestId, so the batch can tell which request it answers.
     */
    CatalogRecord
    createCatalog(QString name, QString path, int requestId, const QAtomicInt *canceled);
};

#endif // CATALOGDATABASE_H
