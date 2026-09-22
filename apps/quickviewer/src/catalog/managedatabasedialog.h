#ifndef MANAGEDATABASEDIALOG_H
#define MANAGEDATABASEDIALOG_H

#include <QtGui>
#include <QDialog>
#include "catalogbuildqueue.h"
#include "catalogdatabase.h"

namespace Ui {
class ManageDatabaseDialog;
}

class ManageDatabaseDialog : public QDialog
{
    Q_OBJECT
public:
    ManageDatabaseDialog(QWidget *parent = nullptr);
    ~ManageDatabaseDialog();
    void normalButtonStates();
    void progressButtonStates();
    void setCatalogDatabase(CatalogDatabase *catalogDatabase);
    void resetCatalogList();
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    bool databaseSettingDialog(CatalogRecord &catalog, bool editing);

protected:
    void reject() override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void handleAddButtonClicked();
    void handleEditActionTriggered();
    void handleDeleteActionTriggered();
    void handleDeleteAllActionTriggered();
    void handlePurgeMissingActionTriggered();
    void handleCatalogSelectionChanged();
    void handleEditTagsButtonClicked();
    void handleCancelButtonClicked();
    void handleOpenInExplorerActionTriggered();
    void handleCatalogContextMenu(const QPoint &position);

private slots:
    void handleCatalogStored(const CatalogRecord catalog);
    void handleBuildFinished(bool canceled);
    void handleBuildStateChanged();
    void handleMissingVolumesChecked();

private:
    /** Writes the button that starts and stops a build from the queue. */
    void updateStartButton();
    /** Asks the worker which of the registered paths are no longer there. */
    void startMissingVolumeCheck();
    /** Writes the purge action from the paths the last check reported. */
    void updatePurgeAction();
    /** Enables actions only for a selected catalog while no build is running. */
    void updateCatalogActions();
    void selectPendingCatalog(int requestId);
    /** Shows the cover of the book the list has selected, or a placeholder. */
    void updateCover();
    void reportCatalogDatabaseProblem();
    bool confirmRemoval(const QString &title, const QString &text);

    Ui::ManageDatabaseDialog *ui;
    QMap<int, CatalogRecord> m_catalogs;
    QStringList m_missingVolumes;
    CatalogDatabase *m_catalogDatabase;
    CatalogBuildQueue *m_buildQueue;
    QFutureWatcher<QStringList> m_missingWatcher;
};

#endif // MANAGEDATABASEDIALOG_H
