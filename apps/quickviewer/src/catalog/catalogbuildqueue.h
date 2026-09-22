#ifndef CATALOBBUILDQUEUE_H
#define CATALOBBUILDQUEUE_H

#include <QObject>
#include <QList>

#include "catalogdatabase.h"

/**
 * The folders the user added to the catalog manager and has not built yet,
 * and the build that turns them into catalogues.
 *
 * Every request is named by an id of its own from the moment it is added, and
 * the id is what the result of a build carries back: the list the manager
 * shows can be rebuilt around the requests without naming one by its place.
 */
class CatalogBuildQueue : public QObject
{
    Q_OBJECT
public:
    explicit CatalogBuildQueue(QObject *parent = nullptr);

    /** The database the queue builds in; may be nullptr before one is set. */
    void setCatalogDatabase(CatalogDatabase *database);

    const QList<CatalogRecord> &requests() const { return m_requests; }
    /** The request \a requestId, or a record with no name when it is gone. */
    CatalogRecord request(int requestId) const;
    /** Adds \a request and returns the id it was given. */
    int add(CatalogRecord request);
    /** Replaces the request \a requestId with \a request, keeping its id. */
    void update(int requestId, const CatalogRecord &request);
    void remove(int requestId);
    void clear();

    bool isEmpty() const { return m_requests.isEmpty(); }
    /** True while the worker is building the batch it was given. */
    bool isBuilding() const { return m_watcher != nullptr; }
    /** True while a build that was asked to stop is still rolling back. */
    bool isStopping() const { return m_stopping; }

    /** Builds the requests, unless there are none or one is already running. */
    void start();
    /** Asks a running build to stop; the worker rolls back what it wrote. */
    void stop();

signals:
    /** The requests or the state of the build changed. */
    void changed();
    /** One catalogue of the batch was stored. */
    void catalogStored(const CatalogRecord &catalog);
    /** The build is over; \a canceled is true when the user stopped it. */
    void buildFinished(bool canceled);

private slots:
    void handleCatalogCreated(const CatalogRecord catalog);
    void handleBuildFinished();

private:
    int indexOf(int requestId) const;

    CatalogDatabase *m_database = nullptr;
    QList<CatalogRecord> m_requests;
    /** Id of the next request; never an id a catalogue has of its own. */
    int m_nextRequestId = -1;
    /** The build in flight, or nullptr while the queue is not building. */
    QFutureWatcher<QList<CatalogRecord>> *m_watcher = nullptr;
    bool m_stopping = false;
};

#endif // CATALOBBUILDQUEUE_H
