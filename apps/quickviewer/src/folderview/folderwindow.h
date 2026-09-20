#ifndef FOLDERWINDOW_H
#define FOLDERWINDOW_H

#include <QtGui>
#include <QtWidgets>
#include "qvenums.h"
#include "folderitemmodel.h"
#include "folderitemdelegate.h"
#include "models/volumelocation.h"

namespace Ui {
class FolderWindow;
class MainWindow;
}

class FolderWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FolderWindow(QWidget *parent, Ui::MainWindow *uiMain);
    ~FolderWindow();
    bool eventFilter(QObject *obj, QEvent *event);
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    void resizeEvent(QResizeEvent *event);
    void setFolderPath(QString path, bool showParent = true);
    void reset();
    void resortVolumes();
    void resetPathLabel(int maxWidth);
    QString currentPath() { return m_currentPath; }
    QString itemPath(const QModelIndex &index) const;
    void keyPressEvent(QKeyEvent *event);
    void handleCurrentFolderItemTriggered();

public slots:
    void handleHomeButtonClicked();
    void handleParentButtonClicked();
    void handleReloadButtonClicked();
    void handleViewerSessionVolumeChanged(QString);
    void handleFolderViewItemSelected(const QModelIndex &index);
    void handleSetAsHomeFolderActionTriggered();

signals:
    void openVolume(const OpenTarget &target);
    void reloadRequested(const QString &containerPath);
    void closed();

protected:
    void closeEvent(QCloseEvent *e);

private:
    void openFolderItem(const QModelIndex &index);
    void sortVolumes();
    void setupHistoryButton(Ui::MainWindow *uiMain);
    void warmUpListFonts();
    int currentVolumeRow() const;
    void updateCurrentVolumeRow();

    Ui::FolderWindow *ui;
    QMenu *m_itemContextMenu;
    QToolButton *m_historyButton;
    bool m_fontWarmUpStarted = false;
    QString m_currentPath;
    QString m_currentVolumePath;
    QList<FolderItem> m_volumes;
    FolderItemModel m_itemModel;
    FolderItemDelegate m_itemDelegate;
};

#endif // FOLDERWINDOW_H
