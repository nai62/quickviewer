#ifndef VOLUMECACHE_H
#define VOLUMECACHE_H

#include <functional>
#include <tuple>

#include <QFuture>
#include <QMap>
#include <QObject>
#include <QString>

#include "fileloader.h"
#include "lrucache.h"
#include "volumehandle.h"

struct VolumeCacheKey
{
    QString normalizedVolumePath;
    bool includeSubfolders = false;
    bool extractSolidArchive = false;

    friend bool operator==(const VolumeCacheKey &lhs, const VolumeCacheKey &rhs)
    {
        return lhs.normalizedVolumePath == rhs.normalizedVolumePath &&
               lhs.includeSubfolders == rhs.includeSubfolders &&
               lhs.extractSolidArchive == rhs.extractSolidArchive;
    }

    friend bool operator<(const VolumeCacheKey &lhs, const VolumeCacheKey &rhs)
    {
        return std::tie(lhs.normalizedVolumePath, lhs.includeSubfolders, lhs.extractSolidArchive) <
               std::tie(rhs.normalizedVolumePath, rhs.includeSubfolders, rhs.extractSolidArchive);
    }
};

struct CachedVolumeLoadResult
{
    VolumeHandle volume;
    ArchiveOpenError error = ArchiveOpenError::None;
};

using VolumeLoadFuture = QFuture<CachedVolumeLoadResult>;

struct DeferredVolumeLoadCleanup
{
    void operator()(VolumeLoadFuture evictedLoad) const;
};

class VolumeCache : public QObject
{
public:
    using LoadStarter = std::function<VolumeLoadFuture()>;

    explicit VolumeCache(int capacity, QObject *parent = nullptr);

    VolumeLoadFuture request(const VolumeCacheKey &key, const LoadStarter &startLoad);
    CachedVolumeLoadResult findReady(const VolumeCacheKey &key);
    ArchiveOpenError takeFailure(const VolumeCacheKey &key);
    void insertReady(const VolumeCacheKey &key, VolumeHandle volume);
    bool markUsed(const VolumeCacheKey &key);
    void invalidate(const VolumeCacheKey &key);
    void clear();

    bool contains(const VolumeCacheKey &key) const;
    int size() const;

private:
    void
    watchFailedLoad(const VolumeCacheKey &key, quint64 generation, const VolumeLoadFuture &load);

    LruCache<VolumeCacheKey, VolumeLoadFuture, DeferredVolumeLoadCleanup> m_loads;
    QMap<VolumeCacheKey, quint64> m_generations;
    QMap<VolumeCacheKey, ArchiveOpenError> m_recentErrors;
    quint64 m_nextGeneration = 0;
};

#endif // VOLUMECACHE_H
