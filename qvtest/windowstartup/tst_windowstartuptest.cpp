#include <QtTest>

#include "folderwindow.h"
#include "mainwindow.h"
#include "models/qvapplication.h"

class StartupWindow : public MainWindow
{
public:
    QList<bool> cloakRequests;

    FolderWindow *folderWindow() const { return m_folderWindow; }
    QSplitter *panelSplitter() const { return findChild<QSplitter *>(QStringLiteral("catalogSplitter")); }

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
    void init()
    {
        qApp->setAutoLoaded(false);
        qApp->setShowOptionViewOnStartup(qvEnums::NoViewStartup);
        qApp->setShowPanelSeparateWindow(false);
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(200);
    }

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

    void disabledWidthSavingUsesDefaultWithoutChangingSavedWidth()
    {
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(360);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        QCOMPARE(viewer.folderWindow()->width(), qMax(200, viewer.folderWindow()->minimumWidth()));
        QCOMPARE(qApp->FolderViewWidth(), 360);

        const int displayedWidth = viewer.folderWindow()->width();
        viewer.handleSaveFolderViewWidthActionTriggered(false);
        QCOMPARE(viewer.folderWindow()->width(), displayedWidth);
        QCOMPARE(qApp->FolderViewWidth(), 360);
    }

    void enablingWidthSavingCapturesOnlyVisibleFolderView()
    {
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(360);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        viewer.panelSplitter()->setSizes({275, 515});
        QCoreApplication::processEvents();
        const int displayedWidth = viewer.folderWindow()->width();

        QCOMPARE(qApp->FolderViewWidth(), 360);
        viewer.handleSaveFolderViewWidthActionTriggered(true);
        QCOMPARE(qApp->FolderViewWidth(), displayedWidth);

        viewer.handleSaveFolderViewWidthActionTriggered(false);
        viewer.folderWindow()->hide();
        qApp->setFolderViewWidth(410);
        viewer.handleSaveFolderViewWidthActionTriggered(true);
        QCOMPARE(qApp->FolderViewWidth(), 410);
    }

    void splitterMoveAndCloseSaveActualDockedWidth()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(320);

        StartupWindow viewer;
        viewer.resize(900, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        viewer.panelSplitter()->setSizes({285, 605});
        QCoreApplication::processEvents();
        const int movedWidth = viewer.folderWindow()->width();
        QVERIFY(QMetaObject::invokeMethod(viewer.panelSplitter(), "splitterMoved", Q_ARG(int, movedWidth), Q_ARG(int, 1)));
        QCOMPARE(qApp->FolderViewWidth(), movedWidth);

        viewer.panelSplitter()->setSizes({345, 545});
        QCoreApplication::processEvents();
        const int finalWidth = viewer.folderWindow()->width();
        QVERIFY(finalWidth != movedWidth);
        QCOMPARE(qApp->FolderViewWidth(), movedWidth);
        viewer.close();
        QCOMPARE(qApp->FolderViewWidth(), finalWidth);
    }

    void restoredWidthIsConstrainedBySplitter()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(10000);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        QVERIFY(viewer.folderWindow()->width() < 10000);
        QVERIFY(viewer.folderWindow()->width() >= viewer.folderWindow()->minimumWidth());
        QVERIFY(viewer.panelSplitter()->sizes().constLast() >= 0);
    }

    void fullscreenWidthIsSavedOnClose()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(300);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        viewer.handleFullscreenActionTriggered();
        QCoreApplication::processEvents();
        const int fullscreenWidth = viewer.folderWindow()->width();
        viewer.close();

        QCOMPARE(qApp->FolderViewWidth(), fullscreenWidth);
    }

    void enabledWidthSavingRestoresSavedWidth()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(310);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        QCOMPARE(viewer.folderWindow()->width(), 310);
        QCOMPARE(qApp->FolderViewWidth(), 310);
    }

    void minimumWidthDoesNotDependOnFolderContents()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString longName(180, QLatin1Char('x'));
        QVERIFY(QDir(directory.path()).mkdir(longName));

        StartupWindow viewer;
        viewer.createFolderWindow(true, QString(), true);
        const int uiMinimumWidth = viewer.folderWindow()->minimumWidth();
        viewer.folderWindow()->setFolderPath(directory.path(), false);

        QCOMPARE(viewer.folderWindow()->minimumWidth(), uiMinimumWidth);
    }

    void separateWindowUsesSharedSavedWidthOnEnableAndExit()
    {
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(330);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(false, QString(), true);
        QCoreApplication::processEvents();
        viewer.folderWindow()->resize(420, viewer.folderWindow()->height());
        QCoreApplication::processEvents();

        QCOMPARE(qApp->FolderViewWidth(), 330);
        viewer.handleSaveFolderViewWidthActionTriggered(true);
        QCOMPARE(qApp->FolderViewWidth(), viewer.folderWindow()->width());

        viewer.folderWindow()->resize(460, viewer.folderWindow()->height());
        QCoreApplication::processEvents();
        QCOMPARE(qApp->FolderViewWidth(), viewer.folderWindow()->width());
        viewer.close();
        QCOMPARE(qApp->FolderViewWidth(), 460);
        viewer.handleFolderWindowClosed();
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
