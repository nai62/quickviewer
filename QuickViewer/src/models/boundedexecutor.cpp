#include "boundedexecutor.h"

#include <QRunnable>

BoundedExecutor::BoundedExecutor(int maximumConcurrency, int maximumPendingJobs)
    : m_activeJobs(0),
      m_maximumConcurrency(qMax(1, maximumConcurrency)),
      m_maximumPendingJobs(qMax(0, maximumPendingJobs)),
      m_acceptingJobs(true)
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
        job.sequence = m_nextSequence++;
        if (!m_acceptingJobs) {
            rejected = true;
        } else if (m_activeJobs < m_maximumConcurrency) {
            ++m_activeJobs;
            launchImmediately = true;
        } else if (m_pendingJobs.size() < m_maximumPendingJobs) {
            auto position = m_pendingJobs.begin();
            while (position != m_pendingJobs.end() && (position->priority > job.priority || (position->priority == job.priority && position->sequence < job.sequence))) {
                ++position;
            }
            m_pendingJobs.insert(position, std::move(job));
            return true;
        } else {
            rejected = true;
        }
    }
    if (rejected) {
        job.cancel();
        return false;
    }
    if (launchImmediately) {
        launch(std::move(job));
    }
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
        if (m_activeJobs < m_maximumConcurrency && !m_pendingJobs.isEmpty()) {
            next = m_pendingJobs.takeFirst();
            ++m_activeJobs;
            hasNext = true;
        }
    }
    if (hasNext) {
        launch(std::move(next));
    }
}

void BoundedExecutor::cancelPendingOlderThan(quint64 owner, quint64 generation)
{
    QList<Job> cancelled;
    {
        QMutexLocker locker(&m_mutex);
        for (int i = m_pendingJobs.size() - 1; i >= 0; --i) {
            const Job &job = m_pendingJobs.at(i);
            if (job.owner == owner && job.generation < generation) {
                cancelled.append(m_pendingJobs.takeAt(i));
            }
        }
    }
    for (Job &job : cancelled) {
        job.cancel();
    }
}

void BoundedExecutor::setMaximumConcurrency(int maximumConcurrency)
{
    QList<Job> jobsToLaunch;
    {
        QMutexLocker locker(&m_mutex);
        const int boundedConcurrency = qMax(1, maximumConcurrency);
        if (boundedConcurrency == m_maximumConcurrency) {
            return;
        }
        m_maximumConcurrency = boundedConcurrency;
        m_pool.setMaxThreadCount(m_maximumConcurrency);
        while (m_activeJobs < m_maximumConcurrency && !m_pendingJobs.isEmpty()) {
            jobsToLaunch.append(m_pendingJobs.takeFirst());
            ++m_activeJobs;
        }
    }

    for (Job &job : jobsToLaunch) {
        launch(std::move(job));
    }
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
