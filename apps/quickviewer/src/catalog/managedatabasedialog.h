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
    void closeEvent(QCloseEvent *e);
    void reject() override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void handleAddButtonClicked();
    void handleDeleteButtonClicked();
    void handleEditButtonClicked();
    void handleUpdateButtonClicked();
    void handleDeleteAllButtonClicked();
    void handlePurgeMissingButtonClicked();
    void handleCatalogSelectionChanged();
    void handleEditTagsButtonClicked();
    void handleUpdateAllButtonClicked();
    void handleCancelButtonClicked();
    void handleOpenInExplorerClicked();

private slots:
    void handleCatalogCreated(const CatalogRecord cr);
    void handleCatalogCreationFinished();

private:
    /** Enables the explorer button for the catalog the list has selected. */
    void updateExplorerButton();
    /** Shows the cover of the book the list has selected, or a placeholder. */
    void updateCover();
    /** Draws the cover kept by updateCover() into the room the label has. */
    void applyCover();
    void releaseCatalogWatcher();
    void reportCatalogDatabaseProblem();
    void stopBuilding();
    bool confirmRemoval(const QString &title, const QString &text);
    void updatePurgeButton();

    Ui::ManageDatabaseDialog *ui;
    QMap<int, CatalogRecord> m_catalogs;
    QList<CatalogRecord> m_makeCatalogs;
    QStringList m_missingVolumes;
    CatalogDatabase *m_catalogDatabase;
    /** The stored cover of each book of the selected catalog, by volume id. */
    QMap<int, QByteArray> m_bookCovers;
    QImage m_cover;

    QFutureWatcher<QList<CatalogRecord>> *m_catalogWatcher;
};

#endif // MANAGEDATABASEDIALOG_H
