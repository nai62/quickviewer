#ifndef VOLUMECACHE_H
#define VOLUMECACHE_H

#include <functional>
#include <tuple>

#include <QFuture>
#include <QString>

#include "lrucache.h"
#include "volumehandle.h"

struct VolumeCacheKey
{
    QString normalizedVolumePath;
    bool includeSubfolders = false;
    bool extractSolidArchive = false;

    friend bool operator==(const VolumeCacheKey &lhs, const VolumeCacheKey &rhs)
    {
        return lhs.normalizedVolumePath == rhs.normalizedVolumePath && lhs.includeSubfolders == rhs.includeSubfolders && lhs.extractSolidArchive == rhs.extractSolidArchive;
    }

    friend bool operator<(const VolumeCacheKey &lhs, const VolumeCacheKey &rhs)
    {
        return std::tie(lhs.normalizedVolumePath, lhs.includeSubfolders, lhs.extractSolidArchive) < std::tie(rhs.normalizedVolumePath, rhs.includeSubfolders, rhs.extractSolidArchive);
    }
};

using VolumeLoadFuture = QFuture<VolumeHandle>;

struct DeferredVolumeLoadCleanup
{
    void operator()(VolumeLoadFuture evictedLoad) const;
};

class VolumeCache
{
public:
    using LoadStarter = std::function<VolumeLoadFuture()>;

    explicit VolumeCache(int capacity);

    VolumeLoadFuture request(const VolumeCacheKey &key, const LoadStarter &startLoad);
    VolumeHandle findReady(const VolumeCacheKey &key);
    void insertReady(const VolumeCacheKey &key, VolumeHandle volume);
    bool markUsed(const VolumeCacheKey &key);
    void invalidate(const VolumeCacheKey &key);
    void clear();

    bool contains(const VolumeCacheKey &key) const;
    int size() const;

private:
    LruCache<VolumeCacheKey, VolumeLoadFuture, DeferredVolumeLoadCleanup> m_loads;
};

#endif // VOLUMECACHE_H
