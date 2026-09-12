#include "volumecache.h"

#include <QPromise>
#include <QThreadPool>

static VolumeLoadFuture makeReadyVolumeFuture(VolumeHandle volume)
{
    QPromise<VolumeHandle> promise;
    promise.start();
    VolumeLoadFuture future = promise.future();
    promise.addResult(std::move(volume));
    promise.finish();
    return future;
}

void DeferredVolumeLoadCleanup::operator()(VolumeLoadFuture evictedLoad) const
{
    QThreadPool::globalInstance()->start(
        [evictedLoad = std::move(evictedLoad)]() mutable {
            evictedLoad.waitForFinished();
        });
}

VolumeCache::VolumeCache(int capacity)
    : m_loads(capacity)
{}

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
        m_loads.insert(key, load);
    }
    return load;
}

VolumeHandle VolumeCache::findReady(const VolumeCacheKey &key)
{
    VolumeLoadFuture *load = m_loads.find(key);
    if (!load || !load->isFinished()) {
        return {};
    }
    if (load->isCanceled() || load->resultCount() == 0) {
        m_loads.remove(key);
        return {};
    }
    VolumeHandle volume = load->result();
    if (!volume) {
        m_loads.remove(key);
    }
    return volume;
}

void VolumeCache::insertReady(const VolumeCacheKey &key, VolumeHandle volume)
{
    m_loads.insert(key, makeReadyVolumeFuture(std::move(volume)));
}

bool VolumeCache::markUsed(const VolumeCacheKey &key)
{
    return m_loads.touch(key);
}

void VolumeCache::invalidate(const VolumeCacheKey &key)
{
    m_loads.remove(key);
}

void VolumeCache::clear()
{
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
