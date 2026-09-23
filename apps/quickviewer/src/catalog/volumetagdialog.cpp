#include "volumetagdialog.h"

#include "ui_volumetagdialog.h"
#include <QKeyEvent>

VolumeTagDialog::VolumeTagDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::VolumeTagDialog)
{
    ui->setupUi(this);
    connect(
        ui->addTagButton, &QPushButton::clicked, this, &VolumeTagDialog::handleAddTagButtonClicked);
    // Consume Enter here so it cannot also activate the dialog's default OK.
    ui->newTagEdit->installEventFilter(this);
}

VolumeTagDialog::~VolumeTagDialog()
{
    delete ui;
}

bool VolumeTagDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->newTagEdit && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            handleAddTagButtonClicked();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void VolumeTagDialog::setVolume(const QString &title,
                                const QString &realname,
                                const QStringList &knownTagNames,
                                const QStringList &volumeTagNames)
{
    ui->newTagEdit->clear();
    ui->nameEdit->setText(title);
    ui->nameEdit->setPlaceholderText(realname);

    QStringList names = knownTagNames;
    for (const QString &name : volumeTagNames) {
        if (!names.contains(name, Qt::CaseInsensitive)) {
            names << name;
        }
    }
    names.sort(Qt::CaseInsensitive);

    ui->tagList->clear();
    for (const QString &name : names) {
        QListWidgetItem *item = new QListWidgetItem(name, ui->tagList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        const bool checked = volumeTagNames.contains(name, Qt::CaseInsensitive);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

QString VolumeTagDialog::displayName() const
{
    return ui->nameEdit->text().trimmed();
}

QStringList VolumeTagDialog::tags() const
{
    QStringList tags;
    for (int row = 0; row < ui->tagList->count(); ++row) {
        const QListWidgetItem *item = ui->tagList->item(row);
        if (item->checkState() == Qt::Checked) {
            tags << item->text();
        }
    }
    // A tag typed but not added yet counts as well.
    const QString typed = ui->newTagEdit->text().trimmed();
    if (!typed.isEmpty() && !tags.contains(typed, Qt::CaseInsensitive)) {
        tags << typed;
    }
    return tags;
}

void VolumeTagDialog::handleAddTagButtonClicked()
{
    const QString name = ui->newTagEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }
    for (int row = 0; row < ui->tagList->count(); ++row) {
        QListWidgetItem *item = ui->tagList->item(row);
        if (QString::compare(item->text(), name, Qt::CaseInsensitive) == 0) {
            // The catalog knows the tag already: tick it instead of adding it twice.
            item->setCheckState(Qt::Checked);
            ui->tagList->scrollToItem(item);
            ui->newTagEdit->clear();
            return;
        }
    }
    QListWidgetItem *item = new QListWidgetItem(name, ui->tagList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
    ui->tagList->scrollToItem(item);
    ui->newTagEdit->clear();
}
