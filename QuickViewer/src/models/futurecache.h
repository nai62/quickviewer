#ifndef FUTURECACHE_H
#define FUTURECACHE_H

#include <QFuture>
#include <QList>
#include <QMap>

template <typename Key, typename T>
class FutureCache
{
public:
    explicit FutureCache(int maximumSize = 30)
        : m_maximumSize(maximumSize)
    {
    }

    void insert(const Key &key, const QFuture<T> &future)
    {
        checkShouldBeInserted(key);
        insertNoChecked(key, future);
    }

    void insertNoChecked(const Key &key, const QFuture<T> &future)
    {
        m_cache.insert(key, future);
        while (m_cache.size() > m_maximumSize) {
            const Key oldest = m_usageOrder.takeLast();
            m_cache.remove(oldest);
        }
    }

    void remove(const Key &key)
    {
        m_usageOrder.removeOne(key);
        m_cache.remove(key);
    }

    bool checkShouldBeInserted(const Key &key)
    {
        const bool containsKey = m_usageOrder.contains(key);
        if (containsKey) {
            m_usageOrder.removeOne(key);
        }
        m_usageOrder.push_front(key);
        return !containsKey;
    }

    void retain(const Key &key)
    {
        if (m_usageOrder.removeOne(key)) {
            m_usageOrder.push_front(key);
        }
    }

    int size() const { return m_cache.size(); }
    bool contains(const Key &key) const { return m_cache.contains(key); }
    QFuture<T> &object(const Key &key) { return m_cache[key]; }

    void clear()
    {
        m_cache.clear();
        m_usageOrder.clear();
    }

private:
    QMap<Key, QFuture<T>> m_cache;
    QList<Key> m_usageOrder;
    int m_maximumSize;
};

#endif // FUTURECACHE_H
