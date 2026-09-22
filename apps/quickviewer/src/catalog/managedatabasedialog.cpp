#include <QMessageBox>

#include "managedatabasedialog.h"
#include "databasesettingdialog.h"
#include "fileloader.h"
#include "models/filemanager.h"
#include "ui_cataloglist.h"
#include "volumetagdialog.h"

ManageDatabaseDialog::ManageDatabaseDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ManageDatabaseDialog),
      m_catalogDatabase(nullptr),
      m_catalogWatcher(nullptr)
{
    ui->setupUi(this);
    ui->progressBar->setVisible(false);
    qRegisterMetaType<CatalogRecord>("CatalogRecord");

    // CatalogTree
    ui->treeWidget->sortByColumn(1, Qt::DescendingOrder);
    QTreeWidgetItem *header = ui->treeWidget->headerItem();
    header->setText(
        0, tr("Name", "Title of the column in the list part of the folder registered as Catalog"));
    header->setText(
        1,
        tr("Created", "Title of the column in the list part of the folder registered as Catalog"));
    header->setText(
        2, tr("Path", "Title of the column in the list part of the folder registered as Catalog"));
    header->setHidden(false);
    // The path is the column that tells the catalogs apart, so it is the one
    // that takes the room the other two leave, and the two keep the width their
    // own text needs.
    QHeaderView *headerView = ui->treeWidget->header();
    headerView->setSectionResizeMode(0, QHeaderView::Interactive);
    headerView->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->treeWidget->setColumnWidth(0, 150);

    // Buttons
    ui->updateAllButton->setVisible(false);
    ui->updateButton->setVisible(false);
    connect(ui->purgeMissingButton,
            &QPushButton::clicked,
            this,
            &ManageDatabaseDialog::handlePurgeMissingButtonClicked);
    connect(ui->treeWidget,
            &QTreeWidget::currentItemChanged,
            this,
            &ManageDatabaseDialog::handleCatalogSelectionChanged);
    connect(ui->openInExplorerButton,
            &QPushButton::clicked,
            this,
            &ManageDatabaseDialog::handleOpenInExplorerClicked);
    connect(ui->editTagsButton,
            &QPushButton::clicked,
            this,
            &ManageDatabaseDialog::handleEditTagsButtonClicked);
    connect(ui->booksTree,
            &QTreeWidget::itemDoubleClicked,
            this,
            &ManageDatabaseDialog::handleEditTagsButtonClicked);
    // The tag editor and the cover both follow the book the list has.
    connect(
        ui->booksTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current) {
            ui->editTagsButton->setEnabled(current != nullptr);
            updateCover();
        });
    ui->editTagsButton->setEnabled(false);
    // The cover changes size with the label, not only with the selection.
    ui->coverLabel->installEventFilter(this);

    resetCatalogList();
}

ManageDatabaseDialog::~ManageDatabaseDialog()
{
    delete ui;
}

void ManageDatabaseDialog::setCatalogDatabase(CatalogDatabase *catalogDatabase)
{
    m_catalogDatabase = catalogDatabase;
    if (!m_catalogDatabase->ensureReady()) {
        reportCatalogDatabaseProblem();
        return;
    }
    m_catalogs = m_catalogDatabase->catalogs();
    updatePurgeButton();
    resetCatalogList();
    normalButtonStates();
}

void ManageDatabaseDialog::updatePurgeButton()
{
    m_missingVolumes = m_catalogDatabase ? m_catalogDatabase->missingVolumePaths() : QStringList();
    ui->purgeMissingButton->setText(
        tr("Remove missing entries (%1)",
           "Button that removes the catalog entries whose folder is no longer there")
            .arg(m_missingVolumes.size()));
    ui->purgeMissingButton->setEnabled(!m_missingVolumes.isEmpty());
}

void ManageDatabaseDialog::handlePurgeMissingButtonClicked()
{
    if (!m_catalogDatabase || m_missingVolumes.isEmpty()) {
        return;
    }
    if (!confirmRemoval(
            tr("Remove missing entries"),
            tr("%1 registered folder(s) are no longer there. Remove them from the list? The "
               "image files are not deleted.")
                .arg(m_missingVolumes.size()))) {
        return;
    }
    const int removed = m_catalogDatabase->removeMissingVolumes();
    updatePurgeButton();
    resetCatalogList();
    normalButtonStates();
    if (removed > 0) {
        QMessageBox::information(
            this, tr("Remove missing entries"), tr("Removed %1 entry(ies).").arg(removed));
    }
}

void ManageDatabaseDialog::reportCatalogDatabaseProblem()
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(tr("Catalog database"));
    msgBox.setText(m_catalogDatabase->errorMessage());
    msgBox.setInformativeText(tr("Move or rename that file, then open the catalog again."));
    msgBox.exec();
}

void ManageDatabaseDialog::normalButtonStates()
{
    ui->addButton->setEnabled(true);
    updateExplorerButton();
    if (m_catalogs.isEmpty() && m_makeCatalogs.isEmpty()) {
        ui->editButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
        ui->updateButton->setEnabled(false);
        ui->deleteAllButton->setEnabled(false);
        ui->updateAllButton->setEnabled(false);
    } else {
        ui->editButton->setEnabled(true);
        ui->deleteButton->setEnabled(true);
        ui->updateButton->setEnabled(true);
        ui->deleteAllButton->setEnabled(true);
        ui->updateAllButton->setEnabled(true);
    }
    ui->buttonBox->setEnabled(true);

    if (m_makeCatalogs.size() == 0) {
        ui->cancelButton->setVisible(false);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    } else {
        ui->cancelButton->setVisible(true);
        ui->cancelButton->setText(tr(
            "Start creating", "Button that builds the folders which were added to the list above"));
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    ui->progressBar->setVisible(false);
    ui->volumeNameLabel->setVisible(false);
}

void ManageDatabaseDialog::progressButtonStates()
{
    ui->addButton->setEnabled(false);
    ui->editButton->setEnabled(false);
    ui->deleteButton->setEnabled(false);
    ui->updateButton->setEnabled(false);
    ui->deleteAllButton->setEnabled(false);
    ui->updateAllButton->setEnabled(false);
    ui->purgeMissingButton->setEnabled(false);
    ui->openInExplorerButton->setEnabled(false);
    ui->buttonBox->setEnabled(false);

    ui->progressBar->setVisible(true);
    ui->cancelButton->setText(tr("Stop creating", "Button that cancels the catalogs being built"));
    ui->cancelButton->setVisible(true);
    ui->volumeNameLabel->setVisible(true);
}

void ManageDatabaseDialog::resetCatalogList()
{
    ui->treeWidget->clear();
    // Existing catalogs
    for (int id : m_catalogs.keys()) {
        const CatalogRecord &catalog = m_catalogs[id];
        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, catalog.name);
        item->setText(1, catalog.created_at.toString(QStringLiteral("yyyy/MM/dd hh:mm:ss")));
        item->setText(2, catalog.path);
        // The column is as wide as the room the list has, so the whole path has
        // to be readable another way too.
        item->setToolTip(2, catalog.path);
        item->setData(0, Qt::UserRole, QVariant(catalog.id));
        ui->treeWidget->addTopLevelItem(item);
    }
    // Making catalogs
    {
        int cnt = -100;
        for (const CatalogRecord &catalog : m_makeCatalogs) {
            QTreeWidgetItem *item = new QTreeWidgetItem;
            item->setText(0, "* " + catalog.name);
            item->setText(1,
                          tr("Not created yet",
                             "Representation of time indicating that the catalog is not currently "
                             "created and will be generated from now"));
            item->setText(2, catalog.path);
            item->setToolTip(2, catalog.path);
            item->setData(0, Qt::UserRole, cnt--);
            item->setBackground(0, QBrush(QColor("lightgreen")));
            QFont font = item->font(0);
            font.setItalic(true);
            item->setFont(0, font);
            ui->treeWidget->addTopLevelItem(item);
        }
    }

    handleCatalogSelectionChanged();
}

void ManageDatabaseDialog::handleCatalogSelectionChanged()
{
    ui->booksTree->clear();
    m_bookCovers.clear();
    ui->editTagsButton->setEnabled(false);
    updateExplorerButton();
    // The cover that was shown belonged to the catalog that was selected.
    updateCover();
    if (!m_catalogDatabase) {
        return;
    }
    const QTreeWidgetItem *current = ui->treeWidget->currentItem();
    if (!current) {
        return;
    }
    // A catalog that is only waiting to be built holds no books yet.
    const int catalogId = current->data(0, Qt::UserRole).toInt();
    if (catalogId < 0) {
        return;
    }

    const QList<QPair<VolumeThumbRecord, QStringList>> volumes =
        m_catalogDatabase->catalogVolumes(catalogId);
    for (const QPair<VolumeThumbRecord, QStringList> &entry : volumes) {
        const VolumeThumbRecord &volume = entry.first;
        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, volume.name.isEmpty() ? volume.realname : volume.name);
        item->setToolTip(0, volume.realname);
        item->setText(1, entry.second.join(QStringLiteral(", ")));
        item->setData(0, Qt::UserRole, volume.id);
        ui->booksTree->addTopLevelItem(item);
        m_bookCovers.insert(volume.id, volume.thumbnail);
    }
    if (ui->booksTree->topLevelItemCount() > 0) {
        // Start on the first book so that the editor is one press away.
        ui->booksTree->setCurrentItem(ui->booksTree->topLevelItem(0));
    }
    updateCover();
}

void ManageDatabaseDialog::updateExplorerButton()
{
    // There is a folder to show as soon as the list has a catalog to show it
    // for, whether or not it has been built yet.
    const QTreeWidgetItem *selected = ui->treeWidget->currentItem();
    ui->openInExplorerButton->setEnabled(selected != nullptr && !selected->text(2).isEmpty());
}

void ManageDatabaseDialog::updateCover()
{
    const QTreeWidgetItem *current = ui->booksTree->currentItem();
    const auto stored = current ? m_bookCovers.constFind(current->data(0, Qt::UserRole).toInt())
                                : m_bookCovers.constEnd();
    m_cover = stored == m_bookCovers.constEnd()
                  ? QImage()
                  : QImage::fromData(*stored, IFileLoader::jpegQtFormatName());
    applyCover();
}

void ManageDatabaseDialog::applyCover()
{
    QLabel *label = ui->coverLabel;
    if (m_cover.isNull()) {
        // A book the catalog stored without a cover, or no book at all.
        label->setPixmap(QPixmap());
        label->setText(tr("No cover", "Text shown where a cover would be"));
        return;
    }
    label->setText(QString());
    label->setPixmap(QPixmap::fromImage(
        m_cover.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

bool ManageDatabaseDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->coverLabel && event->type() == QEvent::Resize) {
        applyCover();
    }
    return QDialog::eventFilter(watched, event);
}

void ManageDatabaseDialog::handleOpenInExplorerClicked()
{
    const QTreeWidgetItem *current = ui->treeWidget->currentItem();
    if (!current) {
        return;
    }
    // A catalog waiting to be built has no record yet; both kinds show the
    // folder they were added with in the path column.
    const int id = current->data(0, Qt::UserRole).toInt();
    const QString path = id > 0 && m_catalogs.contains(id) ? m_catalogs[id].path : current->text(2);
    if (!path.isEmpty()) {
        showInFileManager(path);
    }
}

void ManageDatabaseDialog::handleEditTagsButtonClicked()
{
    if (!m_catalogDatabase) {
        return;
    }
    QTreeWidgetItem *current = ui->booksTree->currentItem();
    if (!current && ui->booksTree->topLevelItemCount() > 0) {
        current = ui->booksTree->topLevelItem(0);
    }
    if (!current) {
        return;
    }
    const int volumeId = current->data(0, Qt::UserRole).toInt();

    QStringList knownTags;
    const QMap<int, TagRecord *> byCount = m_catalogDatabase->tagsByCount();
    for (TagRecord *tag : byCount) {
        knownTags << tag->name;
    }
    QStringList volumeTags;
    for (const TagRecord &tag : m_catalogDatabase->getTagsFromVolumeId(volumeId)) {
        volumeTags << tag.name;
    }

    VolumeTagDialog dialog(this);
    dialog.setVolume(current->text(0), current->toolTip(0), knownTags, volumeTags);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_catalogDatabase->setVolumeDisplayName(volumeId, dialog.displayName());
    m_catalogDatabase->setVolumeTags(volumeId, dialog.tags());

    // Show the title and the tags the book carries now.
    handleCatalogSelectionChanged();
}

void ManageDatabaseDialog::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasFormat("text/uri-list")) {
        e->acceptProposedAction();
    }
}

void ManageDatabaseDialog::handleAddButtonClicked()
{
    CatalogRecord catalog = {0};
    if (!databaseSettingDialog(catalog, false)) {
        return;
    }
    m_makeCatalogs << catalog;

    resetCatalogList();
    normalButtonStates();
}

void ManageDatabaseDialog::dropEvent(QDropEvent *e)
{
    if (!e->mimeData()->hasUrls()) {
        return;
    }
    QList<QUrl> urlList = e->mimeData()->urls();
    for (int i = 0; i < urlList.size(); i++) {
        QUrl url = urlList[i];
        QFileInfo info(url.toLocalFile());
        CatalogRecord catalog = {0};
        if (info.isDir()) {
            catalog.name = info.fileName();
            catalog.path = QDir::toNativeSeparators(info.absoluteFilePath());
        } else if (info.isFile()) {
            // A dropped book (archive) becomes a catalog of its own; any other
            // file stands for the folder that holds it.
            const bool book = IFileLoader::isArchiveFile(info.fileName());
            catalog.name = info.baseName();
            catalog.path = QDir::toNativeSeparators(book ? info.absoluteFilePath() : info.path());
        }
        m_makeCatalogs << catalog;
    }

    resetCatalogList();
    normalButtonStates();
}

bool ManageDatabaseDialog::databaseSettingDialog(CatalogRecord &catalog, bool editing)
{
    DatabaseSettingDialog dialog(this);
    dialog.setName(catalog.name);
    dialog.setPath(catalog.path);
    dialog.setForEditing(editing);
    if (editing) {
        dialog.setWindowTitle(
            tr("Edit Catalog", "Button for editing contents of already created catalog"));
    }

    int result = dialog.exec();
    if (result == QDialog::Rejected) {
        return false;
    }
    catalog.name = dialog.name();
    catalog.path = dialog.path();
    return true;
}

void ManageDatabaseDialog::handleCatalogCreated(const CatalogRecord cr)
{
    if (!cr.created) {
        return;
    }
    m_catalogs[cr.id] = cr;
    int i = 0;
    for (const CatalogRecord &c : m_makeCatalogs) {
        if (cr.path == c.path) {
            break;
        }
        i++;
    }
    if (i < m_makeCatalogs.size()) {
        m_makeCatalogs.removeAt(i);
    }

    resetCatalogList();
}

void ManageDatabaseDialog::handleCatalogCreationFinished()
{
    if (!m_catalogWatcher) {
        return;
    }
    releaseCatalogWatcher();

    resetCatalogList();
    normalButtonStates();

    QMessageBox msgBox(this);
    if (m_makeCatalogs.isEmpty()) {
        msgBox.setWindowTitle(
            tr("Completed", "Title of message box when catalog generation finished successfully"));
        msgBox.setText(tr("Catalog creation completed.",
                          "Body of message box when catalog generation finished successfully"));
    } else {
        // The build ended without storing every catalog it was asked for.
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("Catalog creation incomplete"));
        msgBox.setText(tr("Catalog(s) left unstored: %1",
                          "Body of message box when some catalogs could not be stored")
                           .arg(m_makeCatalogs.size()));
        msgBox.setInformativeText(m_catalogDatabase->errorMessage());
    }
    msgBox.exec();
}

void ManageDatabaseDialog::releaseCatalogWatcher()
{
    if (!m_catalogWatcher) {
        return;
    }
    disconnect(m_catalogDatabase,
               &CatalogDatabase::catalogCreated,
               this,
               &ManageDatabaseDialog::handleCatalogCreated);
    disconnect(m_catalogWatcher,
               &QFutureWatcher<QList<CatalogRecord>>::finished,
               this,
               &ManageDatabaseDialog::handleCatalogCreationFinished);
    disconnect(m_catalogDatabase,
               &CatalogDatabase::catalogProgressRangeChanged,
               ui->progressBar,
               &QProgressBar::setRange);
    disconnect(m_catalogDatabase,
               &CatalogDatabase::catalogProgressValueChanged,
               ui->progressBar,
               &QProgressBar::setValue);
    disconnect(m_catalogDatabase,
               &CatalogDatabase::catalogProgressTextChanged,
               ui->volumeNameLabel,
               &QLabel::setText);

    m_catalogWatcher = nullptr;
}

void ManageDatabaseDialog::handleCancelButtonClicked()
{
    if (!m_catalogDatabase) {
        return;
    }
    if (!m_catalogWatcher) {
        if (!m_catalogDatabase->ensureReady()) {
            reportCatalogDatabaseProblem();
            return;
        }
        connect(m_catalogDatabase,
                &CatalogDatabase::catalogCreated,
                this,
                &ManageDatabaseDialog::handleCatalogCreated);
        m_catalogWatcher = m_catalogDatabase->createCatalogAsync(m_makeCatalogs);
        connect(m_catalogWatcher,
                &QFutureWatcher<QList<CatalogRecord>>::finished,
                this,
                &ManageDatabaseDialog::handleCatalogCreationFinished);
        connect(m_catalogDatabase,
                &CatalogDatabase::catalogProgressRangeChanged,
                ui->progressBar,
                &QProgressBar::setRange);
        connect(m_catalogDatabase,
                &CatalogDatabase::catalogProgressValueChanged,
                ui->progressBar,
                &QProgressBar::setValue);
        connect(m_catalogDatabase,
                &CatalogDatabase::catalogProgressTextChanged,
                ui->volumeNameLabel,
                &QLabel::setText);

        progressButtonStates();
    } else {
        releaseCatalogWatcher();
        m_catalogDatabase->cancelCreateCatalogAsync();

        resetCatalogList();
        normalButtonStates();

        QMessageBox msgBox(this);
        msgBox.setWindowTitle(
            tr("Cancelled!", "Title of message box when catalog generation was canceled"));
        QString message = QString(tr("Catalog creation was cancelled.",
                                     "Body of message box when catalog generation is canceled"));
        msgBox.setText(message);
        msgBox.exec();
    }
}

void ManageDatabaseDialog::closeEvent(QCloseEvent *)
{
    if (!m_catalogDatabase) {
        return;
    }
    stopBuilding();
    m_catalogDatabase->vacuum();
}

void ManageDatabaseDialog::stopBuilding()
{
    if (m_catalogWatcher) {
        // A running build keeps writing to the database, which VACUUM cannot
        // work on, so wait for it before reclaiming space.
        QFutureWatcher<QList<CatalogRecord>> *watcher = m_catalogWatcher;
        releaseCatalogWatcher();
        m_catalogDatabase->cancelCreateCatalogAsync();
        watcher->waitForFinished();
    }
}

void ManageDatabaseDialog::reject()
{
    if (!m_catalogDatabase || m_catalogWatcher) {
        // A running build is stopped the way closing the window stops it.
        stopBuilding();
        QDialog::reject();
        return;
    }
    if (!m_makeCatalogs.isEmpty()) {
        const QMessageBox::StandardButton answer =
            QMessageBox::question(this,
                                  tr("Close"),
                                  tr("%1 added folder(s) are not created yet. Close and discard "
                                     "them?")
                                      .arg(m_makeCatalogs.size()),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    QDialog::reject();
}

bool ManageDatabaseDialog::confirmRemoval(const QString &title, const QString &text)
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void ManageDatabaseDialog::handleEditButtonClicked()
{
    if (!m_catalogDatabase) {
        return;
    }
    QTreeWidgetItem *current = ui->treeWidget->currentItem();
    if (!current) {
        return;
    }

    int id = current->data(0, Qt::UserRole).toInt();
    CatalogRecord catalog;
    if (id >= 0) {
        catalog = m_catalogs[id];
        if (!databaseSettingDialog(catalog, true)) {
            return;
        }
        m_catalogDatabase->updateCatalogName(id, catalog.name);
        m_catalogs[id] = catalog;
    } else {
        // A catalog that is only waiting to be built stays in the request.
        const int pendingIndex = -100 - id;
        catalog = m_makeCatalogs[pendingIndex];
        if (!databaseSettingDialog(catalog, false)) {
            return;
        }
        m_makeCatalogs[pendingIndex] = catalog;
    }

    resetCatalogList();
}

void ManageDatabaseDialog::handleDeleteButtonClicked()
{
    if (!m_catalogDatabase) {
        return;
    }
    QTreeWidgetItem *current = ui->treeWidget->currentItem();
    if (!current) {
        return;
    }
    int id = current->data(0, Qt::UserRole).toInt();
    if (id >= 0) {
        if (!confirmRemoval(
                tr("Delete catalog"),
                tr("Delete \"%1\" from the list of catalogs? The image files are not deleted.")
                    .arg(m_catalogs[id].name))) {
            return;
        }
        m_catalogDatabase->deleteCatalog(id);
        m_catalogs.remove(id);
    } else {
        id = -100 - id;
        if (id < m_makeCatalogs.size()) {
            m_makeCatalogs.removeAt(id);
        }
    }

    resetCatalogList();
    normalButtonStates();
}

void ManageDatabaseDialog::handleUpdateButtonClicked() {}

void ManageDatabaseDialog::handleDeleteAllButtonClicked()
{
    if (!m_catalogDatabase) {
        return;
    }
    if (!confirmRemoval(
            tr("Delete all catalogs"),
            tr("Delete every catalog from the list? The image files are not deleted."))) {
        return;
    }
    m_catalogDatabase->deleteAllCatalogs();
    m_catalogs.clear();
    m_makeCatalogs.clear();

    resetCatalogList();
    normalButtonStates();
}

void ManageDatabaseDialog::handleUpdateAllButtonClicked() {}
