#ifndef MANAGEDATABASEDIALOG_H
#define MANAGEDATABASEDIALOG_H

#include <QtGui>
#include <QDialog>
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
    void handleDeleteButtonClicked();
    void handleEditButtonClicked();
    void handleDeleteAllButtonClicked();
    void handlePurgeMissingActionTriggered();
    void handleCatalogSelectionChanged();
    void handleEditTagsButtonClicked();
    void handleCancelButtonClicked();
    void handleOpenInExplorerClicked();
    void handleCatalogContextMenu(const QPoint &position);

private slots:
    void handleCatalogCreated(const CatalogRecord cr);
    void handleCatalogCreationFinished();
    void handleMissingVolumesChecked();

private:
    /** Asks the worker which of the registered paths are no longer there. */
    void startMissingVolumeCheck();
    /** Writes the purge action from the paths the last check reported. */
    void updatePurgeAction();
    /** Enables actions only for a selected catalog while no build is running. */
    void updateCatalogActions();
    void selectPendingCatalog(int index);
    /** Shows the cover of the book the list has selected, or a placeholder. */
    void updateCover();
    /** Draws the cover kept by updateCover() into the room the label has. */
    void applyCover();
    void releaseCatalogWatcher();
    void reportCatalogDatabaseProblem();
    void stopBuilding();
    bool confirmRemoval(const QString &title, const QString &text);

    Ui::ManageDatabaseDialog *ui;
    QMap<int, CatalogRecord> m_catalogs;
    QList<CatalogRecord> m_makeCatalogs;
    QStringList m_missingVolumes;
    CatalogDatabase *m_catalogDatabase;
    QImage m_cover;
    QFutureWatcher<QStringList> m_missingWatcher;

    QFutureWatcher<QList<CatalogRecord>> *m_catalogWatcher;
};

#endif // MANAGEDATABASEDIALOG_H
