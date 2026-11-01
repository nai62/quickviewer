#include <QtTest>

#include "prefetchplanner.h"

class PrefetchPlannerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesInitialDisplayPlan();
    void preservesNormalPlan();
    void preservesForwardPlan();
    void preservesBackwardPlan();
    void preservesFastForwardPlan();
    void providesFastBackwardPlan();
    void respectsCacheCapacity_data();
    void respectsCacheCapacity();
    void excludesIndexesBeforeFirstPage();
    void excludesIndexesAfterLastPage();
    void decodesTheWindowAndTheFastTurnLanding_data();
    void decodesTheWindowAndTheFastTurnLanding();
};

void PrefetchPlannerTest::preservesInitialDisplayPlan()
{
    QCOMPARE(PrefetchPlanner::offsets(PrefetchMode::InitialDisplay, 22), QList<int>({0}));
}

void PrefetchPlannerTest::preservesNormalPlan()
{
    QCOMPARE(PrefetchPlanner::offsets(PrefetchMode::Normal, 22),
             QList<int>({0, 1, 2, 3, -1, -2, 4, 5, -3, -4, 6, 7, -5, -6}));
}

void PrefetchPlannerTest::preservesForwardPlan()
{
    QCOMPARE(PrefetchPlanner::offsets(PrefetchMode::NormalForward, 22),
             QList<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
}

void PrefetchPlannerTest::preservesBackwardPlan()
{
    QCOMPARE(PrefetchPlanner::offsets(PrefetchMode::NormalBackward, 22),
             QList<int>({0, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, 1}));
}

void PrefetchPlannerTest::preservesFastForwardPlan()
{
    QCOMPARE(PrefetchPlanner::offsets(PrefetchMode::FastForward, 22),
             QList<int>({0, 1, 10, 11, -10, -9, 20, 21, -20, -19}));
}

void PrefetchPlannerTest::providesFastBackwardPlan()
{
    QCOMPARE(PrefetchPlanner::offsets(PrefetchMode::FastBackward, 22),
             QList<int>({0, 1, -10, -9, 10, 11, -20, -19, 20, 21}));
}

void PrefetchPlannerTest::respectsCacheCapacity_data()
{
    QTest::addColumn<PrefetchMode>("mode");
    QTest::addColumn<int>("capacity");
    QTest::addColumn<QList<int>>("expected");

    QTest::newRow("initial-display-6") << PrefetchMode::InitialDisplay << 6 << QList<int>({0});
    QTest::newRow("normal-6") << PrefetchMode::Normal << 6 << QList<int>({0, 1, 2, 3, -1, -2});
    QTest::newRow("forward-6") << PrefetchMode::NormalForward << 6
                               << QList<int>({0, 1, 2, 3, 4, 5});
    QTest::newRow("backward-6") << PrefetchMode::NormalBackward << 6
                                << QList<int>({0, -1, -2, -3, -4, 1});
    QTest::newRow("fast-forward-6")
        << PrefetchMode::FastForward << 6 << QList<int>({0, 1, 10, 11, -10, -9});
    QTest::newRow("fast-backward-6")
        << PrefetchMode::FastBackward << 6 << QList<int>({0, 1, -10, -9, 10, 11});
    QTest::newRow("normal-minimum-dual-page") << PrefetchMode::Normal << 2 << QList<int>({0, 1});
    QTest::newRow("forward-minimum-dual-page")
        << PrefetchMode::NormalForward << 2 << QList<int>({0, 1});
    QTest::newRow("backward-minimum-dual-page")
        << PrefetchMode::NormalBackward << 2 << QList<int>({0, 1});
    QTest::newRow("fast-minimum-dual-page") << PrefetchMode::FastForward << 2 << QList<int>({0, 1});
    QTest::newRow("fast-backward-minimum-dual-page")
        << PrefetchMode::FastBackward << 2 << QList<int>({0, 1});
}

void PrefetchPlannerTest::respectsCacheCapacity()
{
    QFETCH(PrefetchMode, mode);
    QFETCH(int, capacity);
    QFETCH(QList<int>, expected);

    const QList<int> actual = PrefetchPlanner::offsets(mode, capacity);
    QCOMPARE(actual, expected);
    QVERIFY(actual.size() <= capacity);
}

void PrefetchPlannerTest::excludesIndexesBeforeFirstPage()
{
    QCOMPARE(PrefetchPlanner::indexes(PrefetchMode::Normal, 0, 4, 22), QList<int>({0, 1, 2, 3}));
}

void PrefetchPlannerTest::excludesIndexesAfterLastPage()
{
    QCOMPARE(PrefetchPlanner::indexes(PrefetchMode::Normal, 3, 4, 22), QList<int>({3, 2, 1, 0}));
}

void PrefetchPlannerTest::decodesTheWindowAndTheFastTurnLanding_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<QList<int>>("decodedOffsets");
    QTest::newRow("normal") << int(PrefetchMode::Normal) << QList<int>({0, 1, 2, 3, -1, -2, -3});
    QTest::newRow("forward") << int(PrefetchMode::NormalForward) << QList<int>({0, 1, 2, 3});
    QTest::newRow("backward") << int(PrefetchMode::NormalBackward)
                              << QList<int>({0, -1, -2, -3, 1});
    // A fast turn lands ten pages on, and a spread there shows the next page.
    QTest::newRow("fast forward") << int(PrefetchMode::FastForward) << QList<int>({0, 1, 10, 11});
    QTest::newRow("fast backward")
        << int(PrefetchMode::FastBackward) << QList<int>({0, 1, -10, -9});
}

void PrefetchPlannerTest::decodesTheWindowAndTheFastTurnLanding()
{
    QFETCH(int, mode);
    QFETCH(QList<int>, decodedOffsets);
    constexpr int Current = 50;
    const QList<PrefetchTarget> targets =
        PrefetchPlanner::targets(PrefetchMode(mode), Current, 100, 22);

    QList<int> plannedIndexes;
    QList<int> decoded;
    for (const PrefetchTarget &target : targets) {
        plannedIndexes.append(target.index);
        if (target.depth == PrefetchDepth::Decode) {
            decoded.append(target.index - Current);
        }
    }
    // The depth only qualifies the plan; it neither adds nor drops a page.
    QCOMPARE(plannedIndexes, PrefetchPlanner::indexes(PrefetchMode(mode), Current, 100, 22));
    QCOMPARE(decoded, decodedOffsets);
}

QTEST_APPLESS_MAIN(PrefetchPlannerTest)

#include "tst_prefetchplannertest.moc"
