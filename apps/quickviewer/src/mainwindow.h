#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include "models/volume.h"
#include "models/volumelocation.h"
#include "imageview.h"
#include "imagestring.h"
#include "languagemanager.h"

namespace Ui {
class MainWindow;
}
class FolderWindow;
class CatalogWindow;
class ThumbnailManager;
class RetouchWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void initializeStartup();
    virtual bool moveToTrash(QString) { return false; }
    virtual bool setStayOnTop(bool) { return false; }
    virtual void setWindowTop(bool) {}
    virtual void setMailAttachment(QString) {}
    bool eventFilter(QObject *obj, QEvent *event) override;

    /**
     * Opens a path supplied by the user or the operating system: the command
     * line, drag & drop, the history menu, bookmarks and file dialogs. The
     * intent is derived from the path itself.
     *
     * @param allowSecondPage whether an adjacent page may be shown in dual view
     */
    void openPath(QString path, bool allowSecondPage = false);
    /**
     * Opens the location described by target and keeps the folder view in sync.
     */
    void openTarget(const OpenTarget &target);
    void loadVolumeWithAssoc(QString path);

    void resetShortcutKeys();
    void makeHistoryMenu();
    void resetVolume(Volume *newVolume);
    void uncheckAllShaderMenus()
    {
        foreach (QAction *action, m_shaderMenuGroup) {
            action->setChecked(false);
        }
    }
    void uncheckAllLanguageMenus()
    {
        foreach (QAction *action, m_languageMenuGroup) {
            action->setChecked(false);
        }
    }
    void uncheckAllSortByMenus()
    {
        foreach (QAction *action, m_sortByMenuGroup) {
            action->setChecked(false);
        }
    }
    void makeBookmarkMenu();
    void setThumbnailManager(ThumbnailManager *manager);
    void resetVolumeCaption();
    void resetShortCut(const QString name, const QString shortcuttext, bool removed);

    void closeAllDockedWindow();

    // FolderWindow
    bool isFolderSearching();
    void createFolderWindow(bool docked, QString path = "", bool deferLoad = false);
    virtual bool changeFolderPath(QString path);

    // CatalogWindow
    bool isCatalogSearching();
    void createCatalogWindow(bool docked);

    // Retouch panel
    void createRetouchWindow(bool docked);

protected:
    virtual bool setStartupWindowCloaked(bool) { return false; }
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    //    void paintEvent( QPaintEvent *event ) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;
    //    void contextMenuEvent(QContextMenuEvent *e) override;
    //    void mousePressEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void touchEvent(QTouchEvent *e);

signals:
    void changingFullscreen(bool);

public slots:
    // File
    void handleAutoLoadedActionTriggered(bool checked);
    void handleClearHistoryActionTriggered();
    void handleHistoryMenuTriggered(QAction *action);
    void handleExitActionTriggered();
    void handleSavingHistoryActionTriggered(bool checked);

    // Folder
    void handleShowFolderActionTriggered();
    void handleShowSubfoldersActionTriggered(bool checked);
    void handleFolderWindowClosed();
    void handleFolderWindowOpenVolume(const OpenTarget &target);
    void handleFolderWindowReloadRequested(const QString &containerPath);
    void handleOpenVolumeWithProgressActionTriggered(bool checked);
    void handleShowReadProgressActionTriggered(bool checked);
    void handleSaveReadProgressActionTriggered(bool checked);
    void handleSaveFolderViewWidthActionTriggered(bool checked);

    // Catalog
    void handleShowCatalogActionTriggered();
    void handleCatalogWindowClosed();
    void handleCatalogWindowOpenVolume(const OpenTarget &target);
    void handleSearchTitleWithOptionsActionTriggered(bool checked);
    void handleCatalogTitleWithoutOptionsActionTriggered(bool checked);
    void handleCatalogViewListActionTriggered();
    void handleCatalogViewIconActionTriggered();
    void handleCatalogViewIconNoTextActionTriggered();
    void handleShowTagBarActionTriggered(bool checked);
    void handleCatalogIconLongTextActionTriggered(bool checked);
    void handleSaveCatalogViewWidthActionTriggered(bool checked);

    // RetouchWindow
    void handleShowRetouchWindowActionTriggered();
    void handleRetouchWindowClosed();

    // Navigation
    void handleTurnPageOnLeftActionTriggered();
    void handleTurnPageOnRightActionTriggered();

    // Exif
    void handleOpenExifActionTriggered();
    void handleExifDialogClosed();

    // PageBar
    void handleViewerSessionPageChanged();
    void handleViewerSessionVolumeChanged(QString path);
    void handlePageSliderValueChanged(int value);

    // View
    virtual void handleFullscreenActionTriggered();
    void handleStayOnTopActionTriggered(bool checked);
    void handleRestoreWindowStateActionTriggered(bool checked);
    void handleMaximizeOrNormalActionTriggered();
    void handleOpenOptionsDialogActionTriggered();
    void handleBeginAsFullscreenActionTriggered(bool checked);
    //    void handleShowFullscreenTitleBarActionTriggered(bool checked);
    void handleShowPanelSeparateWindowActionTriggered(bool checked);
    void handleLargeToolbarIconsActionTriggered(bool checked);

    // SlideShow
    void handleSlideShowActionTriggered();
    void handleSlideShowStopped();

    // Toolbars
    void handleShowToolBarActionTriggered(bool checked);
    void handleShowPageBarActionTriggered(bool checked);
    void handleShowStatusBarActionTriggered(bool checked);
    void handleShowMenuBarActionTriggered(bool checked);

    // Help
    void handleOpenKeyConfigActionTriggered();
    void handleOpenMouseConfigActionTriggered();
    void handleProjectWebActionTriggered();
    void handleCheckVersionActionTriggered();
    void handleAppVersionActionTriggered();
    void handleLanguageSelectorLanguageChanged(QString language);
    void handleLanguageSelectorOpenTextEditorForLanguage(LanguageInfo info);
    void handleRegisterFileAssociationsActionTriggered();
    void handleRegisterFileAssociationsAsAdministratorActionTriggered();

    // ContextMenus
    void handleContextMenuActionTriggered();
    void handleOpenFolderActionTriggered();
    void handleRecyclePageActionTriggered();
    void handleDeletePageActionTriggered();
    void handleExitApplicationOrFullscreenActionTriggered();
    void handleMailAttachmentActionTriggered();
    void handleRenameImageFileActionTriggered();
    void handleConfirmDeletePageActionTriggered(bool checked);

    // Shaders
    void handleShaderNearestNeighborActionTriggered();
    void handleShaderBilinearActionTriggered();
    void handleShaderBicubicActionTriggered();
    void handleShaderLanczosActionTriggered();
    void handleShaderCpuBicubicActionTriggered();
    void handleShaderCpuSpline16ActionTriggered();
    void handleShaderCpuSpline36ActionTriggered();
    void handleShaderCpuLanczos3ActionTriggered();
    void handleShaderCpuLanczos4ActionTriggered();

    // Bookmark
    void handleSaveBookmarkActionTriggered();
    void handleClearBookmarksActionTriggered();
    void handleLoadBookmarkActionTriggered();
    void handleLoadBookmarkMenuTriggered(QAction *action);

    // Sort by
    void handleSortByFileNameActionTriggered();
    void handleSortByFileNameDescendingActionTriggered();
    void handleSortByFileSizeActionTriggered();
    void handleSortByFileSizeDescendingActionTriggered();
    void handleSortByModifiedTimeActionTriggered();
    void handleSortByModifiedTimeDescendingActionTriggered();

    // Others
    virtual void handleGraphicsViewAnchorHovered(Qt::AnchorPoint anchor);
    void handleScrollModeChanged(bool scrolled);

private slots:
    void handleGraphicsViewFittingChanged(qvEnums::FitMode mode);
    void handleInitialImageDisplayFinished();
    void handleViewerLoadStatusChanged();

private:
    enum class StatusMessage {
        None,
        NoVolume,
    };

    void setStatusMessage(StatusMessage message);
    void syncPageBar();
    void applyImageSortBy(qvEnums::ImageSortBy sortBy);
    void openResolvedTarget(const OpenTarget &target, bool allowSecondPage);
    void openStoredPath(const QString &storedPath, bool allowSecondPage = false);
    void saveVisibleFolderViewWidth();
    void loadStartupVolume();
    void revealStartupWindow();
    void completeDeferredStartupWork();
    void initializeDeferredMenus();
    void initializeConfiguredStartupPanel(const QString &folderPath = QString());
    void reserveConfiguredStartupPanelSpace();
    bool replaceStartupPanelPlaceholder(QWidget *panel);
    void updateFolderViewCurrentItem();

protected:
    Ui::MainWindow *ui;
    bool m_viewerWindowStateMaximized;
    bool m_sliderChanging;
    bool m_onWindowClosing;
    bool m_revealInitialWindow;
    bool m_startupWindowCloaked;
    bool m_startupPanelInitialized;
    bool m_deferredMenusInitialized;

    /**
     * @brief m_contextMenu Define on the context menu mainwindow.ui for the main screen and separate at startup
     */
    QMenu *m_contextMenu;

    QString m_volumeCaption;
    QString m_pageCaption;
    QString m_folderViewRequestedPath;

    ViewerSession m_viewerSession;
    ImageString m_imageString;
    QList<QAction *> m_shaderMenuGroup;
    QList<QAction *> m_languageMenuGroup;
    QList<QAction *> m_sortByMenuGroup;
    ThumbnailManager *m_thumbManager;
    QWidget *m_startupPanelPlaceholder;
    FolderWindow *m_folderWindow;
    QString m_pendingFolderPath;
    CatalogWindow *m_catalogWindow;
    RetouchWindow *m_retouchWindow;
    ExifDialog *m_exifDialog;
    QToolButton *m_fullscreenButton;
    StatusMessage m_statusMessage;
    uint m_menubarFontSize;
    uint m_pageSliderHeight;
};

class ArchiveAwareMainWindow : public MainWindow
{
public:
    explicit ArchiveAwareMainWindow(QWidget *parent = nullptr)
        : MainWindow(parent)
    {
        connect(&m_viewerSession,
                &ViewerSession::archiveOpenFailed,
                this,
                [this](const QString &path, ArchiveOpenError error) {
                    m_lastArchiveOpenFailurePath = QDir::fromNativeSeparators(path);
                    m_lastArchiveOpenFailure = error;
                });
    }

    bool changeFolderPath(QString path) override
    {
        const QString volumePath = QDir::fromNativeSeparators(path);
        if (m_lastArchiveOpenFailure == ArchiveOpenError::PasswordProtected &&
            volumePath == m_lastArchiveOpenFailurePath) {
            clearArchiveOpenFailure();
            return true;
        }
        clearArchiveOpenFailure();
        return MainWindow::changeFolderPath(path);
    }

private:
    void clearArchiveOpenFailure()
    {
        m_lastArchiveOpenFailure = ArchiveOpenError::None;
        m_lastArchiveOpenFailurePath.clear();
    }

    ArchiveOpenError m_lastArchiveOpenFailure = ArchiveOpenError::None;
    QString m_lastArchiveOpenFailurePath;
};

#endif // MAINWINDOW_H
