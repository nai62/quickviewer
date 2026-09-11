#ifndef BOUNDEDEXECUTOR_H
#define BOUNDEDEXECUTOR_H

#include <QFuture>
#include <QMutex>
#include <QPromise>
#include <QQueue>
#include <QSharedPointer>
#include <QThreadPool>

#include <functional>
#include <exception>
#include <type_traits>

class BoundedExecutor
{
public:
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

    template <typename Function,
              typename T = std::invoke_result_t<std::decay_t<Function>>>
    Submission<T> submit(Function &&function)
    {
        auto promise = QSharedPointer<QPromise<T>>::create();
        promise->start();
        Submission<T> submission;
        submission.future = promise->future();

        Job job;
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
        job.cancel = [promise] {
            promise->future().cancel();
            promise->finish();
        };

        submission.accepted = enqueue(std::move(job));
        return submission;
    }

    int activeCount() const;
    int pendingCount() const;
    int maximumConcurrency() const { return m_maximumConcurrency; }
    int maximumPendingJobs() const { return m_maximumPendingJobs; }

private:
    struct Job
    {
        std::function<void()> run;
        std::function<void()> cancel;
    };

    bool enqueue(Job job);
    void launch(Job job);
    void jobFinished();

    mutable QMutex m_mutex;
    QThreadPool m_pool;
    QQueue<Job> m_pendingJobs;
    int m_activeJobs;
    const int m_maximumConcurrency;
    const int m_maximumPendingJobs;
    bool m_acceptingJobs;
};

#endif // BOUNDEDEXECUTOR_H
