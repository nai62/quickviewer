#include <QtConcurrent>
#include <QtTest>

#include "latestresultdispatcher.h"

class LatestResultDispatcherTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesResultOnOwningThread();
    void discardsOlderRequestResult();
    void discardsResultAfterOwnerIsDestroyed();
};

void LatestResultDispatcherTest::appliesResultOnOwningThread()
{
    LatestResultDispatcher<int> dispatcher;
    bool applied = false;
    QThread *callbackThread = nullptr;

    dispatcher.submit(QtConcurrent::run([] { return 42; }), [&](int result) {
            QCOMPARE(result, 42);
            callbackThread = QThread::currentThread();
            applied = true; }, [](int) { QFAIL("The current result must not be discarded"); });

    QTRY_VERIFY(applied);
    QCOMPARE(callbackThread, dispatcher.thread());
}

void LatestResultDispatcherTest::discardsOlderRequestResult()
{
    LatestResultDispatcher<int *> dispatcher;
    QPromise<int *> first;
    QPromise<int *> second;
    first.start();
    second.start();
    int applied = 0;
    int discarded = 0;
    auto apply = [&](int *result) {
        applied = *result;
        delete result;
    };
    auto discard = [&](int *result) {
        discarded = *result;
        delete result;
    };

    dispatcher.submit(first.future(), apply, discard);
    dispatcher.submit(second.future(), apply, discard);

    second.addResult(new int(2));
    second.finish();
    QTRY_COMPARE(applied, 2);

    first.addResult(new int(1));
    first.finish();
    QTRY_COMPARE(discarded, 1);
    QCOMPARE(applied, 2);
}

void LatestResultDispatcherTest::discardsResultAfterOwnerIsDestroyed()
{
    QPromise<int *> promise;
    promise.start();
    bool applied = false;
    int discarded = 0;
    auto *dispatcher = new LatestResultDispatcher<int *>();
    dispatcher->submit(promise.future(), [&](int *result) {
            applied = true;
            delete result; }, [&](int *result) {
            discarded = *result;
            delete result; });

    delete dispatcher;
    promise.addResult(new int(7));
    promise.finish();

    QTRY_COMPARE(discarded, 7);
    QVERIFY(!applied);
}

QTEST_GUILESS_MAIN(LatestResultDispatcherTest)

#include "tst_latestresultdispatchertest.moc"
