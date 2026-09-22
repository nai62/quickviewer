#ifndef CATALOGWINDOW_H
#define CATALOGWINDOW_H

#include <QtGui>
#include <QMenu>
#include <QMainWindow>
#include "catalogdatabase.h"
#include "models/volumelocation.h"
#include "volumeitemmodel.h"

namespace Ui {
class CatalogWindow;
class MainWindow;
}

class SearchWords
{
public:
    bool isEmpty;
    QStringList matches;
    QStringList nomatches;
    SearchWords(const QString &searchNoCase);
    bool match(const QString &targetNoCase);
};

class CatalogWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogWindow(QWidget *parent, Ui::MainWindow *uiMain);
    ~CatalogWindow();
    void setCatalogDatabase(CatalogDatabase *catalogDatabase);
    void resetViewMode();
    void setAsToplevelWindow();
    void setAsInnerWidget();
    void resetVolumes();
    void searchByWord(bool doForce = false);
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    void resizeEvent(QResizeEvent *event);
    bool isCatalogSearching();
    void initTagButtons();
    void resetTagButtons(QStringList buttons, QStringList checks);
    QStringList getTagWords();
    void handleShowTagBarActionTriggered(bool checked);

public slots:
    void handleFolderViewButtonClicked();
    void handleFolderViewListActionTriggered();
    void handleFolderViewIconActionTriggered();
    void handleFolderViewIconNoTextActionTriggered();
    void handleManageCatalogButtonClicked();
    void handleSearchEditTextChanged(QString text);
    void handleSearchLineEditEditingFinished();
    void handleVolumeListItemDoubleClicked(const QModelIndex &index);
    void handleVolumeListContextMenu(const QPoint &position);
    void handleSearchTitleWithOptionsActionTriggered(bool checked);
    void handleCatalogTitleWithoutOptionsActionTriggered(bool checked);
    void handleTagButtonClicked();

signals:
    void openVolume(const OpenTarget &target);
    void closed();

protected:
    void closeEvent(QCloseEvent *e);

private:
    /** Opens the tag dialog for the volume at \a row of the shown list. */
    void editVolumeTags(int row);

    int listableVolumeCount() const;

    Ui::CatalogWindow *ui;
    CatalogDatabase *m_catalogDatabase = nullptr;
    QList<VolumeThumbRecord> m_volumes;
    QList<VolumeThumbRecord *> m_volumeSearch;
    QMenu m_folderViewMenu;
    QString m_lastSearchWord;
    QStringList m_lastTagWords;
    VolumeItemModel m_itemModel;
};

#endif // CATALOGWINDOW_H
