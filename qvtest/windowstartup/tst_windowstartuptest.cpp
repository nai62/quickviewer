#include <QtTest>

#include "mainwindow.h"
#include "models/qvapplication.h"

class StartupWindow : public MainWindow
{
public:
    QList<bool> cloakRequests;

protected:
    bool setStartupWindowCloaked(bool cloaked) override
    {
        cloakRequests.append(cloaked);
        return true;
    }
};

class WindowStartupTest : public QObject
{
    Q_OBJECT

private slots:
    void startupCloaking_data()
    {
        QTest::addColumn<bool>("beginAsFullscreen");
        QTest::addColumn<bool>("restoreWindowState");
        QTest::addColumn<int>("savedState");
        QTest::addColumn<bool>("expectedFullscreen");

        QTest::newRow("normal") << false << true << int(Qt::WindowNoState) << false;
        QTest::newRow("maximized") << false << true << int(Qt::WindowMaximized) << false;
        QTest::newRow("explicit-fullscreen") << true << false << int(Qt::WindowNoState) << true;
        QTest::newRow("restored-fullscreen") << false << true << int(Qt::WindowFullScreen) << true;
        QTest::newRow("restoration-disabled") << false << false << int(Qt::WindowFullScreen) << false;
    }

    void startupCloaking()
    {
        QFETCH(bool, beginAsFullscreen);
        QFETCH(bool, restoreWindowState);
        QFETCH(int, savedState);
        QFETCH(bool, expectedFullscreen);

        // Produce the same geometry blob that closing the viewer saves.
        QMainWindow previous;
        previous.resize(800, 600);
        previous.setWindowState(Qt::WindowStates(savedState));
        qApp->setWindowGeometry(previous.saveGeometry());
        qApp->setWindowState(QByteArray());
        qApp->setRestoreWindowState(restoreWindowState);
        qApp->setBeginAsFullscreen(beginAsFullscreen);
        qApp->setStayOnTop(false);
        qApp->setAutoLoaded(false);
        qApp->setShowOptionViewOnStartup(qvEnums::NoViewStartup);

        StartupWindow viewer;
        viewer.initializeStartup();
        QCOMPARE(viewer.isFullScreen(), expectedFullscreen);
        const QList<bool> expectedRequests = expectedFullscreen ? QList<bool>{} : QList<bool>{true};
        QCOMPARE(viewer.cloakRequests, expectedRequests);
        QTRY_COMPARE(viewer.windowOpacity(), qreal(1.0));
        QCOMPARE(viewer.isFullScreen(), expectedFullscreen);
        const QList<bool> completedRequests = expectedFullscreen ? QList<bool>{} : QList<bool>{true, false};
        QCOMPARE(viewer.cloakRequests, completedRequests);
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    // Keep QtTest arguments out of the application's startup file loader.
    int applicationArgc = 1;
    char *applicationArgv[] = {argv[0], nullptr};
    QVApplication application(applicationArgc, applicationArgv);
    WindowStartupTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_windowstartuptest.moc"
