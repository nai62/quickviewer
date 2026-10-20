#ifndef BOUNDEDEXECUTOR_H
#define BOUNDEDEXECUTOR_H

#include <QFuture>
#include <QMutex>
#include <QPromise>
#include <QList>
#include <QSharedPointer>
#include <QThreadPool>

#include <functional>
#include <exception>
#include <type_traits>

class BoundedExecutor
{
public:
    enum class Priority {
        Low = 0,
        Normal,
        High,
        Critical,
    };

    template <typename T>
    struct Submission
    {
        bool accepted = false;
        QFuture<T> future;
    };

    BoundedExecutor(int maximumConcurrency, int maximumPendingJobs);
    ~BoundedExecutor();

    BoundedExecutor(const BoundedExecutor &) = delete;
    BoundedExecutor &operator=(const BoundedExecutor &) = delete;

    template <typename Function, typename T = std::invoke_result_t<std::decay_t<Function>>>
    Submission<T> submit(Function &&function,
                         Priority priority = Priority::Normal,
                         quint64 owner = 0,
                         quint64 generation = 0)
    {
        return submitJob<T>(
            std::forward<Function>(function),
            [](QPromise<T> &promise) { promise.future().cancel(); },
            priority,
            owner,
            generation);
    }

    /**
     * Like submit(), for work that replaces a result the caller already has. A
     * job dropped before it ran - cancelled, rejected or shut down - finishes
     * its future with \a resultIfDropped instead of cancelling it, so dropping
     * the replacement keeps the result it was going to replace.
     */
    template <typename Function, typename T = std::invoke_result_t<std::decay_t<Function>>>
    Submission<T> submitReplacing(Function &&function,
                                  T resultIfDropped,
                                  Priority priority = Priority::Normal,
                                  quint64 owner = 0,
                                  quint64 generation = 0)
    {
        static_assert(!std::is_void_v<T>, "a replacing job needs a result to keep");
        return submitJob<T>(
            std::forward<Function>(function),
            [result = std::move(resultIfDropped)](QPromise<T> &promise) {
                promise.addResult(result);
            },
            priority,
            owner,
            generation);
    }

    void shutdown();
    static void shutdownAll();

    void cancelPendingOlderThan(quint64 owner, quint64 generation);
    void setMaximumConcurrency(int maximumConcurrency);

    int activeCount() const;
    int pendingCount() const;
    int maximumConcurrency() const { return m_maximumConcurrency; }
    int maximumPendingJobs() const { return m_maximumPendingJobs; }

private:
    template <typename T, typename Function, typename Drop>
    Submission<T>
    submitJob(Function &&function, Drop drop, Priority priority, quint64 owner, quint64 generation)
    {
        auto promise = QSharedPointer<QPromise<T>>::create();
        promise->start();
        Submission<T> submission;
        submission.future = promise->future();

        Job job;
        job.priority = priority;
        job.owner = owner;
        job.generation = generation;
        job.run = [promise, function = std::forward<Function>(function)]() mutable {
            try {
                if constexpr (std::is_void_v<T>) {
                    function();
                } else {
                    promise->addResult(function());
                }
            } catch (...) {
                promise->setException(std::current_exception());
            }
            promise->finish();
        };
        job.cancel = [promise, drop = std::move(drop)]() mutable {
            drop(*promise);
            promise->finish();
        };

        submission.accepted = enqueue(std::move(job));
        return submission;
    }

    struct Job
    {
        std::function<void()> run;
        std::function<void()> cancel;
        Priority priority = Priority::Normal;
        quint64 owner = 0;
        quint64 generation = 0;
        quint64 sequence = 0;
    };

    bool enqueue(Job job);
    void launch(Job job);
    void jobFinished();

    mutable QMutex m_mutex;
    QThreadPool m_pool;
    QList<Job> m_pendingJobs;
    int m_activeJobs;
    int m_maximumConcurrency;
    const int m_maximumPendingJobs;
    bool m_acceptingJobs;
    quint64 m_nextSequence = 0;
};

#endif // BOUNDEDEXECUTOR_H
