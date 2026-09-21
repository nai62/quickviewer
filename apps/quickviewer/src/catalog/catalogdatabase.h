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

#include "catalogrecords.h"

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
    void transaction();
    void commit();
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
    QList<CatalogRecord> callCreateCatalog(const QList<CatalogRecord> &newers);
};

#endif // CATALOGDATABASE_H
