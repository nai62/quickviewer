#include "databasesettingdialog.h"
#include "fileloader.h"
#include "ui_createdb.h"
#include <QButtonGroup>
#include <QFileDialog>

DatabaseSettingDialog::DatabaseSettingDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::DatabaseSettingDialog)

{
    ui->setupUi(this);
    setAcceptDrops(true);
}

DatabaseSettingDialog::~DatabaseSettingDialog()
{
    delete ui;
}

int DatabaseSettingDialog::exec()
{
    if (!m_name.isEmpty()) {
        ui->nameEdit->setText(m_name);
    }
    if (!m_path.isEmpty()) {
        ui->pathEdit->setText(m_path);
    }
    if (m_editing) {
        ui->pathEdit->setEnabled(false);
        ui->selectFolderButton->setEnabled(false);
        setAcceptDrops(false);
    }
    checkAcceptable();
    return QDialog::exec();
}

void DatabaseSettingDialog::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasFormat("text/uri-list")) {
        e->acceptProposedAction();
    }
}
void DatabaseSettingDialog::dropEvent(QDropEvent *e)
{
    if (m_editing || !e->mimeData()->hasUrls()) {
        return;
    }
    QList<QUrl> urlList = e->mimeData()->urls();
    for (int i = 0; i < qMin(1, int(urlList.size())); i++) {
        QUrl url = urlList[i];
        if (!url.isLocalFile()) {
            continue;
        }
        QFileInfo info(url.toLocalFile());
        if (info.isDir()) {
            ui->pathEdit->setText(QDir::toNativeSeparators(info.absoluteFilePath()));
            if (ui->nameEdit->text().isEmpty()) {
                ui->nameEdit->setText(info.fileName());
            }
        } else if (info.isFile()) {
            // A dropped book (archive) is registered as itself; any other file
            // stands for the folder that holds it.
            const bool book = IFileLoader::isArchiveFile(info.fileName());
            ui->pathEdit->setText(
                QDir::toNativeSeparators(book ? info.absoluteFilePath() : info.path()));
            if (ui->nameEdit->text().isEmpty()) {
                ui->nameEdit->setText(info.baseName());
            }
        }
    }
}

void DatabaseSettingDialog::checkAcceptable()
{
    bool enabled = false;
    do {
        if (m_name.trimmed().isEmpty()) {
            break;
        }
        const QFileInfo source(m_path);
        if (m_path.trimmed().isEmpty() ||
            (!m_editing &&
             !(source.isDir() || (source.isFile() && IFileLoader::isArchiveFile(m_path))))) {
            break;
        }
        enabled = true;
    } while (false);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

void DatabaseSettingDialog::handleNameLineEditTextChanged(QString name)
{
    setName(name);
    checkAcceptable();
}

void DatabaseSettingDialog::handleSelectFolderButtonClicked()
{
    QString folder =
        QFileDialog::getExistingDirectory(this,
                                          tr("Select a folder containing images or archives",
                                             "Caption of FolderSelectDialog urging selection of "
                                             "folders containing Images and Archives"));
    if (!folder.isEmpty()) {
        ui->pathEdit->setText(folder);
    }
}
