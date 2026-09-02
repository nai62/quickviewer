#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include "models/volumemanager.h"
#include "imageview.h"
#include "imagestring.h"
#include "qlanguageselector.h"

namespace Ui {
class MainWindow;
}
class FolderWindow;
class CatalogWindow;
class ThumbnailManager;
class BrightnessWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    virtual bool moveToTrash(QString) { return false; }
    virtual bool setStayOnTop(bool) { return false; }
    virtual void setWindowTop(bool) {}
    virtual void setMailAttachment(QString) {}
    virtual bool eventFilter(QObject *obj, QEvent *event);

    /**
     * @brief loadVolume
     * @param allowSecondPage whether an adjacent page may be shown in dual view
     */
    void loadVolume(QString path, bool allowSecondPage = false);
    void loadVolumeWithAssoc(QString path);

    void resetShortcutKeys();
    void makeHistoryMenu();
    void resetVolume(VolumeManager *newer);
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
    void createFolderWindow(bool docked, QString path = "");
    bool changeFolderPath(QString path);

    // CatalogWindow
    bool isCatalogSearching();
    void createCatalogWindow(bool docked);

    // Retouch panel
    void createBrightnessWindow(bool docked);

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    //    void paintEvent( QPaintEvent *event ) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *event);
    void showEvent(QShowEvent *event) override;
    //    void contextMenuEvent(QContextMenuEvent *e) override;
    //    void mousePressEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void handlePageNoLongerNeeded();
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
    void handleFolderWindowClosed();
    void handleFolderWindowOpenVolume(QString path);
    void handleOpenVolumeWithProgressActionTriggered(bool checked);
    void handleShowReadProgressActionTriggered(bool checked);
    void handleSaveReadProgressActionTriggered(bool checked);
    void handleSaveFolderViewWidthActionTriggered(bool checked);

    // Catalog
    void handleShowCatalogActionTriggered();
    void handleCatalogWindowClosed();
    void handleCatalogWindowOpenVolume(QString path);
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
    void handleBrightnessWindowClosed();

    // Navigation
    void handleTurnPageOnLeftActionTriggered();
    void handleTurnPageOnRightActionTriggered();

    // Exif
    void handleOpenExifActionTriggered();
    void handleExifDialogClosed();

    // PageBar
    void handlePageManagerPageChanged();
    void handlePageManagerVolumeChanged(QString path);
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
    void handleShaderBilinearBeforeCpuBicubicActionTriggered();
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

protected:
    Ui::MainWindow *ui;
    bool m_viewerWindowStateMaximized;
    bool m_sliderChanging;
    bool m_onWindowClosing;
    bool m_revealInitialFullscreen;

    /**
     * @brief m_contextMenu Define on the context menu mainwindow.ui for the main screen and separate at startup
     */
    QMenu *m_contextMenu;

    QString m_volumeCaption;
    QString m_pageCaption;

    PageManager m_pageManager;
    ImageString m_imageString;
    QList<QAction *> m_shaderMenuGroup;
    QList<QAction *> m_languageMenuGroup;
    QList<QAction *> m_sortByMenuGroup;
    ThumbnailManager *m_thumbManager;
    FolderWindow *m_folderWindow;
    QString m_pendingFolderPath;
    CatalogWindow *m_catalogWindow;
    BrightnessWindow *m_brightnessWindow;
    ExifDialog *m_exifDialog;
    QToolButton *m_fullscreenButton;
    uint m_menubarFontSize;
    uint m_pageSliderHeight;
};

#endif // MAINWINDOW_H
