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

public slots:
    void handleAddButtonClicked();
    void handleDeleteButtonClicked();
    void handleEditButtonClicked();
    void handleUpdateButtonClicked();
    void handleDeleteAllButtonClicked();
    void handlePurgeMissingButtonClicked();
    void handleUpdateAllButtonClicked();
    void handleCancelButtonClicked();

private slots:
    void handleCatalogCreated(const CatalogRecord cr);
    void handleCatalogCreationFinished();

private:
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

    QFutureWatcher<QList<CatalogRecord>> *m_catalogWatcher;
};

#endif // MANAGEDATABASEDIALOG_H
