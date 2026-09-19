#include "volumecache.h"

#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>

static VolumeLoadFuture makeReadyVolumeFuture(VolumeHandle volume)
{
    QPromise<CachedVolumeLoadResult> promise;
    promise.start();
    VolumeLoadFuture future = promise.future();
    promise.addResult({std::move(volume), ArchiveOpenError::None});
    promise.finish();
    return future;
}

void DeferredVolumeLoadCleanup::operator()(VolumeLoadFuture evictedLoad) const
{
    QThreadPool::globalInstance()->start(
        [evictedLoad = std::move(evictedLoad)]() mutable { evictedLoad.waitForFinished(); });
}

VolumeCache::VolumeCache(int capacity, QObject *parent)
    : QObject(parent),
      m_loads(capacity)
{
}

void VolumeCache::watchFailedLoad(const VolumeCacheKey &key,
                                  quint64 generation,
                                  const VolumeLoadFuture &load)
{
    auto *watcher = new QFutureWatcher<CachedVolumeLoadResult>(this);
    connect(watcher,
            &QFutureWatcher<CachedVolumeLoadResult>::finished,
            this,
            [this, key, generation, watcher] {
                const VolumeLoadFuture finished = watcher->future();
                const bool hasResult = !finished.isCanceled() && finished.resultCount() > 0;
                const CachedVolumeLoadResult result =
                    hasResult ? finished.result() : CachedVolumeLoadResult{};
                const bool failed = !hasResult || !result.volume;
                if (m_generations.value(key, 0) == generation) {
                    if (failed) {
                        if (result.error != ArchiveOpenError::None) {
                            m_recentErrors.insert(key, result.error);
                        }
                        m_loads.remove(key);
                        m_generations.remove(key);
                    } else if (!m_loads.contains(key)) {
                        m_generations.remove(key);
                    }
                }
                watcher->deleteLater();
            });
    watcher->setFuture(load);
}

VolumeLoadFuture VolumeCache::request(const VolumeCacheKey &key, const LoadStarter &startLoad)
{
    if (VolumeLoadFuture *existingLoad = m_loads.find(key)) {
        return *existingLoad;
    }
    if (!startLoad) {
        return {};
    }
    VolumeLoadFuture load = startLoad();
    if (load.isValid()) {
        const quint64 generation = ++m_nextGeneration;
        m_generations.insert(key, generation);
        m_loads.insert(key, load);
        watchFailedLoad(key, generation, load);
    }
    return load;
}

CachedVolumeLoadResult VolumeCache::findReady(const VolumeCacheKey &key)
{
    VolumeLoadFuture *load = m_loads.find(key);
    if (!load || !load->isFinished()) {
        return {};
    }
    if (load->isCanceled() || load->resultCount() == 0) {
        invalidate(key);
        return {};
    }
    const CachedVolumeLoadResult result = load->result();
    if (!result.volume) {
        if (result.error != ArchiveOpenError::None) {
            m_recentErrors.insert(key, result.error);
        }
        m_generations.remove(key);
        m_loads.remove(key);
    }
    return result;
}

ArchiveOpenError VolumeCache::takeFailure(const VolumeCacheKey &key)
{
    const ArchiveOpenError error = m_recentErrors.value(key, ArchiveOpenError::None);
    m_recentErrors.remove(key);
    return error;
}

void VolumeCache::insertReady(const VolumeCacheKey &key, VolumeHandle volume)
{
    m_recentErrors.remove(key);
    const quint64 generation = ++m_nextGeneration;
    m_generations.insert(key, generation);
    m_loads.insert(key, makeReadyVolumeFuture(std::move(volume)));
}

bool VolumeCache::markUsed(const VolumeCacheKey &key)
{
    return m_loads.touch(key);
}

void VolumeCache::invalidate(const VolumeCacheKey &key)
{
    m_generations.remove(key);
    m_recentErrors.remove(key);
    m_loads.remove(key);
}

void VolumeCache::clear()
{
    m_generations.clear();
    m_recentErrors.clear();
    m_loads.clear();
}

bool VolumeCache::contains(const VolumeCacheKey &key) const
{
    return m_loads.contains(key);
}

int VolumeCache::size() const
{
    return m_loads.size();
}
