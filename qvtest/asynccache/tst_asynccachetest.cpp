#include <QtTest>

#include <QElapsedTimer>
#include <QPromise>
#include <QSemaphore>
#include <QWeakPointer>

#include "boundedexecutor.h"
#include "futurecache.h"

class AsyncCacheTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void evictingUnfinishedFutureDoesNotWait();
    void boundsActiveAndPendingJobs();
    void keepsTaskContextAliveUntilCompletion();
};

void AsyncCacheTest::evictingUnfinishedFutureDoesNotWait()
{
    FutureCache<int, int> cache(1);
    QPromise<int> unfinished;
    QPromise<int> replacement;
    unfinished.start();
    replacement.start();
    replacement.addResult(2);
    replacement.finish();
    cache.insert(1, unfinished.future());

    QElapsedTimer timer;
    timer.start();
    cache.insert(2, replacement.future());

    QVERIFY2(timer.elapsed() < 100,
             "Evicting an unfinished future must not wait for its task");
    QVERIFY(!cache.contains(1));
    QVERIFY(cache.contains(2));
    unfinished.addResult(1);
    unfinished.finish();
}

void AsyncCacheTest::boundsActiveAndPendingJobs()
{
    BoundedExecutor executor(2, 2);
    QSemaphore gate;
    QList<QFuture<int>> futures;
    for (int value = 1; value <= 4; ++value) {
        auto submission = executor.submit([&gate, value] {
            gate.acquire();
            return value;
        });
        QVERIFY(submission.accepted);
        futures.append(submission.future);
    }

    QTRY_COMPARE(executor.activeCount(), 2);
    QCOMPARE(executor.pendingCount(), 2);
    auto rejected = executor.submit([] { return 5; });
    QVERIFY(!rejected.accepted);
    QVERIFY(rejected.future.isCanceled());
    QCOMPARE(executor.activeCount(), 2);
    QCOMPARE(executor.pendingCount(), 2);

    gate.release(4);
    for (QFuture<int> &future : futures) {
        future.waitForFinished();
    }
    QTRY_COMPARE(executor.activeCount(), 0);
    QCOMPARE(executor.pendingCount(), 0);
}

void AsyncCacheTest::keepsTaskContextAliveUntilCompletion()
{
    BoundedExecutor executor(1, 1);
    QSemaphore gate;
    QSharedPointer<int> context(new int(42));
    QWeakPointer<int> weakContext(context);
    auto submission = executor.submit([context, &gate] {
        gate.acquire();
        return *context;
    });
    QVERIFY(submission.accepted);

    context.clear();
    QVERIFY(!weakContext.isNull());
    gate.release();
    QCOMPARE(submission.future.result(), 42);
    QTRY_VERIFY(weakContext.isNull());
}

QTEST_GUILESS_MAIN(AsyncCacheTest)

#include "tst_asynccachetest.moc"
