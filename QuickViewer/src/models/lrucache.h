#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <QList>
#include <QMap>
#include <QtGlobal>

#include <utility>

struct DefaultEvictionHandler
{
    template <typename Value>
    void operator()(Value &&) const noexcept
    {
    }
};

/**
 * A bounded, non-thread-safe least-recently-used cache.
 *
 * Values are owned by the cache and should express their ownership through
 * RAII. EvictionHandler is invoked after a value is removed by replacement,
 * explicit removal, capacity eviction, or clear().
 */
template <typename Key, typename Value, typename EvictionHandler = DefaultEvictionHandler>
class LruCache
{
public:
    explicit LruCache(int capacity = 30, EvictionHandler evictionHandler = {})
        : m_capacity(qMax(0, capacity)),
          m_evictionHandler(std::move(evictionHandler))
    {
        Q_ASSERT(capacity >= 0);
    }

    LruCache(const LruCache &) = delete;
    LruCache &operator=(const LruCache &) = delete;
    LruCache(LruCache &&) = delete;
    LruCache &operator=(LruCache &&) = delete;

    ~LruCache() { clear(); }

    void insert(const Key &key, Value value)
    {
        remove(key);
        m_values.insert(key, std::move(value));
        m_recencyOrder.push_front(key);
        evictExcessValues();
    }

    bool touch(const Key &key)
    {
        if (!m_values.contains(key)) {
            return false;
        }

        m_recencyOrder.removeOne(key);
        m_recencyOrder.push_front(key);
        return true;
    }

    Value *find(const Key &key)
    {
        auto value = m_values.find(key);
        return value == m_values.end() ? nullptr : &value.value();
    }

    const Value *find(const Key &key) const
    {
        auto value = m_values.constFind(key);
        return value == m_values.cend() ? nullptr : &value.value();
    }

    bool contains(const Key &key) const { return m_values.contains(key); }
    int size() const { return m_values.size(); }
    bool isEmpty() const { return m_values.isEmpty(); }

    void remove(const Key &key)
    {
        auto value = m_values.find(key);
        if (value == m_values.end()) {
            return;
        }

        m_recencyOrder.removeOne(key);
        evict(value);
    }

    void clear()
    {
        m_recencyOrder.clear();
        while (!m_values.isEmpty()) {
            evict(m_values.begin());
        }
    }

private:
    using ValueIterator = typename QMap<Key, Value>::iterator;

    void evict(ValueIterator value)
    {
        Value evictedValue = std::move(value.value());
        m_values.erase(value);
        m_evictionHandler(std::move(evictedValue));
    }

    void evictExcessValues()
    {
        while (m_values.size() > m_capacity) {
            const Key leastRecentlyUsedKey = m_recencyOrder.takeLast();
            auto value = m_values.find(leastRecentlyUsedKey);
            Q_ASSERT(value != m_values.end());
            evict(value);
        }
    }

    QMap<Key, Value> m_values;
    QList<Key> m_recencyOrder;
    int m_capacity;
    EvictionHandler m_evictionHandler;
};

#endif // LRUCACHE_H
