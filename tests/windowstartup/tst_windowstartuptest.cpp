#include <QtTest>

#include "folderwindow.h"
#include "mainwindow.h"
#include "models/qvapplication.h"

#define FILELOADER_DATAPATH WINDOWSTARTUP_SRCDIR "../fileloader/data/"

class StartupWindow : public ArchiveAwareMainWindow
{
public:
    QList<bool> cloakRequests;

    FolderWindow *folderWindow() const { return m_folderWindow; }
    QSplitter *panelSplitter() const { return findChild<QSplitter *>(QStringLiteral("catalogSplitter")); }
    ViewerSession *viewerSession() { return &m_viewerSession; }
    ImageView *imageView() const { return findChild<ImageView *>(QStringLiteral("graphicsView")); }

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
        qApp->setShowSliderBar(true);
        qApp->setDontSavingHistory(false);
        qApp->clearHistory();
        qApp->setMaxVolumesCache(4);
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

    void destroyingWindowKeepsUiAliveWhileSessionResets()
    {
        QPointer<StartupWindow> viewer = new StartupWindow;

        delete viewer.data();

        QVERIFY(viewer.isNull());
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
        const QList<bool> completedRequests = expectedFullscreen ? QList<bool>{} : QList<bool>{true, false};
        QTRY_COMPARE(viewer.cloakRequests, completedRequests);
        QTRY_COMPARE(viewer.windowOpacity(), qreal(1.0));
        QCOMPARE(viewer.isFullScreen(), expectedFullscreen);
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

    void selectingFolderOnlyEmitsOpenRequest()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("child")));

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        const QString originalPath = folder.currentPath();
        const QString childPath = QDir(originalPath).absoluteFilePath(QStringLiteral("child"));
        QTreeView *view = folder.findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex child = view->model()->index(0, 0);
        QVERIFY(child.isValid());
        QCOMPARE(child.data(Qt::DisplayRole).toString(), QStringLiteral("child"));

        QSignalSpy openVolumeSpy(&folder, &FolderWindow::openVolume);
        folder.handleFolderViewItemSelected(child);

        QCOMPARE(openVolumeSpy.size(), 1);
        QCOMPARE(openVolumeSpy.first().first().toString(), childPath);
        QCOMPARE(folder.currentPath(), originalPath);
    }

    void activeFolderVolumeIsExposedByModelRole()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("child")));

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QTreeView *view = folder.findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex child = view->model()->index(0, 0);
        QVERIFY(child.isValid());
        QVERIFY(!child.data(FolderItemModel::CurrentVolumeRole).toBool());
        QSignalSpy openVolumeSpy(&folder, &FolderWindow::openVolume);

        folder.handleViewerSessionVolumeChanged(
            QDir(directory.path()).absoluteFilePath(QStringLiteral("child")));

        QVERIFY(child.data(FolderItemModel::CurrentVolumeRole).toBool());
        QCOMPARE(openVolumeSpy.size(), 0);
    }

    void folderViewHighlightsCurrentImageFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath(QStringLiteral("current.png"));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(imagePath));

        StartupWindow viewer;
        viewer.loadVolume(directory.path());
        QCOMPARE(
            QDir::cleanPath(QDir::fromNativeSeparators(viewer.viewerSession()->currentPagePath())),
            QDir::cleanPath(QDir::fromNativeSeparators(imagePath)));
        viewer.createFolderWindow(true, directory.path(), false);

        QTreeView *view = viewer.folderWindow()->findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex currentFile = view->model()->index(0, 0);
        QCOMPARE(currentFile.data().toString(), QStringLiteral("current.png"));
        QVERIFY(currentFile.data(FolderItemModel::CurrentVolumeRole).toBool());
    }

    void folderViewConsumesWheelEventsAtScrollBoundary()
    {
        FolderWindow folder(nullptr, nullptr);
        QTreeView *view = folder.findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QVERIFY(view->uniformRowHeights());

        QWheelEvent event(
            QPointF(1, 1),
            QPointF(1, 1),
            QPoint(),
            QPoint(0, -120),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
        event.ignore();
        QApplication::sendEvent(view->viewport(), &event);

        QVERIFY(event.isAccepted());
    }

    void folderWindowLeavesUnhandledFunctionKeysForParent()
    {
        FolderWindow folder(nullptr, nullptr);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier);

        QApplication::sendEvent(&folder, &event);

        QVERIFY(!event.isAccepted());
    }

    void folderViewFocusStillAllowsGlobalFunctionKeys()
    {
        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QTreeView *view = viewer.folderWindow()->findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        viewer.activateWindow();
        QCoreApplication::processEvents();
        view->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(view->hasFocus() || view->viewport()->hasFocus());
        QWidget *focusTarget = QApplication::focusWidget();
        QVERIFY(focusTarget);
        QVERIFY(focusTarget == view || focusTarget == view->viewport());

        QTest::keyPress(focusTarget, Qt::Key_F4);

        QTRY_VERIFY(viewer.folderWindow() == nullptr);
    }

    void historyButtonUsesClockIconAndLabel()
    {
        FolderWindow folder(nullptr, nullptr);
        QToolButton *historyButton = folder.findChild<QToolButton *>(QStringLiteral("historyButton"));
        QVERIFY(historyButton);
        QCOMPARE(historyButton->text(), QStringLiteral("History"));
        QVERIFY(!historyButton->icon().isNull());
        QCOMPARE(historyButton->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    }

    void folderButtonLayoutUsesCompactMargins()
    {
        FolderWindow folder(nullptr, nullptr);
        QFrame *buttonFrame = folder.findChild<QFrame *>(QStringLiteral("frame"));
        QVERIFY(buttonFrame);
        const QMargins margins = buttonFrame->layout()->contentsMargins();
        QCOMPARE(margins, QMargins(4, 4, 4, 4));
        QCOMPARE(buttonFrame->layout()->spacing(), 2);
    }

    void nameSortKeepsArchivesAndImagesInOneFileGroup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("middle-folder")));

        QFile archive(directory.filePath(QStringLiteral("z-last.zip")));
        QVERIFY(archive.open(QIODevice::WriteOnly));
        archive.close();
        QFile image(directory.filePath(QStringLiteral("a-first.png")));
        QVERIFY(image.open(QIODevice::WriteOnly));
        image.close();

        qApp->setFolderSortMode(qvEnums::OrderByName);
        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QTreeView *view = folder.findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);

        QCOMPARE(view->model()->rowCount(), 3);
        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("middle-folder"));
        QCOMPARE(view->model()->index(1, 0).data().toString(), QStringLiteral("a-first.png"));
        QCOMPARE(view->model()->index(2, 0).data().toString(), QStringLiteral("z-last.zip"));
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

    void passwordProtectedArchiveShowsMessageInImageViewWithoutFolderFallback()
    {
        StartupWindow viewer;
        const QString encryptedPath = QString(FILELOADER_DATAPATH "7z/password.7z");
        const QString validArchivePath = QString(FILELOADER_DATAPATH "deflate-utf8.zip");
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QVERIFY(pageSlider);

        viewer.loadVolume(validArchivePath);
        QVERIFY(pageSlider->isEnabled());

        viewer.loadVolume(encryptedPath);

        QCOMPARE(
            viewer.imageView()->displayedMessage(),
            QStringLiteral("Cannot Open Archive\n"
                           "This archive is password-protected.\n\n"
                           "%1")
                .arg(QDir::toNativeSeparators(encryptedPath)));
        QFrame *pageFrame = viewer.findChild<QFrame *>(QStringLiteral("pageFrame"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageFrame);
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageFrame->isHidden());
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("Unavailable"));
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            QVERIFY(qobject_cast<QMessageBox *>(widget) == nullptr);
        }
        QVERIFY(viewer.folderWindow() == nullptr);
        QVERIFY(!qApp->History().contains(encryptedPath));

        viewer.loadVolume(validArchivePath);
        QVERIFY(viewer.imageView()->displayedMessage().isEmpty());
        QVERIFY(pageSlider->isEnabled());
    }

    void archiveWithoutImagesIsActiveAndShowsCentralError()
    {
        StartupWindow viewer;
        const QString archivePath = QString(FILELOADER_DATAPATH "7z/text.7z");
        viewer.createFolderWindow(true, QFileInfo(archivePath).absolutePath(), false);

        viewer.loadVolume(archivePath);

        QTreeView *view = viewer.folderWindow()->findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QModelIndex archiveIndex;
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            const QModelIndex candidate = view->model()->index(row, 0);
            if (candidate.data().toString() == QFileInfo(archivePath).fileName()) {
                archiveIndex = candidate;
                break;
            }
        }
        QVERIFY(archiveIndex.isValid());
        QVERIFY(archiveIndex.data(FolderItemModel::CurrentVolumeRole).toBool());
        QCOMPARE(
            viewer.imageView()->displayedMessage(),
            QStringLiteral("No Viewable Images\n"
                           "No supported images were found in this archive.\n\n"
                           "%1")
                .arg(QDir::toNativeSeparators(archivePath)));
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("No images"));
        QLabel *statusLabel = viewer.findChild<QLabel *>(QStringLiteral("statusLabel"));
        QVERIFY(statusLabel);
        QVERIFY(statusLabel->text().isEmpty());
    }

    void archiveDefersFolderViewUpdatesUntilFirstPaint()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("book.zip"));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), archivePath));
        QVERIFY(QFile::setPermissions(
            archivePath,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner));

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        QTreeView *view = viewer.folderWindow()->findChild<QTreeView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const auto archiveIsCurrent = [view] {
            for (int row = 0; row < view->model()->rowCount(); ++row) {
                const QModelIndex index = view->model()->index(row, 0);
                if (index.data().toString() == QStringLiteral("book.zip")) {
                    return index.data(FolderItemModel::CurrentVolumeRole).toBool();
                }
            }
            return false;
        };
        QVERIFY(!archiveIsCurrent());

        viewer.loadVolume(archivePath);

        QVERIFY(viewer.viewerSession()->initialImagePaintPending());
        QVERIFY(!archiveIsCurrent());

        viewer.viewerSession()->notifyInitialImagePainted();

        QTRY_VERIFY(!viewer.viewerSession()->initialImagePaintPending());
        QTRY_VERIFY(archiveIsCurrent());
    }

    void emptyFolderShowsSpecificMessageAndKeepsPageBar()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        StartupWindow viewer;

        viewer.loadVolume(directory.path());

        QCOMPARE(
            viewer.imageView()->displayedMessage(),
            QStringLiteral("No Viewable Images\n"
                           "No supported images were found in this folder.\n\n"
                           "%1")
                .arg(QDir::toNativeSeparators(directory.path())));
        QFrame *pageFrame = viewer.findChild<QFrame *>(QStringLiteral("pageFrame"));
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageFrame);
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageFrame->isHidden());
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("No images"));
    }

    void brokenImageShowsPathButKeepsVolumeNavigation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath(QStringLiteral("broken.png"));
        QFile image(imagePath);
        QVERIFY(image.open(QIODevice::WriteOnly));
        QCOMPARE(image.write("not an image"), qint64(12));
        image.close();
        StartupWindow viewer;

        viewer.loadVolume(imagePath);

        const QString expected = QStringLiteral("Cannot Display Image\n"
                                                "The image could not be decoded.\n\n"
                                                "%1")
                                     .arg(QDir::toNativeSeparators(imagePath));
        QTRY_COMPARE(viewer.imageView()->displayedMessage(), expected);
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QTRY_VERIFY(pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("(1/1)"));
    }

    void corruptArchiveShowsPathAndDisablesPageBar()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("broken.zip"));
        QFile archive(archivePath);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        QCOMPARE(archive.write("not an archive"), qint64(14));
        archive.close();
        StartupWindow viewer;

        viewer.loadVolume(archivePath);

        const QString message = viewer.imageView()->displayedMessage();
        QVERIFY(message.startsWith(QStringLiteral("Cannot Open Archive\n")));
        QVERIFY(message.contains(QDir::toNativeSeparators(archivePath)));
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("Unavailable"));
    }

    void backgroundPasswordFailureWaitsForForegroundAttempt()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString firstPath = directory.filePath("001.zip");
        const QString secondPath = directory.filePath("002.zip");
        const QString encryptedPath = directory.filePath("003.zip");
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), firstPath));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), secondPath));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "zip/encrypted.zip"), encryptedPath));

        StartupWindow viewer;

        viewer.loadVolume(firstPath);
        QVERIFY(viewer.viewerSession()->nextVolume());
        QVERIFY(viewer.imageView()->displayedMessage().isEmpty());

        QVERIFY(!viewer.viewerSession()->nextVolume());

        QVERIFY(!viewer.imageView()->displayedMessage().isEmpty());
        QVERIFY(!qApp->History().contains(encryptedPath));
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
