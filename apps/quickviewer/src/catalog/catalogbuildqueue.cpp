#include "catalogbuildqueue.h"

CatalogBuildQueue::CatalogBuildQueue(QObject *parent)
    : QObject(parent)
{
}

void CatalogBuildQueue::setCatalogDatabase(CatalogDatabase *database)
{
    if (m_database == database) {
        return;
    }
    if (m_database) {
        disconnect(m_database, &CatalogDatabase::catalogCreated, this, nullptr);
    }
    m_database = database;
    if (m_database) {
        connect(m_database,
                &CatalogDatabase::catalogCreated,
                this,
                &CatalogBuildQueue::handleCatalogCreated);
    }
}

int CatalogBuildQueue::add(CatalogRecord request)
{
    request.id = m_nextRequestId--;
    m_requests << request;
    emit changed();
    return request.id;
}

CatalogRecord CatalogBuildQueue::request(int requestId) const
{
    const int index = indexOf(requestId);
    return index < 0 ? CatalogRecord() : m_requests.at(index);
}

void CatalogBuildQueue::update(int requestId, const CatalogRecord &request)
{
    const int index = indexOf(requestId);
    if (index < 0) {
        return;
    }
    m_requests[index] = request;
    m_requests[index].id = requestId;
    emit changed();
}

void CatalogBuildQueue::remove(int requestId)
{
    const int index = indexOf(requestId);
    if (index < 0) {
        return;
    }
    m_requests.removeAt(index);
    emit changed();
}

void CatalogBuildQueue::clear()
{
    if (m_requests.isEmpty()) {
        return;
    }
    m_requests.clear();
    emit changed();
}

void CatalogBuildQueue::start()
{
    if (!m_database || m_requests.isEmpty() || isBuilding()) {
        return;
    }
    m_watcher = m_database->catalogWatcher();
    connect(m_watcher,
            &QFutureWatcher<QList<CatalogRecord>>::finished,
            this,
            &CatalogBuildQueue::handleBuildFinished);
    emit changed();
    m_database->createCatalogAsync(m_requests);
}

void CatalogBuildQueue::stop()
{
    if (!isBuilding() || m_stopping) {
        return;
    }
    m_stopping = true;
    emit changed();
    m_database->cancelCreateCatalogAsync();
}

void CatalogBuildQueue::handleCatalogCreated(const CatalogRecord catalog)
{
    if (!catalog.created) {
        return;
    }
    // The stored catalogue reaches its listener first, so the row that takes
    // the request's place is there when the request leaves. The result names
    // the request it answers, whatever paths the requests were added with.
    emit catalogStored(catalog);
    remove(catalog.requestId);
}

void CatalogBuildQueue::handleBuildFinished()
{
    const bool canceled = m_stopping;
    disconnect(m_watcher, nullptr, this, nullptr);
    m_watcher = nullptr;
    m_stopping = false;
    // What the build stored is announced before the end of the build is, so a
    // listener that reads the catalog again shows the finished result.
    emit buildFinished(canceled);
    emit changed();
}

int CatalogBuildQueue::indexOf(int requestId) const
{
    for (int index = 0; index < m_requests.size(); ++index) {
        if (m_requests.at(index).id == requestId) {
            return index;
        }
    }
    return -1;
}
