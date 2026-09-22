#include <QtWidgets>

#include "mainwindow.h"
#include "imageview.h"
#include "models/shadereffect.h"
#include "ui_mainwindow.h"
#include "fileloaderdirectory.h"
#include "qvenums.h"
#include "qvapplication.h"
#include "keyconfigdialog.h"
#include "mouseconfigdialog.h"
#include "optionsdialog.h"
#include "catalogwindow.h"
#include "folderwindow.h"
#include "renamedialog.h"
#include "exifdialog.h"
#include "qmousesequence.h"
#include "fileloader.h"
#include "innerframe.h"
#include "retouchwindow.h"
#include "startupprofiler.h"
#include "storedvolumelocation.h"
#include "benchmark/startupfoldertextprofile.h"
#include "folderitemmodel.h"

#ifdef Q_OS_WIN
#    include "fileassocdialog.h"
#endif

namespace {

// Shown in the About dialog.
const QString AppCopyright = QStringLiteral("Copyright 2017 KATO Kanryu");

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_viewerWindowStateMaximized(false),
      m_sliderChanging(false),
      m_onWindowClosing(false),
      m_revealInitialWindow(true),
      m_startupWindowCloaked(false),
      m_startupPanelInitialized(false),
      m_deferredMenusInitialized(false)
      //    , contextMenu(this)
      ,
      m_viewerSession(this),
      m_catalogDatabase(nullptr),
      m_startupPanelPlaceholder(nullptr),
      m_folderWindow(nullptr),
      m_catalogWindow(nullptr),
      m_retouchWindow(nullptr),
      m_exifDialog(nullptr),
      m_fullscreenButton(nullptr),
      m_statusMessage(StatusMessage::NoVolume)
{
    ui->setupUi(this);
    StartupProfiler::mark("mainwindow.ui-setup");
    // Establish the final window size before further UI initialization can
    // expose child surfaces created with the designer geometry.
    if (!qApp->BeginAsFullscreen() && qApp->RestoreWindowState()) {
        restoreGeometry(qApp->WindowGeometry());
    }
#ifndef Q_OS_WIN
    setWindowOpacity(0.0);
#endif

    connect(ui->catalogSplitter, &QSplitter::splitterMoved, this, [this]() {
        if (!m_folderWindow || m_folderWindow->parentWidget() != ui->catalogSplitter) {
            return;
        }
        saveVisibleFolderViewWidth();
    });

    m_menubarFontSize = ui->menuBar->font().pointSize();
    m_pageSliderHeight = ui->pageSlider->height();
    m_imageString.initialize(&m_viewerSession,
                             [view = ui->graphicsView] { return view->renderedPageMetrics(); });

#ifndef Q_OS_WIN
    ui->actionRegisterFileAssociations->setVisible(false);
#endif

#ifdef Q_OS_MACOS
    ui->menuBar->setNativeMenuBar(true);
    ui->actionOpenOptionsDialog->setMenuRole(QAction::PreferencesRole);
    ui->actionAppVersion->setMenuRole(QAction::AboutRole);
    ui->actionExit->setMenuRole(QAction::QuitRole);
    ui->actionOpenKeyConfig->setMenuRole(QAction::NoRole);
    ui->actionOpenMouseConfig->setMenuRole(QAction::NoRole);
    ui->actionCheckVersion->setMenuRole(QAction::ApplicationSpecificRole);
#endif

    ui->graphicsView->setViewerSession(&m_viewerSession);
    connect(&m_viewerSession,
            &ViewerSession::initialImageDisplayFinished,
            this,
            &MainWindow::handleInitialImageDisplayFinished);
    connect(&m_viewerSession,
            &ViewerSession::loadStatusChanged,
            this,
            &MainWindow::handleViewerLoadStatusChanged);
    connect(&m_viewerSession,
            &ViewerSession::archiveOpenFailed,
            this,
            [this](const QString &path, ArchiveOpenError) {
                m_folderViewRequestedPath = QDir::fromNativeSeparators(path);
                if (m_folderWindow) {
                    m_folderWindow->handleViewerSessionVolumeChanged(m_folderViewRequestedPath);
                }
            });
    setAcceptDrops(true);

    // Mapping to Key-Action Table and Key Config Dialog
    qApp->registerActions(ui);
    StartupProfiler::mark("mainwindow.actions-registered");
    resetShortcutKeys();

    // Context menus(independent from menuBar)
    ui->menuBar->removeAction(ui->menuContextMenu->menuAction());
    m_contextMenu = ui->menuContextMenu;

    // setup checkable menus
    ui->actionFitting->setChecked(qApp->Fitting());
    ui->graphicsView->handleFittingActionTriggered(qApp->Fitting());
    switch (qApp->ImageSortBy()) {
    case qvEnums::ImageSortBy::SortByFileName:
        ui->actionSortByFileName->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByFileNameDescending:
        ui->actionSortByFileNameDescending->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByFileSize:
        ui->actionSortByFileSize->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByFileSizeDescending:
        ui->actionSortByFileSizeDescending->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByModifiedTime:
        ui->actionSortByModifiedTime->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByModifiedTimeDescending:
        ui->actionSortByModifiedTimeDescending->setChecked(true);
        break;
    }
    m_sortByMenuGroup << ui->actionSortByFileName << ui->actionSortByFileNameDescending
                      << ui->actionSortByFileSize << ui->actionSortByFileSizeDescending
                      << ui->actionSortByModifiedTime << ui->actionSortByModifiedTimeDescending;
    switch (qApp->ImageFitMode()) {
    case qvEnums::FitMode::FitToRect:
        ui->actionFitToWindow->setChecked(true);
        break;
    case qvEnums::FitMode::FitToWidth:
        ui->actionFitToWidth->setChecked(true);
        break;
    default:
        break;
    }

    ui->actionDualView->setChecked(qApp->DualView());
    ui->graphicsView->handleDualViewActionTriggered(qApp->DualView());

    ui->actionStayOnTop->setChecked(qApp->StayOnTop());

    m_fullscreenButton = new QToolButton(this);
    m_fullscreenButton->setToolTip(tr("&Fullscreen"));
    m_fullscreenButton->setCheckable(true);
    m_fullscreenButton->setIcon(QIcon(":/icons/fullscreen"));
    connect(m_fullscreenButton,
            &QToolButton::clicked,
            this,
            &MainWindow::handleFullscreenActionTriggered);
    connect(ui->actionFullscreen, &QAction::toggled, m_fullscreenButton, &QToolButton::setChecked);
    ui->menuBar->setCornerWidget(m_fullscreenButton);

    ui->actionLargeToolbarIcons->setChecked(qApp->LargeToolbarIcons());
    handleLargeToolbarIconsActionTriggered(qApp->LargeToolbarIcons());

    ui->graphicsView->handleRightSideBookActionTriggered(qApp->RightSideBook());
    ui->actionRightSideBook->setChecked(qApp->RightSideBook());
    ui->actionLoupeTool->setChecked(qApp->LoupeTool());
    ui->actionScrollWithCursorWhenZooming->setChecked(qApp->ScrollWithCursorWhenZooming());

    ui->actionWideImageAsOneView->setChecked(qApp->WideImageAsOnePageInDualView());
    ui->actionFirstImageAsOneView->setChecked(qApp->FirstImageAsOnePageInDualView());
    ui->actionSeparatePagesWhenWideImage->setChecked(qApp->SeparatePagesWhenWideImage());

    ui->actionShowSubfolders->setChecked(qApp->ShowSubfolders());

    ui->actionAutoLoaded->setChecked(qApp->AutoLoaded());
    ui->actionSavingHistory->setChecked(qApp->DontSavingHistory());

    ui->actionDontEnlargeSmallImagesOnFitting->setChecked(qApp->DontEnlargeSmallImagesOnFitting());
    ui->actionRestoreWindowState->setChecked(qApp->RestoreWindowState());
    ui->actionBeginAsFullscreen->setChecked(qApp->BeginAsFullscreen());
    ui->actionShowFullscreenSignage->setChecked(qApp->ShowFullscreenSignage());
    ui->actionShowPanelSeparateWindow->setChecked(qApp->ShowPanelSeparateWindow());
    ui->actionHideMouseCursorInFullscreen->setChecked(qApp->HideMouseCursorInFullscreenForMenu());
    //    ui->actionShowFullscreenTitleBar->setChecked(qApp->ShowFullscreenTitleBar());

    // Languages
    connect(qApp->languageSelector(),
            &LanguageManager::languageChanged,
            this,
            &MainWindow::handleLanguageSelectorLanguageChanged);
    connect(qApp->languageSelector(),
            &LanguageManager::openTextEditorForLanguage,
            this,
            &MainWindow::handleLanguageSelectorOpenTextEditorForLanguage);

    // ToolBar/PageBar/StatusBar/MenuBar
    ui->actionShowToolBar->setChecked(qApp->ShowToolBar());
    handleShowToolBarActionTriggered(qApp->ShowToolBar());
    ui->actionShowPageBar->setChecked(qApp->ShowSliderBar());
    handleShowPageBarActionTriggered(qApp->ShowSliderBar());
    ui->actionShowStatusBar->setChecked(qApp->ShowStatusBar());
    handleShowStatusBarActionTriggered(qApp->ShowStatusBar());
    ui->actionShowMenuBar->setChecked(qApp->ShowMenuBar());
    if (!qApp->ShowMenuBar()) {
        menuBar()->hide();
    }
    // Keep the configured page bar in the initial window layout. Its empty
    // state reserves the same space that the completed folder scan will use,
    // so displaying the volume does not move the already rendered image.

    // History
    connect(ui->menuHistory, &QMenu::triggered, this, &MainWindow::handleHistoryMenuTriggered);

    // Bookmarks
    ui->actionLoadBookmark->setMenu(ui->menuLoadBookmark);
    connect(ui->menuLoadBookmark,
            &QMenu::triggered,
            this,
            &MainWindow::handleLoadBookmarkMenuTriggered);

    // Folders
    ui->actionOpenVolumeWithProgress->setChecked(qApp->OpenVolumeWithProgress());
    ui->actionShowReadProgress->setChecked(qApp->ShowReadProgress());
    ui->actionSaveReadProgress->setChecked(qApp->SaveReadProgress());
    ui->actionSaveFolderViewWidth->setChecked(qApp->SaveFolderViewWidth());

    // Catalogs
    connect(ui->actionManageCatalogs,
            &QAction::triggered,
            this,
            &MainWindow::handleManageCatalogsActionTriggered);
    ui->actionCatalogIconLongText->setChecked(qApp->IconLongText());
    ui->actionSearchTitleWithOptions->setChecked(qApp->SearchTitleWithOptions());
    ui->actionCatalogTitleWithoutOptions->setChecked(qApp->TitleWithoutOptions());
    ui->actionShowTagBar->setChecked(qApp->ShowTagBar());
    ui->actionSaveCatalogViewWidth->setChecked(qApp->SaveCatalogViewWidth());

    switch (qApp->CatalogViewModeSetting()) {
    case qvEnums::CatalogViewMode::List:
        ui->actionCatalogViewList->setChecked(true);
        break;
    case qvEnums::CatalogViewMode::Icon:
        ui->actionCatalogViewIcon->setChecked(true);
        break;
    case qvEnums::CatalogViewMode::IconNoText:
        ui->actionCatalogViewIconNoText->setChecked(true);
        break;
    }

    ui->statusBar->addPermanentWidget(ui->statusLabel);
    StartupProfiler::mark("mainwindow.initial-message.begin");
    setStatusMessage(StatusMessage::NoVolume);
    StartupProfiler::mark("mainwindow.initial-message.end");
    StartupProfiler::mark("mainwindow.page-bar-sync.begin");
    syncPageBar();
    StartupProfiler::mark("mainwindow.page-bar-sync.end");

    // Shader
    m_shaderMenuGroup << ui->actionShaderNearestNeighbor << ui->actionShaderBilinear
                      << ui->actionShaderCpuBicubic << ui->actionShaderCpuSpline16
                      << ui->actionShaderCpuSpline36 << ui->actionShaderCpuLanczos3
                      << ui->actionShaderCpuLanczos4;
    switch (qApp->Effect()) {
    case qvEnums::ShaderEffect::NearestNeighbor:
        ui->actionShaderNearestNeighbor->setChecked(true);
        break;
    case qvEnums::ShaderEffect::Bilinear:
        ui->actionShaderBilinear->setChecked(true);
        break;
    case qvEnums::ShaderEffect::CpuBicubic:
        ui->actionShaderCpuBicubic->setChecked(true);
        break;
    case qvEnums::ShaderEffect::CpuSpline16:
        ui->actionShaderCpuSpline16->setChecked(true);
        break;
    case qvEnums::ShaderEffect::CpuSpline36:
        ui->actionShaderCpuSpline36->setChecked(true);
        break;
    case qvEnums::ShaderEffect::CpuLanczos3:
        ui->actionShaderCpuLanczos3->setChecked(true);
        break;
    case qvEnums::ShaderEffect::CpuLanczos4:
        ui->actionShaderCpuLanczos4->setChecked(true);
        break;
    default:
        break;
    }
#ifndef QV_WITH_LUMINOR
    ui->actionShowRetouchWindow->setVisible(false);
#endif
    ui->graphicsView->installEventFilter(this);
    ui->mainToolBar->installEventFilter(this);
    ui->pageFrame->installEventFilter(this);

    connect(&m_viewerSession,
            &ViewerSession::pageChanged,
            this,
            &MainWindow::handleViewerSessionPageChanged);
    connect(&m_viewerSession,
            &ViewerSession::volumeChanged,
            this,
            &MainWindow::handleViewerSessionVolumeChanged);
    connect(ui->graphicsView,
            &ImageView::scrollModeChanged,
            this,
            &MainWindow::handleScrollModeChanged);
    connect(ui->graphicsView,
            &ImageView::zoomingChanged,
            this,
            &MainWindow::handleViewerSessionPageChanged);
    connect(ui->graphicsView,
            &ImageView::fittingChanged,
            this,
            &MainWindow::handleGraphicsViewFittingChanged);
    connect(
        ui->graphicsView, &ImageView::slideShowStopped, this, &MainWindow::handleSlideShowStopped);

    setWindowTitle(QString("%1 v%2").arg(qApp->applicationName()).arg(qApp->applicationVersion()));
}

void MainWindow::initializeStartup()
{
    // Warm the volume the startup will open while the window is still being
    // created. loadStartupVolume() opens the same target later, so this only
    // fills the volume cache and leaves the startup sequence unchanged.
    prefetchStartupTarget();
    // The panel asks the shell for its list icons; warming them here keeps that
    // work off the moment the window appears.
    FolderItemModel::startIconLoad();

    // restoreGeometry() in the constructor can restore fullscreen even when
    // the explicit "Begin as fullscreen" option is disabled.
    const bool startFullscreen = qApp->BeginAsFullscreen() || isFullScreen();
    const bool stayOnTop = qApp->StayOnTop();
    if (stayOnTop) {
        // Apply the portable flag before the native window is created.
        setWindowFlag(Qt::WindowStaysOnTopHint);
    }

    // Restore all non-native state before creating the startup window.
    if (startFullscreen) {
        if (qApp->HideMouseCursorInFullscreen()) {
            ui->graphicsView->setCursor(Qt::BlankCursor);
        }
    } else if (qApp->RestoreWindowState()) {
        restoreState(qApp->WindowState());
    }
    StartupProfiler::mark("startup.window-state-restored");

    if (startFullscreen) {
        // Let Windows observe the native fullscreen transition. Creating and
        // showing it while DWM-cloaked leaves the taskbar above the window.
        showFullScreen();
    } else {
        // Cloaking keeps a normal startup window out of DWM composition until
        // it is repainted.
        m_startupWindowCloaked = setStartupWindowCloaked(true);
        StartupProfiler::mark("startup.show.begin");
        show();
        StartupProfiler::mark("startup.show.end");
    }
    StartupProfiler::mark("startup.window-shown");
    if (isFullScreen()) {
        menuBar()->hide();
        ui->mainToolBar->hide();
        ui->pageFrame->hide();
        statusBar()->hide();
        ui->actionFullscreen->setChecked(true);
        ui->graphicsView->setFullscreenState(true);
        ui->graphicsView->refreshRenderedPages();
    }

    if (stayOnTop) {
        // The Windows correction uses winId(), so run it only after show().
        setStayOnTop(true);
    }

    // Reserve a deferred docked panel's final width before the first image is
    // laid out. The lightweight placeholder is replaced after the first paint.
    StartupProfiler::mark("startup.panel-reserve.begin");
    reserveConfiguredStartupPanelSpace();
    StartupProfiler::mark("startup.panel-reserve.end");
    StartupProfiler::mark("startup.panel-ready");

    // Settle the initial geometry now, including any reserved panel width, and
    // answer the messages Windows has queued: a window whose thread has not
    // pumped for tens of milliseconds is treated as hung, and the cursor over it
    // becomes the busy one.
    StartupProfiler::mark("startup.process-events.begin");
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    StartupProfiler::mark("startup.process-events.end");
    StartupProfiler::mark("startup.initial-events-processed");

    // Start the requested volume once the event loop is running, while the
    // window is still cloaked. The first completed image paint reveals the
    // final surface; an empty or failed startup request reveals it here.
    QTimer::singleShot(0, this, [this]() {
        if (!m_revealInitialWindow) {
            return;
        }
        if (layout()) {
            layout()->activate();
        }
        loadStartupVolume();
        if (!m_viewerSession.initialImagePaintPending()) {
            initializeStartupPanel();
            revealStartupWindow();
            QTimer::singleShot(0, this, &MainWindow::completeDeferredStartupWork);
        }
    });
}

void MainWindow::revealStartupWindow()
{
    if (!m_revealInitialWindow) {
        return;
    }
    StartupProfiler::mark("startup.reveal.begin");
    StartupProfiler::mark("startup.opacity-restore.begin");
#ifndef Q_OS_WIN
    setWindowOpacity(1.0);
#endif
    StartupProfiler::mark("startup.opacity-restore.end");
    StartupProfiler::mark("startup.repaint.begin");
    repaint();
    StartupProfiler::mark("startup.repaint.end");
    if (m_startupWindowCloaked) {
        StartupProfiler::mark("startup.uncloak.begin");
        setStartupWindowCloaked(false);
        StartupProfiler::mark("startup.uncloak.end");
        m_startupWindowCloaked = false;
    }
    m_revealInitialWindow = false;
    StartupProfiler::mark("startup.reveal.end");
    if (StartupProfiler::enabled() && qEnvironmentVariableIsSet("QV_PROFILE_FOLDER_TEXT")) {
        StartupFolderTextProfile::watch(this, [this] {
            return m_folderWindow ? m_folderWindow->findChild<FolderItemModel *>() : nullptr;
        });
    }
}

void MainWindow::loadStartupVolume()
{
    StartupProfiler::mark("startup-volume.begin");
    // when drop a folder/archive icon to this app
    if (qApp->arguments().length() >= 2) {
        openPath(qApp->arguments().last());
        setWindowTop(!qApp->TopWindowWhenRunWithAssoc());
        return;
    }
    // auto restore
    if (qApp->AutoLoaded() && !qApp->LastViewPath().isEmpty()) {
        openStoredPath(qApp->LastViewPath(), true);
        makeBookmarkMenu();
    }
}

void MainWindow::prefetchStartupTarget()
{
    // loadStartupVolume() opens the first argument when it is given, and the
    // stored view otherwise. Both targets are classified the same way here, so
    // the prefetch warms exactly the container that startup is going to open.
    if (qApp->arguments().length() >= 2) {
        m_viewerSession.prefetchStartupVolume(qApp->arguments().last());
        return;
    }
    if (qApp->AutoLoaded() && !qApp->LastViewPath().isEmpty()) {
        m_viewerSession.prefetchStartupVolume(
            loadStoredVolumeLocation(qApp->LastViewPath()).containerPath);
    }
}

MainWindow::~MainWindow()
{
    if (qApp->AutoLoaded() && m_viewerSession.visiblePageCount() > 0) {
        qApp->setLastViewPath(storeVolumeLocation(m_viewerSession.currentLocation()));
    }
    // reset() emits loadStatusChanged() and visiblePagesChanged(). Keep the UI
    // alive until those synchronous slots have finished.
    m_viewerSession.reset();
    delete ui;
    qApp->saveSettings();
}

void MainWindow::saveVisibleFolderViewWidth()
{
    if (!qApp->SaveFolderViewWidth() || !m_folderWindow || !m_folderWindow->isVisible()) {
        return;
    }
    if (m_folderWindow->parentWidget() == ui->catalogSplitter || m_folderWindow->isWindow()) {
        qApp->setFolderViewWidth(m_folderWindow->width());
    }
}

void MainWindow::resetShortcutKeys()
{
    QMap<QString, QAction *> &actions = qApp->keyActions().actions();
    QMap<QString, QKeySequence> &seqMap = qApp->keyActions().keyMaps();
    for (const QString &name : actions.keys()) {
        auto a = actions[name];
        QKeySequence seq = seqMap[name];
        //        a->setShortcut(seq);

        QList<QKeySequence> seqlist;
        for (int i = 0; i < seq.count(); i++) {
            seqlist << QKeySequence(seq[i]);
        }
        a->setShortcuts(seqlist);
        //        a->setShortcutContext(Qt::ApplicationShortcut);
    }
    handleScrollModeChanged(ui->graphicsView->isScrollMode());
}

void MainWindow::setStatusMessage(StatusMessage message)
{
    m_statusMessage = message;
    ui->statusLabel->clear();
    switch (message) {
    case StatusMessage::None:
        return;
    case StatusMessage::NoVolume:
        ui->graphicsView->showNoVolumeMessage();
        return;
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasFormat("text/uri-list")) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        QList<QUrl> urlList = e->mimeData()->urls();
        for (int i = 0; i < 1; i++) {
            QUrl url = urlList[i];
            openPath(QDir::toNativeSeparators(url.toLocalFile()));
            if (qApp->TopWindowWhenDropped()) {
                setWindowTop(false);
            }
        }
    }
}
static bool needContextMenu = false;

void MainWindow::wheelEvent(QWheelEvent *e)
{
    int delta_y = e->angleDelta().y();
    int delta = 0;
    if (delta_y < 0) {
        delta = -MouseDelta;
    } else if (delta_y > 0) {
        delta = MouseDelta;
    }
    QMouseValue mv(QKeySequence(qApp->keyboardModifiers()), e->buttons(), delta);
    QAction *action = qApp->mouseActions().getActionByValue(mv);
    if (e->buttons() & Qt::RightButton) {
        needContextMenu = false;
    }
    if (action == ui->actionZoomIn || action == ui->actionZoomOut) {
        action->trigger();
        e->accept();
        return;
    }
    if (ui->graphicsView->isScrollMode() && !qApp->ScrollWithCursorWhenZooming()) {
        return;
    }
    if (action) {
        action->trigger();
        e->accept();
        return;
    }
    QMainWindow::wheelEvent(e);
}

/**
 * @brief Support for Customized Shortcut Keys
 */
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QKeySequence seq(event->key() | event->modifiers());

    if (ui->graphicsView->isScrollMode() && !qApp->ScrollWithCursorWhenZooming()) {
        if (seq.toString() == "Left") {
            ui->graphicsView->horizontalScrollBar()->setValue(
                ui->graphicsView->horizontalScrollBar()->value() - 300);
            return;
        }
        if (seq.toString() == "Right") {
            ui->graphicsView->horizontalScrollBar()->setValue(
                ui->graphicsView->horizontalScrollBar()->value() + 300);
            return;
        }
        if (seq.toString() == "Up") {
            ui->graphicsView->verticalScrollBar()->setValue(
                ui->graphicsView->verticalScrollBar()->value() - 300);
            return;
        }
        if (seq.toString() == "Down") {
            ui->graphicsView->verticalScrollBar()->setValue(
                ui->graphicsView->verticalScrollBar()->value() + 300);
            return;
        }
    }

    QAction *action = qApp->keyActions().getActionByKey(seq);
    if (action) {
        QWidget *focusedWidget = focusWidget();
        const bool folderViewHasFocus =
            m_folderWindow && focusedWidget &&
            (focusedWidget == m_folderWindow || m_folderWindow->isAncestorOf(focusedWidget));
        if (folderViewHasFocus) {
            // Some global actions delete or replace FolderWindow. Let the key
            // event unwind before triggering them when it originated there.
            QMetaObject::invokeMethod(action, &QAction::trigger, Qt::QueuedConnection);
        } else {
            action->trigger();
        }
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *)
{
    // Capture the final layout rather than every automatic resize. In
    // particular, fullscreen can redistribute the splitter without emitting
    // splitterMoved.
    saveVisibleFolderViewWidth();
    m_onWindowClosing = true;
    // A panel that was taken out of this window is a window of its own, so it
    // would keep the application running with nothing to attach it to. It
    // belongs here and goes with the window it was taken out of.
    closeSeparatePanels();
    delete m_contextMenu;
    m_contextMenu = nullptr;
    qApp->setWindowGeometry(saveGeometry());
    qApp->setWindowState(saveState());
}

void MainWindow::closeSeparatePanels()
{
    // A panel docked in this window is a child of it and is destroyed with it.
    // One that was separated has no parent, so closing it is this window's job.
    if (m_folderWindow && m_folderWindow->isWindow()) {
        handleFolderWindowClosed();
    }
    if (m_catalogWindow && m_catalogWindow->isWindow()) {
        handleCatalogWindowClosed();
    }
    if (m_retouchWindow && m_retouchWindow->isWindow()) {
        handleRetouchWindowClosed();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    QKeyEvent *keyEvent = nullptr;//event data, if this is a keystroke event
    QMouseEvent *mouseEvent = nullptr;//event data, if this is a keystroke event
    //QDragEnterEvent  *dragEnterEvent = nullptr;//event data, if this is a keystroke event
    //QDropEvent *dropEvent = nullptr;//event data, if this is a keystroke event

//    if(obj == ui->graphicsView) {
//        qDebug() << "graphicsView <= " << event->type();
//    } else {
//        qDebug() << obj << " <= " << event->type();
//    }

    switch (event->type()) {
    case QEvent::ShortcutOverride:
        return true;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
        auto touchEv = dynamic_cast<QTouchEvent *>(event);
        if (touchEv) {
            touchEvent(touchEv);
            return true;
        }
        break;
    }
    case QEvent::KeyPress:
        if (obj == ui->graphicsView) {
            keyEvent = dynamic_cast<QKeyEvent *>(event);
            this->keyPressEvent(keyEvent);
            return true;
        }
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
        if (obj == ui->graphicsView) {
            mouseEvent = dynamic_cast<QMouseEvent *>(event);
            // tap left/right of window
            if (mouseEvent->button() == Qt::LeftButton) {
                if (ui->graphicsView->hoverState() == Qt::AnchorLeft) {
                    ui->actionTurnPageOnLeft->trigger();
                    return true;
                }
                if (ui->graphicsView->hoverState() == Qt::AnchorRight) {
                    ui->actionTurnPageOnRight->trigger();
                    return true;
                }
            }
            // The ContextMenu event is valid only when RightButton is pushed alone
            // and it is invalidated when the other button is pushed or the wheel moves
            if (mouseEvent->buttons() == Qt::RightButton) {
                needContextMenu = true;
            }
            if ((mouseEvent->buttons() & Qt::RightButton) &&
                (mouseEvent->buttons() & ~Qt::RightButton)) {
                needContextMenu = false;
            }
            QMouseValue mv(QKeySequence(qApp->keyboardModifiers()), mouseEvent->buttons(), 0);
            // Processed in ContextMenu event
            if (mv.Key == "+::RightButton") {
                break;
            }
            // If isScrollMode () is enabled, priority is given to screen drag scroll
            if (mv.Key == "+::LeftButton" && ui->graphicsView->isScrollMode()) {
                break;
            }
            QAction *action = qApp->mouseActions().getActionByValue(mv);
            if (action) {
                action->trigger();
                return true;
            }
        }
        break;
    // ContextMenu event occurs when releasing the RightButton
    case QEvent::ContextMenu:
        if (obj == ui->graphicsView) {
            //            QContextMenuEvent *contextMenuEvent = dynamic_cast<QContextMenuEvent*>(event);
            //            qDebug() << contextMenuEvent;
            QMouseValue mv(QKeySequence(qApp->keyboardModifiers()), Qt::RightButton, 0);
            QAction *action = qApp->mouseActions().getActionByValue(mv);
            if (action && needContextMenu) {
                action->trigger();
                needContextMenu = false;
            }
            return true;
        }
        if (obj == ui->mainToolBar) {
            return true;
        }
        break;
    case QEvent::Leave:
        if (obj == ui->mainToolBar && isFullScreen()) {
            ui->mainToolBar->hide();
            return true;
        }
        if (obj == ui->pageFrame && isFullScreen()) {
            ui->pageFrame->hide();
            return true;
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

void MainWindow::openPath(QString path, bool allowSecondPage)
{
    openResolvedTarget(OpenTarget::forPath(path), allowSecondPage);
}

void MainWindow::openTarget(const OpenTarget &target)
{
    openResolvedTarget(target, false);
}

void MainWindow::openStoredPath(const QString &storedPath, bool allowSecondPage)
{
    const VolumeLocation location = loadStoredVolumeLocation(storedPath);
    if (location.isContainer()) {
        // Without a stored entry the path may name a folder, an archive or an
        // image file, so it is classified like any other user supplied path.
        openPath(storedPath, allowSecondPage);
        return;
    }
    openResolvedTarget(OpenTarget::entry(location), allowSecondPage);
}

void MainWindow::openResolvedTarget(const OpenTarget &target, bool allowSecondPage)
{
    const VolumeLocation &location = target.location;
    const bool isFileTarget = target.intent == OpenIntent::FileInContainer;
    const QString filePath = isFileTarget
                                 ? QDir(location.containerPath).absoluteFilePath(location.entryName)
                                 : QString();
    const QString requestedPath =
        QDir::fromNativeSeparators(isFileTarget ? filePath : location.containerPath);
    const bool requestedArchive = IFileLoader::isArchiveFile(requestedPath);
    m_folderViewRequestedPath = requestedPath;
    if (m_folderWindow && !requestedArchive) {
        m_folderWindow->handleViewerSessionVolumeChanged(m_folderViewRequestedPath);
    }
    if (isFileTarget) {
        m_viewerSession.openFileInContainer(filePath, allowSecondPage);
        // The folder view follows the viewer, but a folder it already shows
        // must not be read again: its list stays as it is until the user
        // reloads it.
        const QString folderPath = QFileInfo(requestedPath).absolutePath();
        const bool folderViewShowsIt =
            m_folderWindow &&
            QDir::cleanPath(QDir::fromNativeSeparators(m_folderWindow->currentPath())) ==
                QDir::cleanPath(QDir::fromNativeSeparators(folderPath));
        if (!folderViewShowsIt) {
            changeFolderPath(folderPath);
        }
        return;
    }
    const bool opened = target.intent == OpenIntent::Entry
                            ? m_viewerSession.openEntry(location)
                            : m_viewerSession.openContainer(location.containerPath);
    if (opened) {
        changeFolderPath(m_viewerSession.volumePath());
        return;
    }

    if (changeFolderPath(requestedPath)) {
        return;
    }

    createFolderWindow(true, requestedPath);
}

void MainWindow::makeHistoryMenu()
{
    if (!m_deferredMenusInitialized) {
        return;
    }
    static const QString shortcuts = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    ui->menuHistory->clear();
    const QStringList &history = qApp->History();
    for (int i = 0; i < history.size(); i++) {
        const QString shortcut = shortcuts.mid(i, 1);
        // Entries past the shortcut list show only their path, and every
        // action carries the stored path so the handler never parses the text.
        QAction *action = ui->menuHistory->addAction(
            shortcut.isEmpty() ? history.at(i)
                               : QString("&%1: %2").arg(shortcut).arg(history.at(i)));
        action->setData(history.at(i));
    }
}

void MainWindow::makeBookmarkMenu()
{
    if (!m_deferredMenusInitialized) {
        return;
    }
    static const QString shortcuts = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    ui->menuLoadBookmark->clear();
    QStringList bookmarks = qApp->Bookmarks();
    if (!bookmarks.size()) {
        return;
    }
    for (int i = 0; i < bookmarks.size(); i++) {
        QString path = bookmarks[i];
        QFileInfo info(path);
        QString text = QString("&%1: %2 - %3")
                           .arg(shortcuts.mid(i, 1))
                           .arg(info.fileName())
                           .arg(info.dir().dirName());
        QAction *action = ui->menuLoadBookmark->addAction(text);
        action->setData(path);
    }
    ui->menuLoadBookmark->addSeparator();
    ui->menuLoadBookmark->addAction(ui->actionClearBookmarks);
}

void MainWindow::setCatalogDatabase(CatalogDatabase *catalogDatabase)
{
    m_catalogDatabase = catalogDatabase;

    const bool startupVolumeRequested =
        qApp->arguments().length() >= 2 || (qApp->AutoLoaded() && !qApp->LastViewPath().isEmpty());
    if (!startupVolumeRequested) {
        initializeConfiguredStartupPanel();
    }
}

void MainWindow::initializeConfiguredStartupPanel(const QString &folderPath)
{
    if (m_startupPanelInitialized || !m_catalogDatabase) {
        return;
    }
    m_startupPanelInitialized = true;

    switch (qApp->ShowOptionViewOnStartup()) {
    case qvEnums::OptionViewOnStartup::NoViewStartup:
        break;
    case qvEnums::OptionViewOnStartup::FolderStartup:
        createFolderWindow(!qApp->ShowPanelSeparateWindow(), folderPath);
        break;
    case qvEnums::OptionViewOnStartup::CatalogStartup:
        createCatalogWindow(!qApp->ShowPanelSeparateWindow());
        break;
    case qvEnums::OptionViewOnStartup::RetouchStartup:
        createRetouchWindow(!qApp->ShowPanelSeparateWindow());
        break;
    }
}

void MainWindow::reserveConfiguredStartupPanelSpace()
{
    const bool startupVolumeRequested =
        qApp->arguments().length() >= 2 || (qApp->AutoLoaded() && !qApp->LastViewPath().isEmpty());
    if (!startupVolumeRequested || qApp->ShowPanelSeparateWindow() || m_startupPanelPlaceholder) {
        return;
    }

    int panelWidth = 0;
    switch (qApp->ShowOptionViewOnStartup()) {
    case qvEnums::OptionViewOnStartup::NoViewStartup:
        return;
    case qvEnums::OptionViewOnStartup::FolderStartup:
        panelWidth = qApp->SaveFolderViewWidth() ? qApp->FolderViewWidth() : 200;
        break;
    case qvEnums::OptionViewOnStartup::CatalogStartup:
        panelWidth = qApp->SaveCatalogViewWidth() ? qApp->CatalogViewWidth() : 200;
        break;
    case qvEnums::OptionViewOnStartup::RetouchStartup:
        panelWidth = 200;
        break;
    }

    m_startupPanelPlaceholder = new QWidget(ui->catalogSplitter);
    m_startupPanelPlaceholder->setObjectName(QStringLiteral("startupPanelPlaceholder"));
    ui->catalogSplitter->insertWidget(0, m_startupPanelPlaceholder);
    auto sizes = ui->catalogSplitter->sizes();
    const int sum = sizes[0] + sizes[1];
    sizes[0] = panelWidth;
    sizes[1] = sum - panelWidth;
    ui->catalogSplitter->setSizes(sizes);
}

bool MainWindow::replaceStartupPanelPlaceholder(QWidget *panel)
{
    if (!m_startupPanelPlaceholder) {
        return false;
    }
    const int index = ui->catalogSplitter->indexOf(m_startupPanelPlaceholder);
    if (index < 0) {
        delete m_startupPanelPlaceholder;
        m_startupPanelPlaceholder = nullptr;
        return false;
    }

    QWidget *placeholder = m_startupPanelPlaceholder;
    m_startupPanelPlaceholder = nullptr;
    QWidget *replaced = ui->catalogSplitter->replaceWidget(index, panel);
    Q_ASSERT(replaced == placeholder);
    delete replaced;
    return true;
}

void MainWindow::handleExitActionTriggered()
{
    close();
    //    QApplication::quit();
    //    QCoreApplication::quit();
}

void MainWindow::handleSavingHistoryActionTriggered(bool checked)
{
    qApp->setDontSavingHistory(checked);
}

void MainWindow::handleClearHistoryActionTriggered()
{
    qApp->clearHistory();
    makeHistoryMenu();
}

void MainWindow::handleGraphicsViewAnchorHovered(Qt::AnchorPoint anchor)
{
    bool fullscreen = isFullScreen();
    bool showMenubar = fullscreen ? !qApp->HideMenuBarInFullscreen()
                                  : (!qApp->ShowMenuBar() && !qApp->HideMenuBarParmanently());
    bool showToolbar = fullscreen ? !qApp->HideToolBarInFullscreen()
                                  : (!qApp->ShowToolBar() && !qApp->HideToolBarParmanently());
    bool showPageBar = fullscreen ? !qApp->HidePageBarInFullscreen()
                                  : (!qApp->ShowSliderBar() && !qApp->HidePageBarParmanently());
    if (!showToolbar && !showMenubar && !showPageBar) {
        return;
    }
    if (anchor == Qt::AnchorTop && (showMenubar || showToolbar)) {
        InnerFrame *innerFrame = new InnerFrame(ui->graphicsView);
        connect(innerFrame, &InnerFrame::init, this, [=] {
            if (showMenubar) {
                innerFrame->layout()->addWidget(ui->menuBar);
                ui->menuBar->setVisible(true);
            }
            if (showToolbar) {
                innerFrame->layout()->addWidget(ui->mainToolBar);
                ui->mainToolBar->setVisible(true);
            }
            ui->menuBar->setCursor(Qt::ArrowCursor);
            ui->mainToolBar->setCursor(Qt::ArrowCursor);
            qApp->setInnerFrameShowing(true);
        });
        connect(innerFrame, &InnerFrame::deinit, this, [=] {
            //            qDebug() << showToolbar << showMenubar << fullscreen;
            bool fullscreen2 = isFullScreen();
            if (showToolbar || fullscreen2) {
                ui->mainToolBar->setVisible(false);
                addToolBar(ui->mainToolBar);
                if (!fullscreen2 && qApp->ShowToolBar()) {
                    ui->mainToolBar->setVisible(true);
                }
            }
            if (showMenubar || fullscreen2) {
                ui->menuBar->setVisible(false);
                setMenuBar(ui->menuBar);
                if (!fullscreen2 && qApp->ShowMenuBar()) {
                    ui->menuBar->setVisible(true);
                }
            }
            qApp->setInnerFrameShowing(false);
        });
        connect(this, &MainWindow::changingFullscreen, innerFrame, &InnerFrame::close);
        connect(innerFrame, &InnerFrame::closed, this, [=] { delete innerFrame; });
        innerFrame->showWithoutTitleBar();
    }
    if (anchor == Qt::AnchorBottom && !qApp->HidePageBarParmanently() &&
        (showPageBar || fullscreen)) {
        InnerFrame *innerFrame =
            new InnerFrame(ui->graphicsView, Qt::AnchorBottom, qApp->LargeToolbarIcons() ? 60 : 30);
        connect(innerFrame, &InnerFrame::init, this, [&] {
            innerFrame->layout()->addWidget(ui->pageFrame);
            ui->pageFrame->show();
            qApp->setInnerFrameShowing(true);
            ui->pageFrame->setCursor(Qt::ArrowCursor);
        });
        connect(innerFrame, &InnerFrame::deinit, this, [&] {
            ui->pageFrame->hide();
            ui->verticalViewPage->layout()->addWidget(ui->pageFrame);
            qApp->setInnerFrameShowing(false);
        });
        connect(this, &MainWindow::changingFullscreen, innerFrame, &InnerFrame::close);
        connect(innerFrame, &InnerFrame::closed, this, [=] { delete innerFrame; });
        innerFrame->showWithoutTitleBar();
    }
    if (anchor == Qt::AnchorHorizontalCenter) {
        //        ui->pageFrame->hide();
    }
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleScrollModeChanged(bool scrolled)
{
    QStringList cusors = {"Left", "Right", "Up", "Down"};
    // enable/disable cursor key shortcuts
    for (const QString &c : cusors) {
        auto key = QKeySequence(c);
        QString name = qApp->keyActions().getNameByKey(key);
        if (!name.isEmpty()) {
            resetShortCut(name, c, scrolled);
        }
    }
}

void MainWindow::resetShortCut(const QString name, const QString shortcuttext, bool removed)
{
    QMap<QString, QAction *> &actions = qApp->keyActions().actions();
    QMap<QString, QKeySequence> &seqMap = qApp->keyActions().keyMaps();
    auto a = actions[name];
    QKeySequence seq = seqMap[name];

    QList<QKeySequence> seqlist;
    for (int i = 0; i < seq.count(); i++) {
        seqlist << QKeySequence(seq[i]);
    }
    if (removed) {
        seqlist.removeOne(QKeySequence(shortcuttext));
    }
    a->setShortcuts(seqlist);
}

void MainWindow::closeAllDockedWindow()
{
    if (m_catalogWindow && m_catalogWindow->parent()) {
        handleCatalogWindowClosed();
    }
    if (m_folderWindow && m_folderWindow->parent()) {
        handleFolderWindowClosed();
    }
    if (m_retouchWindow && m_retouchWindow->parent()) {
        handleRetouchWindowClosed();
    }
    if (m_exifDialog && m_exifDialog->parent()) {
        handleExifDialogClosed();
    }
}

////////////////////////////
//// FolderWindow
////////////////////////////
void MainWindow::handleShowFolderActionTriggered()
{
    if (m_folderWindow) {
        handleFolderWindowClosed();
        return;
    }
    createFolderWindow(!qApp->ShowPanelSeparateWindow());
}

void MainWindow::handleShowSubfoldersActionTriggered(bool checked)
{
    const bool wasShowingSubfolders = qApp->ShowSubfolders();
    qApp->setShowSubfolders(checked);
    if (checked == wasShowingSubfolders) {
        return;
    }
    // Turning the option off is applied the next time a volume is opened, so
    // that the image on screen stays part of the displayed volume. Turning it
    // on scans the folder again straight away and keeps the current page.
    if (!checked || !m_viewerSession.isFolder()) {
        return;
    }
    const QString containerPath = m_viewerSession.volumePath();
    if (containerPath.isEmpty()) {
        return;
    }
    m_viewerSession.reloadContainer(containerPath);
}

void MainWindow::handleFolderWindowClosed()
{
    FolderWindow *folderWindow = m_folderWindow;
    if (!folderWindow) {
        return;
    }
    // Detach the panel before it is destroyed: callbacks that arrive while it
    // is being torn down must not find it any more.
    m_folderWindow = nullptr;
    delete folderWindow;
    ui->actionShowFolder->setChecked(false);

    if (!m_onWindowClosing) {
        qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::NoViewStartup);
    }
}

bool MainWindow::isFolderSearching()
{
    if (!m_folderWindow || !m_folderWindow->parent()) {
        return false;
    }
    return true;
}

void MainWindow::handleFolderWindowOpenVolume(const OpenTarget &target)
{
    openTarget(target);
}

void MainWindow::handleFolderWindowReloadRequested(const QString &containerPath)
{
    m_viewerSession.reloadContainer(containerPath);
}

void MainWindow::createFolderWindow(bool docked, QString path, bool deferLoad)
{
    const bool deferFolderLoad = deferLoad || m_viewerSession.initialImagePaintPending();
    QString oldpath = path;
    if (m_folderWindow) {
        oldpath = m_folderWindow->currentPath();
        handleFolderWindowClosed();
    }
    if (oldpath.isEmpty() && !m_pendingFolderPath.isEmpty()) {
        oldpath = m_pendingFolderPath;
    }
    if (deferFolderLoad) {
        m_pendingFolderPath = oldpath;
    } else {
        m_pendingFolderPath.clear();
    }

    if (oldpath.isEmpty()) {
        oldpath = m_viewerSession.volumePath();
        if (oldpath.isEmpty()) {
            oldpath = m_folderViewRequestedPath;
        }
        if (oldpath.isEmpty()) {
            oldpath = qApp->HomeFolderPath();
        }
    }
    qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::FolderStartup);
    if (docked) {
        closeAllDockedWindow();
        StartupProfiler::mark("folder-window.construct.begin");
        m_folderWindow = new FolderWindow(nullptr, ui);
        StartupProfiler::mark("folder-window.construct.end");
        if (!deferFolderLoad) {
            m_folderWindow->setFolderPath(oldpath, false);
        }
        // Queued: the independent window emits this from its own closeEvent,
        // and deleting the widget there would use it after close() returns.
        connect(m_folderWindow,
                &FolderWindow::closed,
                this,
                &MainWindow::handleFolderWindowClosed,
                Qt::QueuedConnection);
        connect(m_folderWindow,
                &FolderWindow::openVolume,
                this,
                &MainWindow::handleFolderWindowOpenVolume);
        connect(m_folderWindow,
                &FolderWindow::reloadRequested,
                this,
                &MainWindow::handleFolderWindowReloadRequested);
        if (!replaceStartupPanelPlaceholder(m_folderWindow)) {
            ui->catalogSplitter->insertWidget(0, m_folderWindow);
        }
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        const int requestedWidth = qApp->SaveFolderViewWidth() ? qApp->FolderViewWidth() : 200;
        // Avoid invalid splitter sizes first; QSplitter then applies the
        // widgets' effective minimum and maximum sizes.
        sizes[0] = qBound(0, requestedWidth, sum);
        sizes[1] = sum - sizes[0];
        ui->catalogSplitter->setSizes(sizes);
    } else {
        // close child widget, and recreate as independent window
        StartupProfiler::mark("folder-window.construct.begin");
        m_folderWindow = new FolderWindow(nullptr, ui);
        StartupProfiler::mark("folder-window.construct.end");
        QRect self = geometry();
        m_folderWindow->setGeometry(
            self.left() - 100, self.top() + 100, self.width(), self.height());
        if (!deferFolderLoad) {
            m_folderWindow->setFolderPath(oldpath, false);
        }
        // Queued: the independent window emits this from its own closeEvent,
        // and deleting the widget there would use it after close() returns.
        connect(m_folderWindow,
                &FolderWindow::closed,
                this,
                &MainWindow::handleFolderWindowClosed,
                Qt::QueuedConnection);
        connect(m_folderWindow,
                &FolderWindow::openVolume,
                this,
                &MainWindow::handleFolderWindowOpenVolume);
        connect(m_folderWindow,
                &FolderWindow::reloadRequested,
                this,
                &MainWindow::handleFolderWindowReloadRequested);
        m_folderWindow->show();
    }
    updateFolderViewCurrentItem();
    ui->actionShowFolder->setChecked(true);
}

void MainWindow::updateFolderViewCurrentItem()
{
    if (!m_folderWindow) {
        return;
    }
    const QString path = m_viewerSession.isArchive() ? m_viewerSession.volumePath()
                                                     : m_viewerSession.currentPagePath();
    if (!path.isEmpty()) {
        m_folderViewRequestedPath = path;
    }
    m_folderWindow->handleViewerSessionVolumeChanged(m_folderViewRequestedPath);
}

bool MainWindow::changeFolderPath(QString path)
{
    if (m_viewerSession.initialImagePaintPending()) {
        m_pendingFolderPath = path;
        return false;
    }
    if (!m_folderWindow) {
        // An image passed on the command line is loaded before setCatalogDatabase()
        // creates the startup FolderWindow. Preserve its directory until then.
        m_pendingFolderPath = path;
        return false;
    }
    m_folderWindow->setFolderPath(path, false);
    return true;
}

void MainWindow::handleInitialImageDisplayFinished()
{
    // The panel belongs to the frame the reveal paints: create it while the
    // window is still cloaked, so the folder list is already there when the
    // window appears - showing a placeholder for any name whose font is still
    // loading - instead of arriving a moment later. The menus stay deferred;
    // they are the part of the startup work the first frame does not need.
    initializeStartupPanel();
    revealStartupWindow();
    if (StartupProfiler::enabled() && !qEnvironmentVariableIsSet("QV_PROFILE_FOLDER_TEXT")) {
        StartupProfiler::flush();
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }
    QTimer::singleShot(0, this, &MainWindow::completeDeferredStartupWork);
}

void MainWindow::completeDeferredStartupWork()
{
    initializeDeferredMenus();
    initializeStartupPanel();
}

/**
 * Builds the panel the startup window shows, or points the panel that already
 * exists at the startup path. Called before the window is revealed, so the
 * folder list is part of the frame the reveal paints.
 */
void MainWindow::initializeStartupPanel()
{
    if (m_folderWindow) {
        if (!m_pendingFolderPath.isEmpty()) {
            const QString path = m_pendingFolderPath;
            m_pendingFolderPath.clear();
            m_folderWindow->setFolderPath(path, false);
            updateFolderViewCurrentItem();
        }
        return;
    }

    const QString path = m_pendingFolderPath;
    m_pendingFolderPath.clear();
    initializeConfiguredStartupPanel(path);
}

void MainWindow::initializeDeferredMenus()
{
    if (m_deferredMenusInitialized) {
        return;
    }
    m_deferredMenusInitialized = true;
    qApp->languageSelector()->initializeMenu(ui->menuChange_Language);
    makeHistoryMenu();
    makeBookmarkMenu();
}

////////////////////////////
//// CatalogWindow
////////////////////////////
void MainWindow::handleShowCatalogActionTriggered()
{
    if (m_catalogWindow) {
        handleCatalogWindowClosed();
        return;
    }
    createCatalogWindow(!qApp->ShowPanelSeparateWindow());
}

void MainWindow::handleCatalogWindowClosed()
{
    if (m_catalogWindow) {
        delete m_catalogWindow;
        m_catalogWindow = nullptr;
        ui->actionShowCatalog->setChecked(false);

        if (!m_onWindowClosing) {
            qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::NoViewStartup);
        }
    }
}

void MainWindow::handleManageCatalogsActionTriggered()
{
    // The dialog belongs to the catalog panel, and the panel reloads its list
    // when the dialog closes, so open the panel first.
    if (!m_catalogWindow) {
        createCatalogWindow(!qApp->ShowPanelSeparateWindow());
    }
    if (m_catalogWindow) {
        m_catalogWindow->handleManageCatalogButtonClicked();
    }
}

bool MainWindow::isCatalogSearching()
{
    if (!m_catalogWindow || !m_catalogWindow->parent()) {
        return false;
    }
    return m_catalogWindow->isCatalogSearching();
}

void MainWindow::createCatalogWindow(bool docked)
{
    if (m_catalogWindow) {
        handleCatalogWindowClosed();
    }
    qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::CatalogStartup);
    if (docked) {
        closeAllDockedWindow();
        int lastwidth = qApp->CatalogViewWidth();
        m_catalogWindow = new CatalogWindow(nullptr, ui);
        m_catalogWindow->setCatalogDatabase(m_catalogDatabase);
        connect(
            m_catalogWindow, &CatalogWindow::closed, this, &MainWindow::handleCatalogWindowClosed);
        connect(m_catalogWindow,
                &CatalogWindow::openVolume,
                this,
                &MainWindow::handleCatalogWindowOpenVolume);
        if (!replaceStartupPanelPlaceholder(m_catalogWindow)) {
            ui->catalogSplitter->insertWidget(0, m_catalogWindow);
        }
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        sizes[0] = qApp->SaveCatalogViewWidth() ? lastwidth : 200;
        sizes[1] = sum - sizes[0];
        ui->catalogSplitter->setSizes(sizes);
        m_catalogWindow->setAsInnerWidget();
    } else {
        m_catalogWindow = new CatalogWindow(nullptr, ui);
        m_catalogWindow->setCatalogDatabase(m_catalogDatabase);
        connect(
            m_catalogWindow, &CatalogWindow::closed, this, &MainWindow::handleCatalogWindowClosed);
        connect(m_catalogWindow,
                &CatalogWindow::openVolume,
                this,
                &MainWindow::handleCatalogWindowOpenVolume);
        m_catalogWindow->setAsToplevelWindow();
        QRect self = geometry();
        m_catalogWindow->setGeometry(
            self.left() - 100, self.top() + 100, self.width(), self.height());
        m_catalogWindow->show();
    }
    ui->actionShowCatalog->setChecked(true);
}

////////////////////////////
//// Retouch panel
////////////////////////////
void MainWindow::handleShowRetouchWindowActionTriggered()
{
    if (m_retouchWindow) {
        handleRetouchWindowClosed();
        return;
    }
    createRetouchWindow(!qApp->ShowPanelSeparateWindow());
}

void MainWindow::handleRetouchWindowClosed()
{
    if (m_retouchWindow) {
        delete m_retouchWindow;
        m_retouchWindow = nullptr;
        ui->actionShowRetouchWindow->setChecked(false);

        if (!m_onWindowClosing) {
            qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::NoViewStartup);
        }
    }
}

void MainWindow::createRetouchWindow(bool docked)
{
    if (m_retouchWindow) {
        handleRetouchWindowClosed();
    }
    if (m_viewerSession.visiblePages().isEmpty()) {
        return;
    }
    qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::RetouchStartup);
    m_retouchWindow = new RetouchWindow(nullptr);
    connect(m_retouchWindow, &RetouchWindow::closed, this, &MainWindow::handleRetouchWindowClosed);
    connect(m_retouchWindow,
            &RetouchWindow::retouchParametersChanged,
            ui->graphicsView,
            &ImageView::handleRetouchParametersChanged);
    m_retouchWindow->initializeFromImageView(ui->graphicsView);

    if (docked) {
        closeAllDockedWindow();
        if (!replaceStartupPanelPlaceholder(m_retouchWindow)) {
            ui->catalogSplitter->insertWidget(0, m_retouchWindow);
        }
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        sizes[0] = 200;
        sizes[1] = sum - sizes[0];
        ui->catalogSplitter->setSizes(sizes);
    } else {
        QRect self = geometry();
        m_retouchWindow->setGeometry(
            self.left() - 100, self.top() + 100, self.width(), self.height());
        m_retouchWindow->show();
    }
    ui->actionShowRetouchWindow->setChecked(true);
}

////////////////////////////
//// ExifDialog
////////////////////////////
constexpr int ExifDialogWidth = 280;

void MainWindow::handleOpenExifActionTriggered()
{
    if (m_exifDialog || m_viewerSession.visiblePageCount() == 0) {
        return;
    }
    const VisiblePages pages = m_viewerSession.visiblePages();
    const ImageContent *page = pages.first();
    if (!page || page->exifInfo.ImageWidth == 0) {
        return;
    }
    if (m_catalogWindow && m_catalogWindow->parent()) {
        handleCatalogWindowClosed();
    }
    if (m_folderWindow && m_folderWindow->parent()) {
        handleFolderWindowClosed();
    }

    m_exifDialog = new ExifDialog();
    ui->actionOpenExif->setChecked(true);
    m_exifDialog->setExif(*page);
    connect(m_exifDialog, &ExifDialog::closed, this, &MainWindow::handleExifDialogClosed);

    ui->catalogSplitter->insertWidget(1, m_exifDialog);
    auto sizes = ui->catalogSplitter->sizes();
    int sum = sizes[0] + sizes[1];
    sizes[1] = ExifDialogWidth;
    sizes[0] = sum - sizes[1];
    ui->catalogSplitter->setSizes(sizes);
}

void MainWindow::handleExifDialogClosed()
{
    if (m_exifDialog) {
        delete m_exifDialog;
        m_exifDialog = nullptr;
        ui->actionOpenExif->setChecked(false);
    }
}

void MainWindow::handleFullscreenActionTriggered()
{
    qDebug() << "handleFullscreenActionTriggered";
    if (isFullScreen()) {
        emit changingFullscreen(false);
        ui->graphicsView->setFullscreenState(false);
        ui->graphicsView->setResizeEventsSkipped(true);
        if (qApp->ShowMenuBar()) {
            menuBar()->show();
        }
        if (qApp->ShowToolBar()) {
            ui->mainToolBar->show();
        }
        if (qApp->ShowSliderBar()) {
            ui->pageFrame->show();
        }
        if (qApp->ShowStatusBar()) {
            statusBar()->show();
        }
        ui->actionFullscreen->setChecked(false);
        ui->graphicsView->setResizeEventsSkipped(false);
        ui->graphicsView->setCursor(Qt::ArrowCursor);

        if (m_viewerWindowStateMaximized) {
            showMaximized();
        } else {
            showNormal();
        }
        if (!qApp->SlideShowOnNormalWindow() && ui->graphicsView->isSlideShow()) {
            ui->graphicsView->toggleSlideShow();
        }
    } else {
        emit changingFullscreen(true);
        ui->graphicsView->setFullscreenState(true);
        ui->graphicsView->setResizeEventsSkipped(true);
        m_viewerWindowStateMaximized = isMaximized();
        if (qApp->HideMouseCursorInFullscreen()) {
            ui->graphicsView->setCursor(Qt::BlankCursor);
        }

        menuBar()->hide();
        ui->mainToolBar->hide();
        ui->pageFrame->hide();
        statusBar()->hide();
        ui->actionFullscreen->setChecked(true);
        showFullScreen();
    }
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleStayOnTopActionTriggered(bool checked)
{
    qApp->setStayOnTop(checked);
    // Qt's StayOnTop mechanism is not working correctly in Windows.
    // so win32api calling manually
    if (setStayOnTop(checked)) {
        return;
    }
    Qt::WindowFlags flags = windowFlags();
    if (checked) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }

    const bool visible = isVisible();
    const bool full = isFullScreen();
    setWindowFlags(flags);
    if (!visible) {
        return;
    }
    if (!full) {
        show();
        return;
    }
    handleFullscreenActionTriggered();
    handleFullscreenActionTriggered();
}

void MainWindow::handleGraphicsViewFittingChanged(qvEnums::FitMode mode)
{
    ui->actionFitting->setChecked(qApp->Fitting());
    ui->actionFitToWindow->setChecked(mode == qvEnums::FitMode::FitToRect);
    ui->actionFitToWidth->setChecked(mode == qvEnums::FitMode::FitToWidth);
}

void MainWindow::handleViewerSessionPageChanged()
{
    int maxVolume = m_viewerSession.pageCount();
    if (maxVolume <= 0) {
        syncPageBar();
        return;
    }
    if (!m_viewerSession.initialImagePaintPending()) {
        updateFolderViewCurrentItem();
    }
    // PageSlider
    syncPageBar();

    // StatusBar
    m_statusMessage = StatusMessage::None;
    m_pageCaption = m_imageString.getStatusBarText();

    // Elide text(Otherwise the width of the main window will be forcibly changed)
    QFontMetrics fontMetrics(ui->statusLabel->font());
    QString statusLabelTxt = fontMetrics.elidedText(m_pageCaption, Qt::ElideMiddle, width() - 100);
    ui->statusLabel->setText(statusLabelTxt);
    resetVolumeCaption();

    if (!qApp->ShowStatusBar()) {
        setWindowTitle(QString("%1 - %2").arg(m_pageCaption).arg(qApp->applicationName()));
    }

    if (m_exifDialog) {
        const VisiblePages pages = m_viewerSession.visiblePages();
        if (const ImageContent *page = pages.first()) {
            m_exifDialog->setExif(*page);
        }
    }
}

void MainWindow::handleViewerSessionVolumeChanged(QString path)
{
    if (!m_viewerSession.initialImagePaintPending()) {
        updateFolderViewCurrentItem();
    }
    if (path.isEmpty()) {
        setWindowTitle(
            QString("%1 v%2").arg(qApp->applicationName()).arg(qApp->applicationVersion()));
        syncPageBar();
        return;
    }
    if (!qApp->DontSavingHistory()) {
        qApp->addHistory(path);
    }
    if (!isFullScreen() && qApp->ShowSliderBar()) {
        ui->pageFrame->show();
    }

    resetVolumeCaption();
    makeHistoryMenu();
}

void MainWindow::handlePageSliderValueChanged(int value)
{
    if (m_sliderChanging) {
        return;
    }
    m_sliderChanging = true;
    const bool selected = m_viewerSession.selectPage(value - 1);
    m_sliderChanging = false;
    if (!selected) {
        syncPageBar();
    }
}

void MainWindow::handleViewerLoadStatusChanged()
{
    if (m_viewerSession.loadStatus().phase == ViewerLoadPhase::Loading ||
        m_viewerSession.loadStatus().phase == ViewerLoadPhase::Failed) {
        m_statusMessage = StatusMessage::None;
        m_pageCaption.clear();
        ui->statusLabel->clear();
    }
    syncPageBar();
}

void MainWindow::syncPageBar()
{
    const ViewerLoadStatus &status = m_viewerSession.loadStatus();
    const int pageCount = m_viewerSession.pageCount();
    const bool ready = status.phase == ViewerLoadPhase::Ready && pageCount > 0;

    m_sliderChanging = true;
    ui->pageSlider->setEnabled(ready);
    if (!ready) {
        ui->pageSlider->setRange(0, 0);
        ui->pageSlider->setValue(0);
        if (status.phase == ViewerLoadPhase::Loading) {
            ui->pageLabel->setText(tr("Loading..."));
        } else if (status.failureReason == LoadFailureReason::NoViewableImages ||
                   status.phase == ViewerLoadPhase::Empty) {
            ui->pageLabel->setText(tr("No images"));
        } else {
            ui->pageLabel->setText(tr("Unavailable"));
        }
        m_sliderChanging = false;
        return;
    }

    int maximum = pageCount;
    if (qApp->DualView() && ((pageCount - m_viewerSession.currentPageIndex()) & 0x1) == 0) {
        --maximum;
    }
    ui->pageLabel->setText(m_viewerSession.currentPageNumberText());
    ui->pageSlider->setRange(1, qMax(1, maximum));
    ui->pageSlider->setValue(m_viewerSession.currentPageIndex() + 1);
    m_sliderChanging = false;
}

void MainWindow::handleAppVersionActionTriggered()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString("about %1").arg(QApplication::applicationName()));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setTextFormat(Qt::RichText);
    //    msgBox.setText(QApplication::applicationVersion());
    QString message =
        QString(
            "<h1>%1 %2</h1><p>%3&lt;<a "
            "href=\"mailto:k.kanryu@gmail.com\">k.kanryu@gmail.com&gt;</a> All rights reserved.</p>"
            "<p>Project Webpage: <a "
            "href=\"https://kanryu.github.io/quickviewer/\">https://kanryu.github.io/quickviewer/</"
            "a></p>"
            "<p>This program is distributed in the hope that it will be useful, but WITHOUT ANY "
            "WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A "
            "PARTICULAR PURPOSE. See the GNU General Public License for more details.</p>")
            .arg(QApplication::applicationName())
            .arg(QApplication::applicationVersion())
            .arg(AppCopyright);
    msgBox.setText(message);
    msgBox.exec();
}

void MainWindow::handleLanguageSelectorLanguageChanged(QString language)
{
    qApp->setUiLanguage(language);
    ui->retranslateUi(this);
    m_fullscreenButton->setToolTip(tr("&Fullscreen"));
    ui->statusBar->clearMessage();

    qApp->keyActions().clearActionGroups();
    qApp->registerActions(ui);

    if (m_viewerSession.pageCount() > 0) {
        handleViewerSessionPageChanged();
    } else {
        setStatusMessage(m_statusMessage);
    }
}

void MainWindow::handleLanguageSelectorOpenTextEditorForLanguage(LanguageInfo info)
{
    qDebug() << "openTextEditorForLanguage:" << info.TextFile;
    QMessageBox msgBox(qApp->activeWindow());
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setTextFormat(Qt::RichText);
    QDir translationDir(qApp->getTranslationPath());
    QString message = QString("<p>You can translate QuickViewer with a text editor!</p>"
                              "<p>1. Open the file <b>\"%1\"</b><br />2. Save the file<br />3. "
                              "Select 'UserLanguage' again.</p>")
                          .arg(translationDir.filePath(info.TextFile));

    msgBox.setText(message);
    msgBox.exec();
}

void MainWindow::handleRegisterFileAssociationsActionTriggered()
{
#ifdef Q_OS_WIN
    FileAssocDialog dialog(this);
    dialog.exec();
#endif
}

void MainWindow::handleContextMenuActionTriggered()
{
    m_contextMenu->exec(QCursor::pos());
}

void MainWindow::handleAutoLoadedActionTriggered(bool checked)
{
    qApp->setAutoLoaded(checked);
}

void MainWindow::handleHistoryMenuTriggered(QAction *action)
{
    const QString path = action->data().toString();
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    if (m_exifDialog && m_exifDialog->parent()) {
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        sizes[1] = ExifDialogWidth;
        sizes[0] = sum - sizes[1];
        ui->catalogSplitter->setSizes(sizes);
    }
    QMainWindow::resizeEvent(e);
}

static int touchCount = -1;
static QTouchEvent::TouchPoint touchBegin;
static QTouchEvent::TouchPoint touchEnd;
//static QTouchEvent::TouchPoint touchPrev;
static QPoint scrollBarBegin;
static bool touchFirst = false;
static bool rescaling = false;
static int twoFingersCount = 0;

void MainWindow::touchEvent(QTouchEvent *e)
{
    qDebug() << "type:" << e->type() << "count:" << e->touchPoints().count();
    switch (e->type()) {
    case QEvent::TouchBegin:
        touchFirst = true;
        rescaling = false;
        break;
    case QEvent::TouchUpdate:
        if (touchFirst) {
            touchCount = qMax(touchCount, e->touchPoints().count());
            touchBegin = e->touchPoints().first();
            //            touchBegin = touchPrev = e->touchPoints().first();
            touchFirst = false;
            scrollBarBegin = QPoint(ui->graphicsView->horizontalScrollBar()->value(),
                                    ui->graphicsView->verticalScrollBar()->value());
            if (touchCount == 2) {
                twoFingersCount++;
            }
        } else {
            //            touchPrev = touchEnd;
            touchEnd = e->touchPoints().first();
            qreal beginx = 1.0 * touchBegin.pos().x() / ui->graphicsView->width();
            qreal beginy = 1.0 * touchBegin.pos().y() / ui->graphicsView->height();
            // finger operations are effective only in the center of the screen
            if (beginx < 0.25 || 0.75 < beginx || beginy < 0.25 || 0.75 < beginy) {
                break;
            }
            if (touchCount == 1) {
                //                ui->graphicsView->updateViewportOffset(
                //                    QPointF(
                //                        touchEnd.pos().x()-touchPrev.pos().x(),
                //                        touchEnd.pos().y()-touchPrev.pos().y()));

                //                ui->graphicsView->horizontalScrollBar()->setValue(ui->graphicsView->horizontalScrollBar()->value()-touchEnd.pos().x()+touchPrev.pos().x());
                //                ui->graphicsView->verticalScrollBar()->setValue(ui->graphicsView->verticalScrollBar()->value()-touchEnd.pos().y()+touchPrev.pos().y());

                ui->graphicsView->horizontalScrollBar()->setValue(
                    scrollBarBegin.x() - touchEnd.pos().x() + touchEnd.startPos().x());
                ui->graphicsView->verticalScrollBar()->setValue(
                    scrollBarBegin.y() - touchEnd.pos().y() + touchEnd.startPos().y());

                break;
            } else if (touchCount > 2 || e->touchPoints().count() < 2) {
                break;
            }
            // determine scale and rotate factor
            const QTouchEvent::TouchPoint &touchPoint0 = e->touchPoints().first();
            const QTouchEvent::TouchPoint &touchPoint1 = e->touchPoints().last();
            if (!rescaling) {
                // Do not process when two fingers move in parallel
                QPointF move = (touchPoint0.pos() - touchPoint0.startPos()) -
                               (touchPoint1.pos() - touchPoint1.startPos());
                rescaling = move.x() * move.x() + move.y() * move.y() > 1000;
            }
            if (rescaling) {
                qreal currentScale =
                    QLineF(touchPoint0.pos(), touchPoint1.pos()).length() /
                    QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();
                QLineF line0(touchPoint0.startPos(), touchPoint1.startPos());
                QLineF line1(touchPoint0.scenePos(), touchPoint1.scenePos());
                ui->graphicsView->updateGestureTransform(currentScale, line1.angleTo(line0));
            }
        }
        break;
    case QEvent::TouchEnd:
        int ofsX = touchEnd.pos().x() - touchBegin.pos().x();
        int ofsY = touchEnd.pos().y() - touchBegin.pos().y();
        // React only at the bottom 1/3 of the screen
        qreal endy = 1.0 * touchEnd.pos().y() / ui->graphicsView->height();
        if (touchCount == 1 && 0.75 < endy) {
            if (ofsX > 30) {
                ui->actionTurnPageOnLeft->trigger();
            } else if (ofsX < -30) {
                ui->actionTurnPageOnRight->trigger();
            }
        }
        if (touchCount == 2) {
            if (twoFingersCount >= 2) {
                // Double tap with 2 fingers to cancel scale
                ui->graphicsView->resetGestureTransform();
                twoFingersCount = 0;
            } else if (rescaling) {
                // Confirm scale with the last input content
                ui->graphicsView->commitGestureTransform();
                twoFingersCount = 0;
            } else if (ofsY < -30 && endy < 0.25) {
                ui->actionFullscreen->trigger();
            } else if (ofsX > 30 && 0.75 < endy) {
                ui->actionNextOnePage->trigger();
            } else if (ofsX < -30 && 0.75 < endy) {
                ui->actionPrevOnePage->trigger();
            }
        }
        touchCount = -1;
        break;
    }
}

void MainWindow::handleOpenVolumeWithProgressActionTriggered(bool checked)
{
    qApp->setOpenVolumeWithProgress(checked);
}

void MainWindow::handleShowReadProgressActionTriggered(bool checked)
{
    qApp->setShowReadProgress(checked);
    if (m_folderWindow) {
        m_folderWindow->reset();
    }
}

void MainWindow::handleSaveReadProgressActionTriggered(bool checked)
{
    qApp->setSaveReadProgress(checked);
}

void MainWindow::handleSaveFolderViewWidthActionTriggered(bool checked)
{
    qApp->setSaveFolderViewWidth(checked);
    if (checked) {
        saveVisibleFolderViewWidth();
    }
}

void MainWindow::resetVolumeCaption()
{
    m_volumeCaption = m_imageString.getTitleBarText();
    setWindowTitle(m_volumeCaption);
}

void MainWindow::handleCatalogWindowOpenVolume(const OpenTarget &target)
{
    openTarget(target);
    setWindowTop(false);
}

void MainWindow::loadVolumeWithAssoc(QString path)
{
    openPath(path);
    setWindowTop(!qApp->TopWindowWhenRunWithAssoc());
}

void MainWindow::handleSearchTitleWithOptionsActionTriggered(bool checked)
{
    qApp->setSearchTitleWithOptions(checked);
    if (m_catalogWindow) {
        m_catalogWindow->searchByWord(true);
    }
}

void MainWindow::handleCatalogTitleWithoutOptionsActionTriggered(bool checked)
{
    qApp->setTitleWithoutOptions(checked);
    if (m_catalogWindow) {
        m_catalogWindow->searchByWord(true);
    }
}

void MainWindow::handleCatalogViewListActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::CatalogViewMode::List);
    ui->actionCatalogViewList->setChecked(true);
    ui->actionCatalogViewIcon->setChecked(false);
    ui->actionCatalogViewIconNoText->setChecked(false);
    if (m_catalogWindow) {
        m_catalogWindow->resetViewMode();
    }
}

void MainWindow::handleCatalogViewIconActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::CatalogViewMode::Icon);
    ui->actionCatalogViewList->setChecked(false);
    ui->actionCatalogViewIcon->setChecked(true);
    ui->actionCatalogViewIconNoText->setChecked(false);
    if (m_catalogWindow) {
        m_catalogWindow->resetViewMode();
    }
}

void MainWindow::handleCatalogViewIconNoTextActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::CatalogViewMode::IconNoText);
    ui->actionCatalogViewList->setChecked(false);
    ui->actionCatalogViewIcon->setChecked(false);
    ui->actionCatalogViewIconNoText->setChecked(true);
    if (m_catalogWindow) {
        m_catalogWindow->resetViewMode();
    }
}

void MainWindow::handleShowTagBarActionTriggered(bool checked)
{
    qApp->setShowTagBar(checked);
    if (m_catalogWindow) {
        m_catalogWindow->handleShowTagBarActionTriggered(checked);
    }
}

void MainWindow::handleCatalogIconLongTextActionTriggered(bool checked)
{
    qApp->setIconLongText(checked);
    if (m_catalogWindow) {
        m_catalogWindow->resetViewMode();
    }
}

void MainWindow::handleSaveCatalogViewWidthActionTriggered(bool checked)
{
    qApp->setSaveCatalogViewWidth(checked);
}

void MainWindow::handleTurnPageOnLeftActionTriggered()
{
    if (qApp->RightSideBook()) {
        ui->actionNextPage->trigger();
    } else {
        ui->actionPrevPage->trigger();
    }
}

void MainWindow::handleTurnPageOnRightActionTriggered()
{
    if (qApp->RightSideBook()) {
        ui->actionPrevPage->trigger();
    } else {
        ui->actionNextPage->trigger();
    }
}

void MainWindow::handleOpenFolderActionTriggered()
{
    QString filter = tr("All Files( *.* );;Images ( *.jpg *.jpeg *.jpe *.png *.tif *.tiff *.ico "
                        "*.heic *.heif);;Archives( *.zip *.7z *.rar)",
                        "Text that specifies the file extension to be displayed when opening a "
                        "file with OpenFileFolder");
    QString folder = QFileDialog::getOpenFileName(
        this,
        tr("Select an image or archive",
           "Title of the dialog displayed when opening a file with OpenFileFolder"),
        qApp->LastOpenedFolderPath(),
        filter);
    //    QFileDialog dialog = QFileDialog(this, tr("Open a image folder"));
    //    if(dialog.exec()) {
    if (folder.length() > 0) {
        //qDebug() << folder;
        //        QDir dir(folder);
        //        if(dir.exists())
        //            loadVolume(folder);
        openPath(folder);
        qApp->setLastOpenedFolderPath(folder);
    }
}

void MainWindow::handleShowToolBarActionTriggered(bool checked)
{
    if (checked) {
        ui->mainToolBar->show();
    } else {
        ui->mainToolBar->hide();
    }
    qApp->setShowToolBar(checked);
}

void MainWindow::handleShowPageBarActionTriggered(bool checked)
{
    if (checked) {
        ui->pageFrame->show();
    } else {
        ui->pageFrame->hide();
    }
    qApp->setShowSliderBar(checked);
}

void MainWindow::handleShowStatusBarActionTriggered(bool checked)
{
    if (checked) {
        setWindowTitle(m_volumeCaption);
        ui->statusBar->show();
        if (m_statusMessage == StatusMessage::None) {
            ui->statusLabel->setText(m_pageCaption);
        } else {
            setStatusMessage(m_statusMessage);
        }
    } else {
        ui->statusBar->hide();
        setWindowTitle(m_pageCaption);
    }
    qApp->setShowStatusBar(checked);
}

void MainWindow::handleShowMenuBarActionTriggered(bool checked)
{
    if (checked) {
        if (qApp->ShowToolBar()) {
            ui->mainToolBar->hide();
        }
        menuBar()->show();
        if (qApp->ShowToolBar()) {
            ui->mainToolBar->show();
        }
    } else {
        menuBar()->hide();
    }
    qApp->setShowMenuBar(checked);
}

void MainWindow::handleOpenKeyConfigActionTriggered()
{
    KeyConfigDialog dialog(qApp->keyActions(), this);
    int result = dialog.exec();
    if (result == QDialog::Accepted) {
        qApp->keyActions() = dialog.keyActions();
        resetShortcutKeys();
    }
}

void MainWindow::handleOpenMouseConfigActionTriggered()
{
    MouseConfigDialog dialog(qApp->mouseActions(), this);
    int result = dialog.exec();
    if (result == QDialog::Accepted) {
        qApp->mouseActions() = dialog.mouseActions();
    }
}

void MainWindow::handleOpenOptionsDialogActionTriggered()
{
    OptionsDialog dialog(this);
    QColor back = qApp->BackgroundColor();
    QColor back2 = qApp->BackgroundColor2();
    bool checkered = qApp->UseCheckeredPattern();
    if (dialog.exec() == QDialog::Accepted) {
        dialog.reflectResults();
        if (m_viewerSession.pageCount() > 0) {
            handleViewerSessionPageChanged();
        }
        if (back != qApp->BackgroundColor() || back2 != qApp->BackgroundColor2() ||
            checkered != qApp->UseCheckeredPattern()) {
            ui->graphicsView->resetBackgroundColor();
        }
    }
}

void MainWindow::handleBeginAsFullscreenActionTriggered(bool checked)
{
    qApp->setBeginAsFullscreen(checked);
}

void MainWindow::handleShowPanelSeparateWindowActionTriggered(bool checked)
{
    qApp->setShowPanelSeparateWindow(checked);
    if (m_folderWindow) {
        createFolderWindow(!qApp->ShowPanelSeparateWindow());
    }
    if (m_catalogWindow) {
        createCatalogWindow(!qApp->ShowPanelSeparateWindow());
    }
}

template <typename MenuTypePtr>
static void setMenuAndSubmenuFont(MenuTypePtr parent, QFont font)
{
    parent->setFont(font);
    for (QObject *obj : parent->children()) {
        QMenu *menu = dynamic_cast<QMenu *>(obj);
        if (menu) {
            setMenuAndSubmenuFont(menu, font);
        }
    }
}

void MainWindow::handleLargeToolbarIconsActionTriggered(bool checked)
{
    qApp->setLargeToolbarIcons(checked);
    ui->mainToolBar->setIconSize(
        checked ? QSize(static_cast<int>(qvEnums::ToolbarIconSize::Large2Icon),
                        static_cast<int>(qvEnums::ToolbarIconSize::Large2Icon))
                : QSize(static_cast<int>(qvEnums::ToolbarIconSize::NormalIcon),
                        static_cast<int>(qvEnums::ToolbarIconSize::NormalIcon)));
    int fontsize = checked ? (int)(1.5 * m_menubarFontSize) : m_menubarFontSize;
    m_fullscreenButton->setIconSize(QSize(2 * fontsize, 2 * fontsize));
    QFont font = ui->menuBar->font();
    font.setPointSize(fontsize);

    setMenuAndSubmenuFont(ui->menuBar, font);
    setMenuAndSubmenuFont(m_contextMenu, font);
    ui->pageLabel->setFont(font);
    ui->pageLabel->setMinimumWidth(fontsize * 10);
    if (checked) {
        //		int sliderHeight = (int)(1.5*m_pageSliderHeight);
        ui->pageSlider->setMinimumHeight(m_pageSliderHeight);
        if (ui->pageFrame->isVisible()) {
            ui->pageFrame->setVisible(false);
            ui->pageFrame->setVisible(true);
        }
    } else {
        ui->pageSlider->setMinimumHeight(0);
    }
}

//void MainWindow::handleShowFullscreenTitleBarActionTriggered(bool checked)
//{
//    qApp->setShowFullscreenTitleBar(checked);
//}

void MainWindow::handleProjectWebActionTriggered()
{
    QUrl url = QString("https://kanryu.github.io/quickviewer/");
    QDesktopServices::openUrl(url);
}

void MainWindow::handleCheckVersionActionTriggered()
{
    QUrl url = QString("https://kanryu.github.io/quickviewer/checkversion/?ver=%1")
                   .arg(qApp->applicationVersion());
    QDesktopServices::openUrl(url);
}

void MainWindow::handleExitApplicationOrFullscreenActionTriggered()
{
    if (m_catalogWindow) {
        handleCatalogWindowClosed();
        return;
    }
    if (isFullScreen()) {
        ui->actionFullscreen->trigger();
    } else {
        ui->actionExit->trigger();
    }
}

void MainWindow::handleMailAttachmentActionTriggered()
{
    if (m_viewerSession.isArchive()) {
        return;
    }
    QString path = m_viewerSession.currentPagePath();
    if (!path.length()) {
        return;
    }
    setMailAttachment(path);
}

void MainWindow::handleRenameImageFileActionTriggered()
{
    if (!m_viewerSession.isFolder() || m_viewerSession.visiblePageCount() == 0) {
        return;
    }
    RenameDialog dialog(this, m_viewerSession.realVolumePath(), m_viewerSession.currentPageName());
    if (dialog.exec() == QDialog::Accepted) {
        const QString folderPath = m_viewerSession.realVolumePath();
        const QString renamedPath = QDir(folderPath).absoluteFilePath(dialog.newName());
        // The dialog renamed the file on disk, so the cached listing of its
        // folder is stale. Drop it and open the renamed file by name.
        m_viewerSession.invalidateVolumeCache(folderPath);
        openTarget(OpenTarget::fileInContainer(renamedPath));
    }
}

void MainWindow::handleConfirmDeletePageActionTriggered(bool checked)
{
    qApp->setConfirmDeletePage(checked);
}

void MainWindow::handleRecyclePageActionTriggered()
{
    if (m_viewerSession.visiblePageCount() <= 0 || !m_viewerSession.isFolder()) {
        return;
    }
    QString path = m_viewerSession.currentPagePath();
    if (!path.length()) {
        return;
    }
    if (qApp->ConfirmDeletePage()) {
        QMessageBox msgBox(this);
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.setWindowTitle(
            tr("Confirmation", "Confirm deleting image file on MessageBox title"));

        //text
        msgBox.setTextFormat(Qt::RichText);
        QString message =
            QString("<h2>%1</h2><p>%2</p>")
                .arg(tr("Are you sure you want to move the image to Recycle Bin?",
                        "Confirm putting displayed file in Recycle Box Message Box body"))
                .arg(path);
        msgBox.setText(message);

        //icon
        const VisiblePages pages = m_viewerSession.visiblePages();
        const ImageContent *page = pages.first();
        if (!page) {
            return;
        }
        QImage image = page->loadedImage;
        image = image.scaled(QSize(100, 100), Qt::KeepAspectRatio);
        msgBox.setIconPixmap(QPixmap::fromImage(image));

        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
    }
    if (moveToTrash(path)) {
        m_viewerSession.reloadVolumeAfterImageRemoval();
    }
}

void MainWindow::handleDeletePageActionTriggered()
{
    if (m_viewerSession.visiblePageCount() <= 0 || !m_viewerSession.isFolder()) {
        return;
    }
    QString path = m_viewerSession.currentPagePath();
    if (!path.length()) {
        return;
    }
    if (qApp->ConfirmDeletePage()) {
        QMessageBox msgBox(this);
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.setWindowTitle(
            tr("Confirmation", "Confirm deleting image file on MessageBox title"));

        //text
        msgBox.setTextFormat(Qt::RichText);
        QString message = QString("<h2>%1</h2><p>%2</p>")
                              .arg(tr("Are you sure you want to delete this image?",
                                      "Confirm deleting image file on Message Box body"))
                              .arg(path);
        msgBox.setText(message);

        //icon
        const VisiblePages pages = m_viewerSession.visiblePages();
        const ImageContent *page = pages.first();
        if (!page) {
            return;
        }
        QImage image = page->loadedImage;
        image = image.scaled(QSize(100, 100), Qt::KeepAspectRatio);
        msgBox.setIconPixmap(QPixmap::fromImage(image));

        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
    }
    QFile file(path);
    if (file.remove()) {
        m_viewerSession.reloadVolumeAfterImageRemoval();
    }
}

void MainWindow::handleMaximizeOrNormalActionTriggered()
{
    if (isFullScreen()) {
        ui->actionFullscreen->trigger();
    } else if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::handleRestoreWindowStateActionTriggered(bool checked)
{
    qApp->setRestoreWindowState(checked);
}

void MainWindow::handleSlideShowActionTriggered()
{
    if (m_viewerSession.pageCount() == 0) {
        return;
    }
    if (!qApp->SlideShowOnNormalWindow() && !isFullScreen()) {
        ui->actionFullscreen->trigger();
    }
    ui->graphicsView->toggleSlideShow();
}

void MainWindow::handleSlideShowStopped()
{
    ui->actionSlideShow->setChecked(false);
}

void MainWindow::handleShaderNearestNeighborActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::ShaderEffect::NearestNeighbor);
    ui->actionShaderNearestNeighbor->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderBilinearActionTriggered()
{
    uncheckAllShaderMenus();
    ui->actionShaderBilinear->setChecked(true);
    qApp->setEffect(qvEnums::ShaderEffect::Bilinear);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuBicubicActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::ShaderEffect::CpuBicubic);
    ui->actionShaderCpuBicubic->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuSpline16ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::ShaderEffect::CpuSpline16);
    ui->actionShaderCpuSpline16->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuSpline36ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::ShaderEffect::CpuSpline36);
    ui->actionShaderCpuSpline36->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuLanczos3ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::ShaderEffect::CpuLanczos3);
    ui->actionShaderCpuLanczos3->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuLanczos4ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::ShaderEffect::CpuLanczos4);
    ui->actionShaderCpuLanczos4->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleSaveBookmarkActionTriggered()
{
    if (!m_viewerSession.visiblePageCount()) {
        return;
    }
    qApp->addBookMark(storeVolumeLocation(m_viewerSession.currentLocation()));
    makeBookmarkMenu();
    ui->statusBar->showMessage(tr("Bookmark saved."));
}

void MainWindow::handleClearBookmarksActionTriggered()
{
    qApp->clearBookmarks();
    makeBookmarkMenu();
}

void MainWindow::handleLoadBookmarkActionTriggered()
{
    QWidget *widget = ui->mainToolBar->widgetForAction(ui->actionLoadBookmark);

    QPoint p = widget->mapToGlobal(QPoint(0, widget->height()));
    ui->menuLoadBookmark->exec(p);
}

void MainWindow::handleLoadBookmarkMenuTriggered(QAction *action)
{
    if (action == ui->actionClearBookmarks) {
        return;
    }
    // Bookmarks store the same page location as the last view path, so they
    // load through the entry point that also moves the folder view.
    openStoredPath(action->data().toString());
}

void MainWindow::handleSortByFileNameActionTriggered()
{
    applyImageSortBy(qvEnums::ImageSortBy::SortByFileName);
}

void MainWindow::handleSortByFileNameDescendingActionTriggered()
{
    applyImageSortBy(qvEnums::ImageSortBy::SortByFileNameDescending);
}

void MainWindow::handleSortByFileSizeActionTriggered()
{
    applyImageSortBy(qvEnums::ImageSortBy::SortByFileSize);
}

void MainWindow::handleSortByFileSizeDescendingActionTriggered()
{
    applyImageSortBy(qvEnums::ImageSortBy::SortByFileSizeDescending);
}

void MainWindow::handleSortByModifiedTimeActionTriggered()
{
    applyImageSortBy(qvEnums::ImageSortBy::SortByModifiedTime);
}

void MainWindow::handleSortByModifiedTimeDescendingActionTriggered()
{
    applyImageSortBy(qvEnums::ImageSortBy::SortByModifiedTimeDescending);
}

void MainWindow::applyImageSortBy(qvEnums::ImageSortBy sortBy)
{
    uncheckAllSortByMenus();
    switch (sortBy) {
    case qvEnums::ImageSortBy::SortByFileName:
        ui->actionSortByFileName->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByFileNameDescending:
        ui->actionSortByFileNameDescending->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByFileSize:
        ui->actionSortByFileSize->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByFileSizeDescending:
        ui->actionSortByFileSizeDescending->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByModifiedTime:
        ui->actionSortByModifiedTime->setChecked(true);
        break;
    case qvEnums::ImageSortBy::SortByModifiedTimeDescending:
        ui->actionSortByModifiedTimeDescending->setChecked(true);
        break;
    }
    if (qApp->ImageSortBy() == sortBy) {
        return;
    }
    qApp->setImageSortBy(sortBy);
    m_viewerSession.sortActiveVolumePages(sortBy);
    if (m_folderWindow) {
        m_folderWindow->resortVolumes();
    }
}
