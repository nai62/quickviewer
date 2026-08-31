#ifndef LATESTRESULTDISPATCHER_H
#define LATESTRESULTDISPATCHER_H

#include <QFuture>
#include <QFutureWatcher>
#include <QPointer>
#include <QThread>

#include <functional>

template<typename T>
class LatestResultDispatcher : public QObject
{
public:
    using RequestId = quint64;
    using ResultHandler = std::function<void(T)>;

    explicit LatestResultDispatcher(QObject *parent = nullptr)
        : QObject(parent)
        , m_requestId(0)
    {
    }

    RequestId submit(const QFuture<T> &future,
                     ResultHandler apply,
                     ResultHandler discard)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        const RequestId requestId = ++m_requestId;
        auto *watcher = new QFutureWatcher<T>();
        const QPointer<LatestResultDispatcher<T>> guard(this);

        QObject::connect(watcher, &QFutureWatcher<T>::finished, watcher,
                         [watcher, guard, requestId,
                          apply = std::move(apply),
                          discard = std::move(discard)]() mutable {
            T result = watcher->result();
            if(guard && guard->isCurrent(requestId))
                apply(std::move(result));
            else
                discard(std::move(result));
            watcher->deleteLater();
        });
        watcher->setFuture(future);
        return requestId;
    }

    void invalidate()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        ++m_requestId;
    }

    bool isCurrent(RequestId requestId) const
    {
        return requestId == m_requestId;
    }

private:
    RequestId m_requestId;
};

#endif // LATESTRESULTDISPATCHER_H
