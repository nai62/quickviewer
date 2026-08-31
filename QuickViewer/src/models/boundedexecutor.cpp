#include "boundedexecutor.h"

#include <QRunnable>

BoundedExecutor::BoundedExecutor(int maximumConcurrency, int maximumPendingJobs)
    : m_activeJobs(0)
    , m_maximumConcurrency(qMax(1, maximumConcurrency))
    , m_maximumPendingJobs(qMax(0, maximumPendingJobs))
    , m_acceptingJobs(true)
{
    m_pool.setMaxThreadCount(m_maximumConcurrency);
}

BoundedExecutor::~BoundedExecutor()
{
    {
        QMutexLocker locker(&m_mutex);
        m_acceptingJobs = false;
    }
    m_pool.waitForDone();
}

bool BoundedExecutor::enqueue(Job job)
{
    bool launchImmediately = false;
    bool rejected = false;
    {
        QMutexLocker locker(&m_mutex);
        if(!m_acceptingJobs) {
            rejected = true;
        } else if(m_activeJobs < m_maximumConcurrency) {
            ++m_activeJobs;
            launchImmediately = true;
        } else if(m_pendingJobs.size() < m_maximumPendingJobs) {
            m_pendingJobs.enqueue(std::move(job));
            return true;
        } else {
            rejected = true;
        }
    }
    if(rejected) {
        job.cancel();
        return false;
    }
    if(launchImmediately)
        launch(std::move(job));
    return true;
}

void BoundedExecutor::launch(Job job)
{
    m_pool.start(QRunnable::create([this, job = std::move(job)]() mutable {
        job.run();
        jobFinished();
    }));
}

void BoundedExecutor::jobFinished()
{
    Job next;
    bool hasNext = false;
    {
        QMutexLocker locker(&m_mutex);
        --m_activeJobs;
        if(!m_pendingJobs.isEmpty()) {
            next = m_pendingJobs.dequeue();
            ++m_activeJobs;
            hasNext = true;
        }
    }
    if(hasNext)
        launch(std::move(next));
}

int BoundedExecutor::activeCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeJobs;
}

int BoundedExecutor::pendingCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_pendingJobs.size();
}
