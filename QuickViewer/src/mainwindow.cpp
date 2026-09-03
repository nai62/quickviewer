#include <QtWidgets>

#include "mainwindow.h"
#include "imageview.h"
#include "ui_mainwindow.h"
#include "fileloaderdirectory.h"
#include "qv_init.h"
#include "qvapplication.h"
#include "keyconfigdialog.h"
#include "mouseconfigdialog.h"
#include "optionsdialog.h"
#include "catalogwindow.h"
#include "folderwindow.h"
#include "renamedialog.h"
#include "exifdialog.h"
#include "qnamedpipe.h"
#include "qmousesequence.h"
#include "fileloader.h"
#include "qinnerframe.h"
#include "retouchwindow.h"

#ifdef Q_OS_WIN
#    include "fileassocdialog.h"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_viewerWindowStateMaximized(false),
      m_sliderChanging(false),
      m_onWindowClosing(false),
      m_revealInitialFullscreen(false)
      //    , contextMenu(this)
      ,
      m_pageManager(this),
      m_thumbManager(nullptr),
      m_folderWindow(nullptr),
      m_catalogWindow(nullptr),
      m_retouchWindow(nullptr),
      m_exifDialog(nullptr),
      m_fullscreenButton(nullptr)
{
    ui->setupUi(this);
    // Establish the final window size before further UI initialization can
    // expose child surfaces created with the designer geometry.
    if (!qApp->BeginAsFullscreen() && qApp->RestoreWindowState()) {
        restoreGeometry(qApp->WindowGeometry());
    }
    m_revealInitialFullscreen = qApp->BeginAsFullscreen() || isFullScreen();
    setWindowOpacity(0.0);

    m_menubarFontSize = ui->menuBar->font().pointSize();
    m_pageSliderHeight = ui->pageSlider->height();
    m_imageString.initialize(&m_pageManager, [view = ui->graphicsView] {
        return view->renderedPageMetrics();
    });

#ifndef Q_OS_WIN
    ui->actionRegisterFileAssociationsAsAdministrator->setVisible(false);
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

    ui->graphicsView->setPageManager(&m_pageManager);
    connect(&m_pageManager, &PageManager::initialImageDisplayFinished, this, &MainWindow::handleInitialImageDisplayFinished);
    setAcceptDrops(true);

    // Mapping to Key-Action Table and Key Config Dialog
    qApp->registerActions(ui);
    resetShortcutKeys();

    // Context menus(independent from menuBar)
    ui->menuBar->removeAction(ui->menuContextMenu->menuAction());
    m_contextMenu = ui->menuContextMenu;

    // setup checkable menus
    ui->actionFitting->setChecked(qApp->Fitting());
    ui->graphicsView->handleFittingActionTriggered(qApp->Fitting());
    switch (qApp->ImageSortBy()) {
    case qvEnums::SortByFileName:
        ui->actionSortByFileName->setChecked(true);
        break;
    case qvEnums::SortByFileNameDescending:
        ui->actionSortByFileNameDescending->setChecked(true);
        break;
    case qvEnums::SortByFileSize:
        ui->actionSortByFileSize->setChecked(true);
        break;
    case qvEnums::SortByFileSizeDescending:
        ui->actionSortByFileSizeDescending->setChecked(true);
        break;
    case qvEnums::SortByModifiedTime:
        ui->actionSortByModifiedTime->setChecked(true);
        break;
    case qvEnums::SortByModifiedTimeDescending:
        ui->actionSortByModifiedTimeDescending->setChecked(true);
        break;
    }
    m_sortByMenuGroup
        << ui->actionSortByFileName
        << ui->actionSortByFileNameDescending
        << ui->actionSortByFileSize
        << ui->actionSortByFileSizeDescending
        << ui->actionSortByModifiedTime
        << ui->actionSortByModifiedTimeDescending;
    switch (qApp->ImageFitMode()) {
    case qvEnums::FitToRect:
        ui->actionFitToWindow->setChecked(true);
        break;
    case qvEnums::FitToWidth:
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
    connect(m_fullscreenButton, SIGNAL(clicked(bool)), this, SLOT(handleFullscreenActionTriggered()));
    connect(ui->actionFullscreen, SIGNAL(toggled(bool)), m_fullscreenButton, SLOT(setChecked(bool)));
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
    qApp->languageSelector()->initializeMenu(ui->menuChange_Language);
    connect(qApp->languageSelector(), SIGNAL(languageChanged(QString)), this, SLOT(handleLanguageSelectorLanguageChanged(QString)));
    connect(qApp->languageSelector(), SIGNAL(openTextEditorForLanguage(LanguageInfo)), this, SLOT(handleLanguageSelectorOpenTextEditorForLanguage(LanguageInfo)));

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
    makeHistoryMenu();
    connect(ui->menuHistory, SIGNAL(triggered(QAction *)), this, SLOT(handleHistoryMenuTriggered(QAction *)));

    // Bookmarks
    makeBookmarkMenu();
    ui->actionLoadBookmark->setMenu(ui->menuLoadBookmark);
    connect(ui->menuLoadBookmark, SIGNAL(triggered(QAction *)), this, SLOT(handleLoadBookmarkMenuTriggered(QAction *)));

    // Folders
    ui->actionOpenVolumeWithProgress->setChecked(qApp->OpenVolumeWithProgress());
    ui->actionShowReadProgress->setChecked(qApp->ShowReadProgress());
    ui->actionSaveReadProgress->setChecked(qApp->SaveReadProgress());
    ui->actionSaveFolderViewWidth->setChecked(qApp->SaveFolderViewWidth());

    // Catalogs
    ui->actionCatalogIconLongText->setChecked(qApp->IconLongText());
    ui->actionSearchTitleWithOptions->setChecked(qApp->SearchTitleWithOptions());
    ui->actionCatalogTitleWithoutOptions->setChecked(qApp->TitleWithoutOptions());
    ui->actionShowTagBar->setChecked(qApp->ShowTagBar());
    ui->actionSaveCatalogViewWidth->setChecked(qApp->SaveCatalogViewWidth());

    switch (qApp->CatalogViewModeSetting()) {
    case qvEnums::List:
        ui->actionCatalogViewList->setChecked(true);
        break;
    case qvEnums::Icon:
        ui->actionCatalogViewIcon->setChecked(true);
        break;
    case qvEnums::IconNoText:
        ui->actionCatalogViewIconNoText->setChecked(true);
        break;
    }

    ui->statusBar->addPermanentWidget(ui->statusLabel);
    ui->statusLabel->setText(tr("Any folder or archive is not loaded.", "The text of the status bar to be displayed when there is no image to be displayed immediately after the application is activated"));

    // Shader
    ui->actionShaderBilinearBeforeCpuBicubic->setVisible(false);
#ifdef QV_WITHOUT_OPENGL
    ui->actionShaderBicubic->setVisible(false);
    ui->actionShaderLanczos->setVisible(false);
#endif
    m_shaderMenuGroup
        << ui->actionShaderNearestNeighbor
        << ui->actionShaderBilinear
#ifndef QV_WITHOUT_OPENGL
        << ui->actionShaderBicubic
        << ui->actionShaderLanczos
#endif
        << ui->actionShaderBilinearBeforeCpuBicubic
        << ui->actionShaderCpuBicubic
        << ui->actionShaderCpuSpline16
        << ui->actionShaderCpuSpline36
        << ui->actionShaderCpuLanczos3
        << ui->actionShaderCpuLanczos4;
    switch (qApp->Effect()) {
    case qvEnums::NearestNeighbor:
        ui->actionShaderNearestNeighbor->setChecked(true);
        break;
    case qvEnums::Bilinear:
        ui->actionShaderBilinear->setChecked(true);
        break;
#ifndef QV_WITHOUT_OPENGL
    case qvEnums::Bicubic:
        ui->actionShaderBicubic->setChecked(true);
        break;
    case qvEnums::Lanczos:
        ui->actionShaderLanczos->setChecked(true);
        break;
#endif
    case qvEnums::BilinearAndCpuBicubic:
        ui->actionShaderBilinearBeforeCpuBicubic->setChecked(true);
        break;
    case qvEnums::CpuBicubic:
        ui->actionShaderCpuBicubic->setChecked(true);
        break;
    case qvEnums::CpuSpline16:
        ui->actionShaderCpuSpline16->setChecked(true);
        break;
    case qvEnums::CpuSpline36:
        ui->actionShaderCpuSpline36->setChecked(true);
        break;
    case qvEnums::CpuLanczos3:
        ui->actionShaderCpuLanczos3->setChecked(true);
        break;
    case qvEnums::CpuLanczos4:
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

    connect(&m_pageManager, SIGNAL(pageChanged()), this, SLOT(handlePageManagerPageChanged()));
    connect(&m_pageManager, SIGNAL(volumeChanged(QString)), this, SLOT(handlePageManagerVolumeChanged(QString)));
    connect(ui->graphicsView, SIGNAL(scrollModeChanged(bool)), this, SLOT(handleScrollModeChanged(bool)));
    connect(ui->graphicsView, SIGNAL(zoomingChanged()), this, SLOT(handlePageManagerPageChanged()));
    connect(ui->graphicsView, SIGNAL(fittingChanged(qvEnums::FitMode)), this, SLOT(handleGraphicsViewFittingChanged(qvEnums::FitMode)));
    connect(ui->graphicsView, SIGNAL(slideShowStopped()), this, SLOT(handleSlideShowStopped()));

    setWindowTitle(QString("%1 v%2").arg(qApp->applicationName()).arg(qApp->applicationVersion()));
    // WindowState Restoreing
    if (qApp->BeginAsFullscreen()) {
        if (qApp->HideMouseCursorInFullscreen()) {
            ui->graphicsView->setCursor(Qt::BlankCursor);
        }
        showFullScreen();
    } else if (qApp->RestoreWindowState()) {
        restoreState(qApp->WindowState());
    }
    if (isFullScreen()) {
        menuBar()->hide();
        ui->mainToolBar->hide();
        ui->pageFrame->hide();
        statusBar()->hide();
        ui->actionFullscreen->setChecked(true);
        ui->graphicsView->setFullscreenState(true);
        ui->graphicsView->refreshRenderedPages();
    }

    if (!m_revealInitialFullscreen) {
        // Build and paint a normal window before starting any potentially
        // blocking image or archive load.
        if (!isVisible()) {
            show();
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        if (layout()) {
            layout()->activate();
        }
        ui->graphicsView->refreshRenderedPages();
        repaint();
        setWindowOpacity(1.0);
    }
}

void MainWindow::initializeStartup()
{
    // Platform-specific virtual functions must be invoked after the most-derived
    // MainWindow has finished construction.
    handleStayOnTopActionTriggered(qApp->StayOnTop());

    // when drop a folder/archive icon to this app
    if (qApp->arguments().length() >= 2) {
        loadVolume(qApp->arguments().last());
        setWindowTop(!qApp->TopWindowWhenRunWithAssoc());
        return;
    }
    // auto restore
    if (qApp->AutoLoaded() && !qApp->LastViewPath().isEmpty()) {
        QString bookmark = qApp->LastViewPath();
        loadVolume(bookmark, true);
        makeBookmarkMenu();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!m_revealInitialFullscreen) {
        return;
    }

    // Full-screen state and geometry are applied asynchronously by the native
    // window system. Keep the initial designer-sized surface transparent until
    // the full-screen show has crossed event-loop boundaries and been painted.
    QTimer::singleShot(0, this, [this]() {
        if (!m_revealInitialFullscreen) {
            return;
        }
        if (!isFullScreen()) {
            m_revealInitialFullscreen = false;
            setWindowOpacity(1.0);
            return;
        }
        if (layout()) {
            layout()->activate();
        }
        ui->graphicsView->refreshRenderedPages();
        repaint();
        QTimer::singleShot(0, this, [this]() {
            if (!m_revealInitialFullscreen) {
                return;
            }
            m_revealInitialFullscreen = false;
            setWindowOpacity(1.0);
        });
    });
}

MainWindow::~MainWindow()
{
    if (qApp->AutoLoaded() && m_pageManager.currentPageCount() > 0) {
        QString path = QDir::fromNativeSeparators(m_pageManager.currentPagePath());
        qApp->setLastViewPath(path);
    }
    delete ui;
    m_pageManager.dispose();
    qApp->saveSettings();
}

void MainWindow::resetShortcutKeys()
{
    QMap<QString, QAction *> &actions = qApp->keyActions().actions();
    QMap<QString, QKeySequence> &seqMap = qApp->keyActions().keyMaps();
    foreach (const QString &name, actions.keys()) {
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
            loadVolume(QDir::toNativeSeparators(url.toLocalFile()));
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
        delta = -Q_MOUSE_DELTA;
    } else if (delta_y > 0) {
        delta = Q_MOUSE_DELTA;
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
    qDebug() << seq.toString() << focusWidget();

    if (this->focusWidget() != ui->graphicsView) {
        return;
    }
    if (ui->graphicsView->isScrollMode() && !qApp->ScrollWithCursorWhenZooming()) {
        if (seq.toString() == "Left") {
            ui->graphicsView->horizontalScrollBar()->setValue(ui->graphicsView->horizontalScrollBar()->value() - 300);
            return;
        }
        if (seq.toString() == "Right") {
            ui->graphicsView->horizontalScrollBar()->setValue(ui->graphicsView->horizontalScrollBar()->value() + 300);
            return;
        }
        if (seq.toString() == "Up") {
            ui->graphicsView->verticalScrollBar()->setValue(ui->graphicsView->verticalScrollBar()->value() - 300);
            return;
        }
        if (seq.toString() == "Down") {
            ui->graphicsView->verticalScrollBar()->setValue(ui->graphicsView->verticalScrollBar()->value() + 300);
            return;
        }
    }

    QAction *action = qApp->keyActions().getActionByKey(seq);
    if (action) {
        action->trigger();
        event->accept();
        return;
    }
}

void MainWindow::closeEvent(QCloseEvent *)
{
    m_onWindowClosing = true;
    delete m_contextMenu;
    m_contextMenu = nullptr;
    qApp->setWindowGeometry(saveGeometry());
    qApp->setWindowState(saveState());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    QKeyEvent *keyEvent = NULL;//event data, if this is a keystroke event
    QMouseEvent *mouseEvent = NULL;//event data, if this is a keystroke event
    //QDragEnterEvent  *dragEnterEvent = NULL;//event data, if this is a keystroke event
    //QDropEvent *dropEvent = NULL;//event data, if this is a keystroke event

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
            if ((mouseEvent->buttons() & Qt::RightButton) && (mouseEvent->buttons() & ~Qt::RightButton)) {
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

void MainWindow::loadVolume(QString path, bool allowSecondPage)
{
    QStringList seps = path.split("::");
    if (!IFileLoader::isArchiveFile(seps[0]) && IFileLoader::isImageFile(path)) {
        m_pageManager.loadVolumeWithFile(path, allowSecondPage);
        changeFolderPath(QFileInfo(QDir::fromNativeSeparators(path)).absolutePath());
        return;
    }
    if (m_pageManager.loadVolume(path)) {
        if (m_pageManager.isArchive()) {
            m_pageManager.deferFolderWorkUntilNextPaint();
        }
        changeFolderPath(m_pageManager.volumePath());
        return;
    }

    if (changeFolderPath(path)) {
        return;
    }

    createFolderWindow(true, path);
    ui->statusLabel->setText(tr("Image file not found. Can't be opened", "Text to display in the status bar when failed to open the specified Volume"));
}

void MainWindow::makeHistoryMenu()
{
    static const QString shortcuts = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    ui->menuHistory->clear();
    QStringList history = qApp->History();
    for (int i = 0; i < history.size(); i++) {
        QString text = QString("&%1: %2").arg(shortcuts.mid(i, 1)).arg(history.at(i));
        ui->menuHistory->addAction(text);
    }
}

void MainWindow::makeBookmarkMenu()
{
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

void MainWindow::setThumbnailManager(ThumbnailManager *manager)
{
    m_thumbManager = manager;

    switch (qApp->ShowOptionViewOnStartup()) {
    case qvEnums::NoViewStartup:
        break;
    case qvEnums::FolderStartup:
        createFolderWindow(!qApp->ShowPanelSeparateWindow());
        break;
    case qvEnums::CatalogStartup:
        createCatalogWindow(!qApp->ShowPanelSeparateWindow());
        break;
    case qvEnums::RetouchStartup:
        createRetouchWindow(!qApp->ShowPanelSeparateWindow());
        break;
    }
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
    bool showMenubar = fullscreen ? !qApp->HideMenuBarInFullscreen() : (!qApp->ShowMenuBar() && !qApp->HideMenuBarParmanently());
    bool showToolbar = fullscreen ? !qApp->HideToolBarInFullscreen() : (!qApp->ShowToolBar() && !qApp->HideToolBarParmanently());
    bool showPageBar = fullscreen ? !qApp->HidePageBarInFullscreen() : (!qApp->ShowSliderBar() && !qApp->HidePageBarParmanently());
    if (!showToolbar && !showMenubar && !showPageBar) {
        return;
    }
    if (anchor == Qt::AnchorTop && (showMenubar || showToolbar)) {
        QInnerFrame *innerFrame = new QInnerFrame(ui->graphicsView);
        connect(innerFrame, &QInnerFrame::init, this, [=] {
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
        connect(innerFrame, &QInnerFrame::deinit, this, [=] {
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
        connect(this, SIGNAL(changingFullscreen(bool)), innerFrame, SLOT(close()));
        connect(innerFrame, &QInnerFrame::closed, this, [=] {
            delete innerFrame;
        });
        innerFrame->showWithoutTitleBar();
    }
    if (anchor == Qt::AnchorBottom && !qApp->HidePageBarParmanently() && (showPageBar || fullscreen)) {
        QInnerFrame *innerFrame = new QInnerFrame(ui->graphicsView, Qt::AnchorBottom, qApp->LargeToolbarIcons() ? 60 : 30);
        connect(innerFrame, &QInnerFrame::init, this, [&] {
            innerFrame->layout()->addWidget(ui->pageFrame);
            ui->pageFrame->show();
            qApp->setInnerFrameShowing(true);
            ui->pageFrame->setCursor(Qt::ArrowCursor);
        });
        connect(innerFrame, &QInnerFrame::deinit, this, [&] {
            ui->pageFrame->hide();
            ui->verticalViewPage->layout()->addWidget(ui->pageFrame);
            qApp->setInnerFrameShowing(false);
        });
        connect(this, SIGNAL(changingFullscreen(bool)), innerFrame, SLOT(close()));
        connect(innerFrame, &QInnerFrame::closed, this, [=] {
            delete innerFrame;
        });
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
    foreach (const QString &c, cusors) {
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

void MainWindow::handleFolderWindowClosed()
{
    if (m_folderWindow) {
        delete m_folderWindow;
        m_folderWindow = nullptr;
        ui->actionShowFolder->setChecked(false);

        if (!m_onWindowClosing) {
            qApp->setShowOptionViewOnStartup(qvEnums::NoViewStartup);
        }
    }
}

bool MainWindow::isFolderSearching()
{
    if (!m_folderWindow || !m_folderWindow->parent()) {
        return false;
    }
    return true;
}

void MainWindow::handleFolderWindowOpenVolume(QString path)
{
    loadVolume(path);
}

void MainWindow::createFolderWindow(bool docked, QString path)
{
    const bool deferFolderLoad = m_pageManager.initialImagePaintPending();
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
        oldpath = m_pageManager.volumePath();
        if (oldpath.isEmpty()) {
            oldpath = qApp->HomeFolderPath();
        }
    }
    qApp->setShowOptionViewOnStartup(qvEnums::FolderStartup);
    if (docked) {
        closeAllDockedWindow();
        int lastwidth = qApp->FolderViewWidth();
        m_folderWindow = new FolderWindow(nullptr, ui);
        if (!deferFolderLoad) {
            m_folderWindow->setFolderPath(oldpath, false);
        }
        connect(m_folderWindow, SIGNAL(closed()), this, SLOT(handleFolderWindowClosed()));
        connect(m_folderWindow, SIGNAL(openVolume(QString)), this, SLOT(handleFolderWindowOpenVolume(QString)));
        connect(&m_pageManager, SIGNAL(volumeChanged(QString)), m_folderWindow, SLOT(handlePageManagerVolumeChanged(QString)));
        ui->catalogSplitter->insertWidget(0, m_folderWindow);
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        sizes[0] = qApp->SaveFolderViewWidth() ? lastwidth : 200;
        sizes[1] = sum - sizes[0];
        ui->catalogSplitter->setSizes(sizes);
        m_folderWindow->setAsInnerWidget();
    } else {
        // close child widget, and recreate as independent window
        m_folderWindow = new FolderWindow(nullptr, ui);
        QRect self = geometry();
        m_folderWindow->setGeometry(self.left() - 100, self.top() + 100, self.width(), self.height());
        m_folderWindow->setAsToplevelWindow();
        if (!deferFolderLoad) {
            m_folderWindow->setFolderPath(oldpath, false);
        }
        connect(m_folderWindow, SIGNAL(closed()), this, SLOT(handleFolderWindowClosed()));
        connect(m_folderWindow, SIGNAL(openVolume(QString)), this, SLOT(handleFolderWindowOpenVolume(QString)));
        connect(&m_pageManager, SIGNAL(volumeChanged(QString)), m_folderWindow, SLOT(handlePageManagerVolumeChanged(QString)));
        m_folderWindow->show();
    }
    ui->actionShowFolder->setChecked(true);
}

bool MainWindow::changeFolderPath(QString path)
{
    if (m_pageManager.initialImagePaintPending()) {
        m_pendingFolderPath = path;
        return false;
    }
    if (!m_folderWindow) {
        // An image passed on the command line is loaded before setThumbnailManager()
        // creates the startup FolderWindow. Preserve its directory until then.
        m_pendingFolderPath = path;
        return false;
    }
    m_folderWindow->setFolderPath(path, false);
    return true;
}

void MainWindow::handleInitialImageDisplayFinished()
{
    if (m_pendingFolderPath.isEmpty()) {
        return;
    }

    if (m_folderWindow) {
        const QString path = m_pendingFolderPath;
        m_pendingFolderPath.clear();
        m_folderWindow->setFolderPath(path, false);
        return;
    }

    if (m_thumbManager && qApp->ShowOptionViewOnStartup() == qvEnums::FolderStartup) {
        const QString path = m_pendingFolderPath;
        createFolderWindow(!qApp->ShowPanelSeparateWindow(), path);
    }
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
            qApp->setShowOptionViewOnStartup(qvEnums::NoViewStartup);
        }
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
    qApp->setShowOptionViewOnStartup(qvEnums::CatalogStartup);
    if (docked) {
        closeAllDockedWindow();
        int lastwidth = qApp->CatalogViewWidth();
        m_catalogWindow = new CatalogWindow(nullptr, ui);
        m_catalogWindow->setThumbnailManager(m_thumbManager);
        connect(m_catalogWindow, SIGNAL(closed()), this, SLOT(handleCatalogWindowClosed()));
        connect(m_catalogWindow, SIGNAL(openVolume(QString)), this, SLOT(handleCatalogWindowOpenVolume(QString)));
        ui->catalogSplitter->insertWidget(0, m_catalogWindow);
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        sizes[0] = qApp->SaveCatalogViewWidth() ? lastwidth : 200;
        sizes[1] = sum - sizes[0];
        ui->catalogSplitter->setSizes(sizes);
        m_catalogWindow->setAsInnerWidget();
    } else {
        m_catalogWindow = new CatalogWindow(nullptr, ui);
        m_catalogWindow->setThumbnailManager(m_thumbManager);
        connect(m_catalogWindow, SIGNAL(closed()), this, SLOT(handleCatalogWindowClosed()));
        connect(m_catalogWindow, SIGNAL(openVolume(QString)), this, SLOT(handleCatalogWindowOpenVolume(QString)));
        m_catalogWindow->setAsToplevelWindow();
        QRect self = geometry();
        m_catalogWindow->setGeometry(self.left() - 100, self.top() + 100, self.width(), self.height());
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
            qApp->setShowOptionViewOnStartup(qvEnums::NoViewStartup);
        }
    }
}

void MainWindow::createRetouchWindow(bool docked)
{
    if (m_retouchWindow) {
        handleRetouchWindowClosed();
    }
    if (m_pageManager.visiblePages().isEmpty()) {
        return;
    }
    qApp->setShowOptionViewOnStartup(qvEnums::RetouchStartup);
    if (docked) {
        closeAllDockedWindow();
        m_retouchWindow = new RetouchWindow(nullptr);
        connect(m_retouchWindow, SIGNAL(closed()), this, SLOT(handleRetouchWindowClosed()));
        connect(m_retouchWindow, SIGNAL(retouchParametersChanged(ImageRetouch)), ui->graphicsView, SLOT(handleRetouchParametersChanged(ImageRetouch)));
        m_retouchWindow->setImageView(ui->graphicsView);
        ui->catalogSplitter->insertWidget(0, m_retouchWindow);
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        sizes[0] = 200;
        sizes[1] = sum - sizes[0];
        ui->catalogSplitter->setSizes(sizes);
    } else {
        m_retouchWindow = new RetouchWindow(nullptr);
        connect(m_retouchWindow, SIGNAL(closed()), this, SLOT(handleRetouchWindowClosed()));
        connect(m_retouchWindow, SIGNAL(retouchParametersChanged(ImageRetouch)), ui->graphicsView, SLOT(handleRetouchParametersChanged(ImageRetouch)));
        m_retouchWindow->setImageView(ui->graphicsView);
        QRect self = geometry();
        m_retouchWindow->setGeometry(self.left() - 100, self.top() + 100, self.width(), self.height());
        m_retouchWindow->show();
    }
    ui->actionShowRetouchWindow->setChecked(true);
}

////////////////////////////
//// ExifDialog
////////////////////////////
#define EXIF_DIALOG_WIDTH 280

void MainWindow::handleOpenExifActionTriggered()
{
    if (m_exifDialog || m_pageManager.currentPageCount() == 0) {
        return;
    }
    const VisiblePages pages = m_pageManager.visiblePages();
    const ImageContent *page = pages.first();
    if (!page || page->Info.ImageWidth == 0) {
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
    connect(m_exifDialog, SIGNAL(closed()), this, SLOT(handleExifDialogClosed()));

    ui->catalogSplitter->insertWidget(1, m_exifDialog);
    auto sizes = ui->catalogSplitter->sizes();
    int sum = sizes[0] + sizes[1];
    sizes[1] = EXIF_DIALOG_WIDTH;
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

    bool full = isFullScreen();
    setWindowFlags(flags);
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
    ui->actionFitToWindow->setChecked(mode == qvEnums::FitToRect);
    ui->actionFitToWidth->setChecked(mode == qvEnums::FitToWidth);
}

void MainWindow::handlePageManagerPageChanged()
{
    //qDebug() << "handlePageManagerPageChanged";
    int maxVolume = m_pageManager.size();
    if (maxVolume <= 0) {
        return;
    }
    // PageSlider
    ui->pageLabel->setText(m_pageManager.currentPageNumAsString());
    m_sliderChanging = true;

    // at DualView Mode, last 2 page should be [volume.size()-2, volume.size()-1]
    // so the last page should not changed by the slider
    // the logical last page is [volume.size()-2]
    if (qApp->DualView() && ((m_pageManager.size() - m_pageManager.currentPage()) & 0x1) == 0) {
        maxVolume--;
    }

    ui->pageSlider->setMaximum(maxVolume);
    ui->pageSlider->setValue(m_pageManager.currentPage() + 1);
    m_sliderChanging = false;

    // StatusBar
    //    m_pageCaption = m_pageManager.currentPageStatusAsString();
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
        const VisiblePages pages = m_pageManager.visiblePages();
        if (const ImageContent *page = pages.first()) {
            m_exifDialog->setExif(*page);
        }
    }
}

void MainWindow::handlePageManagerVolumeChanged(QString path)
{
    if (path.isEmpty()) {
        handlePageNoLongerNeeded();
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
    //qDebug() << "handlePageSliderValueChanged " << value << m_sliderChanging;
    if (m_sliderChanging) {
        return;
    }
    m_sliderChanging = true;
    m_pageManager.selectPage(value - 1);
    m_sliderChanging = false;
}

void MainWindow::handlePageNoLongerNeeded()
{
    setWindowTitle(QString("%1 v%2").arg(qApp->applicationName()).arg(qApp->applicationVersion()));
    ui->pageFrame->hide();
    ui->statusLabel->setText(tr("Image file was not found. Can't be opened.", "Text to display in the status bar when failed to open the specified Volume"));
}

void MainWindow::handleAppVersionActionTriggered()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString("about %1").arg(QApplication::applicationName()));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setTextFormat(Qt::RichText);
    //    msgBox.setText(QApplication::applicationVersion());
    QString message = QString("<h1>%1 %2</h1><p>%3&lt;<a href=\"mailto:k.kanryu@gmail.com\">k.kanryu@gmail.com&gt;</a> All rights reserved.</p>"
                              "<p>Project Webpage: <a href=\"https://kanryu.github.io/quickviewer/\">https://kanryu.github.io/quickviewer/</a></p>"
                              "<p>This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.</p>")
                          .arg(QApplication::applicationName())
                          .arg(QApplication::applicationVersion())
                          .arg(APP_COPYRIGHT);
    msgBox.setText(message);
    msgBox.exec();
}

void MainWindow::handleLanguageSelectorLanguageChanged(QString language)
{
    qApp->setUiLanguage(language);
    ui->retranslateUi(this);
    ui->menuLoadBookmark->setTitle(QApplication::translate("MainWindow", "LoadBookmark", Q_NULLPTR));
}

void MainWindow::handleLanguageSelectorOpenTextEditorForLanguage(LanguageInfo info)
{
    qDebug() << "openTextEditorForLanguage:" << info.TextFile;
    QMessageBox msgBox(qApp->activeWindow());
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setTextFormat(Qt::RichText);
    QDir translationDir(qApp->getTranslationPath());
    QString message = QString("<p>You can translate QuickViewer with a text editor!</p>"
                              "<p>1. Open the file <b>\"%1\"</b><br />2. Save the file<br />3. Select 'UserLanguage' again.</p>")
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

void MainWindow::handleRegisterFileAssociationsAsAdministratorActionTriggered()
{
    QProcess::startDetached(qApp->getApplicationFilePath("AssociateFilesWithQuickViewer.exe"),
                            QStringList(),
                            QDir::toNativeSeparators(qApp->applicationDirPath()));
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
    //qDebug() << action;
    loadVolume(action->text().mid(4));
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    if (m_exifDialog && m_exifDialog->parent()) {
        auto sizes = ui->catalogSplitter->sizes();
        int sum = sizes[0] + sizes[1];
        sizes[1] = EXIF_DIALOG_WIDTH;
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
            scrollBarBegin = QPoint(ui->graphicsView->horizontalScrollBar()->value(), ui->graphicsView->verticalScrollBar()->value());
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

                ui->graphicsView->horizontalScrollBar()->setValue(scrollBarBegin.x() - touchEnd.pos().x() + touchEnd.startPos().x());
                ui->graphicsView->verticalScrollBar()->setValue(scrollBarBegin.y() - touchEnd.pos().y() + touchEnd.startPos().y());

                break;
            } else if (touchCount > 2 || e->touchPoints().count() < 2) {
                break;
            }
            // determine scale and rotate factor
            const QTouchEvent::TouchPoint &touchPoint0 = e->touchPoints().first();
            const QTouchEvent::TouchPoint &touchPoint1 = e->touchPoints().last();
            if (!rescaling) {
                // Do not process when two fingers move in parallel
                QPointF move = (touchPoint0.pos() - touchPoint0.startPos()) - (touchPoint1.pos() - touchPoint1.startPos());
                rescaling = move.x() * move.x() + move.y() * move.y() > 1000;
            }
            if (rescaling) {
                qreal currentScale =
                    QLineF(touchPoint0.pos(), touchPoint1.pos()).length() / QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();
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
}

void MainWindow::resetVolumeCaption()
{
    m_volumeCaption = m_imageString.getTitleBarText();
    setWindowTitle(m_volumeCaption);
}

void MainWindow::handleCatalogWindowOpenVolume(QString path)
{
    loadVolume(path);
    setWindowTop(false);
}

void MainWindow::loadVolumeWithAssoc(QString path)
{
    loadVolume(path);
    setWindowTop(!qApp->TopWindowWhenRunWithAssoc());
}

void MainWindow::handleSearchTitleWithOptionsActionTriggered(bool checked)
{
    qApp->setSearchTitleWithOptions(checked);
    if (m_catalogWindow) {
        m_catalogWindow->resetVolumes();
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
    qApp->setCatalogViewModeSetting(qvEnums::List);
    ui->actionCatalogViewList->setChecked(true);
    ui->actionCatalogViewIcon->setChecked(false);
    ui->actionCatalogViewIconNoText->setChecked(false);
    if (m_catalogWindow) {
        m_catalogWindow->resetVolumes();
    }
}

void MainWindow::handleCatalogViewIconActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::Icon);
    ui->actionCatalogViewList->setChecked(false);
    ui->actionCatalogViewIcon->setChecked(true);
    ui->actionCatalogViewIconNoText->setChecked(false);
    if (m_catalogWindow) {
        m_catalogWindow->resetVolumes();
    }
}

void MainWindow::handleCatalogViewIconNoTextActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::IconNoText);
    ui->actionCatalogViewList->setChecked(false);
    ui->actionCatalogViewIcon->setChecked(false);
    ui->actionCatalogViewIconNoText->setChecked(true);
    if (m_catalogWindow) {
        m_catalogWindow->resetVolumes();
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
    QString filter = tr("All Files( *.* );;Images ( *.jpg *.jpeg *.jpe *.png *.tif *.tiff *.ico *.heic *.heif);;Archives( *.zip *.7z *.rar)", "Text that specifies the file extension to be displayed when opening a file with OpenFileFolder");
    QString folder = QFileDialog::getOpenFileName(
        this,
        tr("Please select the image or archive", "Title of the dialog displayed when opening a file with OpenFileFolder"),
        qApp->LastOpenedFolderPath(),
        filter);
    //    QFileDialog dialog = QFileDialog(this, tr("Open a image folder"));
    //    if(dialog.exec()) {
    if (folder.length() > 0) {
        //qDebug() << folder;
        //        QDir dir(folder);
        //        if(dir.exists())
        //            loadVolume(folder);
        loadVolume(folder);
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
        ui->statusLabel->setText(m_pageCaption);
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
        if (m_pageManager.size() > 0) {
            handlePageManagerPageChanged();
        }
        if (back != qApp->BackgroundColor() || back2 != qApp->BackgroundColor2() || checkered != qApp->UseCheckeredPattern()) {
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
    foreach (QObject *obj, parent->children()) {
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
        checked ? QSize(qvEnums::Large2Icon, qvEnums::Large2Icon)
                : QSize(qvEnums::NormalIcon, qvEnums::NormalIcon));
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
    QUrl url = QString("https://kanryu.github.io/quickviewer/checkversion/?ver=%1").arg(qApp->applicationVersion());
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
    if (m_pageManager.isArchive()) {
        return;
    }
    QString path = m_pageManager.currentPagePath();
    if (!path.length()) {
        return;
    }
    setMailAttachment(path);
}

void MainWindow::handleRenameImageFileActionTriggered()
{
    if (!m_pageManager.isFolder() || m_pageManager.currentPageCount() == 0) {
        return;
    }
    RenameDialog dialog(this, m_pageManager.realVolumePath(), m_pageManager.currentPageName());
    if (dialog.exec() == QDialog::Accepted) {
        m_pageManager.loadVolume(QDir(m_pageManager.realVolumePath()).absoluteFilePath(dialog.newName()));
    }
}

void MainWindow::handleConfirmDeletePageActionTriggered(bool checked)
{
    qApp->setConfirmDeletePage(checked);
}

void MainWindow::handleRecyclePageActionTriggered()
{
    if (m_pageManager.currentPageCount() <= 0 || !m_pageManager.isFolder()) {
        return;
    }
    QString path = m_pageManager.currentPagePath();
    if (!path.length()) {
        return;
    }
    if (qApp->ConfirmDeletePage()) {
        QMessageBox msgBox(this);
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.setWindowTitle(tr("Confirmation", "Confirm deleting image file on MessageBox title"));

        //text
        msgBox.setTextFormat(Qt::RichText);
        QString message = QString("<h2>%1</h2><p>%2</p>")
                              .arg(tr("Are you sure you want to move the image to Recycle Bin?", "Confirm putting displayed file in Recycle Box Message Box body"))
                              .arg(path);
        msgBox.setText(message);

        //icon
        const VisiblePages pages = m_pageManager.visiblePages();
        const ImageContent *page = pages.first();
        if (!page) {
            return;
        }
        QImage image = page->Image;
        image = image.scaled(QSize(100, 100), Qt::KeepAspectRatio);
        msgBox.setIconPixmap(QPixmap::fromImage(image));

        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
    }
    if (moveToTrash(path)) {
        m_pageManager.reloadVolumeAfterRemoveImage();
    }
}

void MainWindow::handleDeletePageActionTriggered()
{
    if (m_pageManager.currentPageCount() <= 0 || !m_pageManager.isFolder()) {
        return;
    }
    QString path = m_pageManager.currentPagePath();
    if (!path.length()) {
        return;
    }
    if (qApp->ConfirmDeletePage()) {
        QMessageBox msgBox(this);
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.setWindowTitle(tr("Confirmation", "Confirm deleting image file on MessageBox title"));

        //text
        msgBox.setTextFormat(Qt::RichText);
        QString message = QString("<h2>%1</h2><p>%2</p>")
                              .arg(tr("Are you sure you want to delete this image?", "Confirm deleting image file on Message Box body"))
                              .arg(path);
        msgBox.setText(message);

        //icon
        const VisiblePages pages = m_pageManager.visiblePages();
        const ImageContent *page = pages.first();
        if (!page) {
            return;
        }
        QImage image = page->Image;
        image = image.scaled(QSize(100, 100), Qt::KeepAspectRatio);
        msgBox.setIconPixmap(QPixmap::fromImage(image));

        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
    }
    QFile file(path);
    if (file.remove()) {
        m_pageManager.reloadVolumeAfterRemoveImage();
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
    if (m_pageManager.size() == 0) {
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
    qApp->setEffect(qvEnums::NearestNeighbor);
    ui->actionShaderNearestNeighbor->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderBilinearActionTriggered()
{
    uncheckAllShaderMenus();
    ui->actionShaderBilinear->setChecked(true);
    qApp->setEffect(qvEnums::Bilinear);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderBicubicActionTriggered()
{
#ifndef QV_WITHOUT_OPENGL
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::Bicubic);
    ui->actionShaderBicubic->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
#endif
}

void MainWindow::handleShaderLanczosActionTriggered()
{
#ifndef QV_WITHOUT_OPENGL
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::Lanczos);
    ui->actionShaderLanczos->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
#endif
}

void MainWindow::handleShaderBilinearBeforeCpuBicubicActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::BilinearAndCpuBicubic);
    ui->actionShaderBilinearBeforeCpuBicubic->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuBicubicActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::CpuBicubic);
    ui->actionShaderCpuBicubic->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuSpline16ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::CpuSpline16);
    ui->actionShaderCpuSpline16->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuSpline36ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::CpuSpline36);
    ui->actionShaderCpuSpline36->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuLanczos3ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::CpuLanczos3);
    ui->actionShaderCpuLanczos3->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleShaderCpuLanczos4ActionTriggered()
{
    uncheckAllShaderMenus();
    qApp->setEffect(qvEnums::CpuLanczos4);
    ui->actionShaderCpuLanczos4->setChecked(true);
    ui->graphicsView->refreshRenderedPages();
}

void MainWindow::handleSaveBookmarkActionTriggered()
{
    if (!m_pageManager.currentPageCount()) {
        return;
    }
    QString path = QDir::fromNativeSeparators(m_pageManager.currentPagePath());
    qApp->addBookMark(path);
    makeBookmarkMenu();
    ui->statusBar->showMessage(tr("Bookmark Saved."));
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
    QString path = action->data().toString();
    m_pageManager.loadVolume(QDir::toNativeSeparators(path));
}

void MainWindow::handleSortByFileNameActionTriggered()
{
    uncheckAllSortByMenus();
    ui->actionSortByFileName->setChecked(true);
    if (qApp->ImageSortBy() == qvEnums::SortByFileName) {
        return;
    }
    qApp->setImageSortBy(qvEnums::SortByFileName);
    m_pageManager.sort(qvEnums::SortByFileName);
}

void MainWindow::handleSortByFileNameDescendingActionTriggered()
{
    uncheckAllSortByMenus();
    ui->actionSortByFileNameDescending->setChecked(true);
    if (qApp->ImageSortBy() == qvEnums::SortByFileNameDescending) {
        return;
    }
    qApp->setImageSortBy(qvEnums::SortByFileNameDescending);
    m_pageManager.sort(qvEnums::SortByFileNameDescending);
}

void MainWindow::handleSortByFileSizeActionTriggered()
{
    uncheckAllSortByMenus();
    ui->actionSortByFileSize->setChecked(true);
    if (qApp->ImageSortBy() == qvEnums::SortByFileSize) {
        return;
    }
    qApp->setImageSortBy(qvEnums::SortByFileSize);
    m_pageManager.sort(qvEnums::SortByFileSize);
}

void MainWindow::handleSortByFileSizeDescendingActionTriggered()
{
    uncheckAllSortByMenus();
    ui->actionSortByFileSizeDescending->setChecked(true);
    if (qApp->ImageSortBy() == qvEnums::SortByFileSizeDescending) {
        return;
    }
    qApp->setImageSortBy(qvEnums::SortByFileSizeDescending);
    m_pageManager.sort(qvEnums::SortByFileSizeDescending);
}

void MainWindow::handleSortByModifiedTimeActionTriggered()
{
    uncheckAllSortByMenus();
    ui->actionSortByModifiedTime->setChecked(true);
    if (qApp->ImageSortBy() == qvEnums::SortByModifiedTime) {
        return;
    }
    qApp->setImageSortBy(qvEnums::SortByModifiedTime);
    m_pageManager.sort(qvEnums::SortByModifiedTime);
}

void MainWindow::handleSortByModifiedTimeDescendingActionTriggered()
{
    uncheckAllSortByMenus();
    ui->actionSortByModifiedTimeDescending->setChecked(true);
    if (qApp->ImageSortBy() == qvEnums::SortByModifiedTimeDescending) {
        return;
    }
    qApp->setImageSortBy(qvEnums::SortByModifiedTimeDescending);
    m_pageManager.sort(qvEnums::SortByModifiedTimeDescending);
}
