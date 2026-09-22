#include <QMessageBox>
#include <QMenu>
#include <QSignalBlocker>
#include <QtConcurrent>

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
      m_buildQueue(new CatalogBuildQueue(this))
{
    ui->setupUi(this);
    ui->progressWidget->setVisible(false);
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

    // Daily actions stay beside the list. Maintenance lives in the overflow menu.
    auto *maintenance = new QMenu(ui->moreButton);
    maintenance->addAction(ui->purgeMissingAction);
    maintenance->addSeparator();
    maintenance->addAction(ui->deleteAllAction);
    ui->moreButton->setMenu(maintenance);
    ui->buttonBox->button(QDialogButtonBox::Close)->setAutoDefault(false);
    ui->buttonBox->button(QDialogButtonBox::Close)
        ->setText(tr("Close", "Button that closes the catalog manager"));
    ui->dialogLayout->setStretchFactor(ui->catalogSplitter, 1);
    ui->catalogSplitter->setStretchFactor(0, 1);
    ui->catalogSplitter->setStretchFactor(1, 1);
    ui->catalogSplitter->setSizes({width() / 2, width() / 2});
    ui->booksTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->booksTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeWidget->installEventFilter(this);
    connect(ui->treeWidget,
            &QWidget::customContextMenuRequested,
            this,
            &ManageDatabaseDialog::handleCatalogContextMenu);
    ui->editAction->setShortcut(QKeySequence(Qt::Key_F2));
    ui->deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    for (QAction *action : {ui->editAction, ui->deleteAction, ui->openInExplorerAction}) {
        action->setShortcutContext(Qt::WidgetShortcut);
        ui->treeWidget->addAction(action);
    }
    connect(ui->purgeMissingAction,
            &QAction::triggered,
            this,
            &ManageDatabaseDialog::handlePurgeMissingActionTriggered);
    // Telling a slow path from a gone one belongs on a worker: a catalog can
    // hold thousands of volumes on slow drives.
    connect(&m_missingWatcher,
            &QFutureWatcher<QStringList>::finished,
            this,
            &ManageDatabaseDialog::handleMissingVolumesChecked);
    connect(ui->treeWidget,
            &QTreeWidget::currentItemChanged,
            this,
            &ManageDatabaseDialog::handleCatalogSelectionChanged);
    connect(ui->openInExplorerAction,
            &QAction::triggered,
            this,
            &ManageDatabaseDialog::handleOpenInExplorerActionTriggered);
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

    // The queue owns the folders waiting to be built and the build itself; the
    // dialog only shows what it reports.
    connect(
        m_buildQueue, &CatalogBuildQueue::changed, this, &ManageDatabaseDialog::resetCatalogList);
    connect(m_buildQueue,
            &CatalogBuildQueue::changed,
            this,
            &ManageDatabaseDialog::handleBuildStateChanged);
    connect(m_buildQueue,
            &CatalogBuildQueue::catalogStored,
            this,
            &ManageDatabaseDialog::handleCatalogStored);
    connect(m_buildQueue,
            &CatalogBuildQueue::buildFinished,
            this,
            &ManageDatabaseDialog::handleBuildFinished);

    resetCatalogList();
    normalButtonStates();
}

ManageDatabaseDialog::~ManageDatabaseDialog()
{
    // A build that is still running is asked to stop, and the worker rolls it
    // back on its own thread: the catalog that opened this dialog reads the
    // result when the database says the build is over. Leave the connections
    // first, so nothing reaches the widgets on the way out.
    disconnect(m_buildQueue, nullptr, this, nullptr);
    m_buildQueue->stop();
    delete ui;
}

void ManageDatabaseDialog::setCatalogDatabase(CatalogDatabase *catalogDatabase)
{
    m_catalogDatabase = catalogDatabase;
    m_buildQueue->setCatalogDatabase(catalogDatabase);
    if (!m_catalogDatabase->ensureReady()) {
        reportCatalogDatabaseProblem();
        return;
    }
    // A build reports its progress through the database while it runs.
    connect(m_catalogDatabase,
            &CatalogDatabase::catalogProgressRangeChanged,
            ui->progressBar,
            &QProgressBar::setRange,
            Qt::UniqueConnection);
    connect(m_catalogDatabase,
            &CatalogDatabase::catalogProgressValueChanged,
            ui->progressBar,
            &QProgressBar::setValue,
            Qt::UniqueConnection);
    connect(m_catalogDatabase,
            &CatalogDatabase::catalogProgressTextChanged,
            ui->volumeNameLabel,
            &QLabel::setText,
            Qt::UniqueConnection);
    m_catalogs = m_catalogDatabase->catalogs();
    resetCatalogList();
    normalButtonStates();
}

void ManageDatabaseDialog::startMissingVolumeCheck()
{
    if (!m_catalogDatabase || m_missingWatcher.isRunning()) {
        return;
    }
    // Nothing is known until the check answers: offer nothing to remove rather
    // than the count of the state before.
    m_missingVolumes.clear();
    updatePurgeAction();
    const QStringList paths = m_catalogDatabase->volumePaths();
    m_missingWatcher.setFuture(QtConcurrent::run([paths] {
        QStringList missing;
        for (const QString &path : paths) {
            if (!QFileInfo::exists(path) && !missing.contains(path)) {
                missing << path;
            }
        }
        return missing;
    }));
}

void ManageDatabaseDialog::handleMissingVolumesChecked()
{
    m_missingVolumes = m_missingWatcher.result();
    updatePurgeAction();
}

void ManageDatabaseDialog::updatePurgeAction()
{
    ui->purgeMissingAction->setText(
        m_missingVolumes.isEmpty()
            ? tr("Remove missing entries",
                 "Button that removes the catalog entries whose folder is no longer there")
            : tr("Remove missing entries (%1)",
                 "Button that removes the catalog entries whose folder is no longer there")
                  .arg(m_missingVolumes.size()));
    ui->purgeMissingAction->setEnabled(!m_missingVolumes.isEmpty());
}

void ManageDatabaseDialog::handlePurgeMissingActionTriggered()
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
    const int removed = m_catalogDatabase->removeVolumes(m_missingVolumes);
    startMissingVolumeCheck();
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
    setAcceptDrops(true);
    ui->treeWidget->setEnabled(true);
    ui->booksTree->setEnabled(true);
    ui->editTagsButton->setEnabled(ui->booksTree->currentItem() != nullptr);
    ui->addButton->setEnabled(true);
    ui->moreButton->setEnabled(true);
    ui->buttonBox->setEnabled(true);
    startMissingVolumeCheck();
    updateCatalogActions();
    updateStartButton();
    ui->progressWidget->setVisible(false);
}

void ManageDatabaseDialog::updateStartButton()
{
    const int pending = int(m_buildQueue->requests().size());
    if (m_buildQueue->isStopping()) {
        // The worker is rolling back; starting again must not reset its
        // cancellation flag, so the button waits for it.
        ui->cancelButton->setEnabled(false);
        ui->cancelButton->setText(tr("Stopping..."));
        return;
    }
    if (m_buildQueue->isBuilding()) {
        ui->cancelButton->setEnabled(true);
        ui->cancelButton->setText(
            tr("Stop creating", "Button that cancels the catalogs being built"));
        return;
    }
    ui->cancelButton->setEnabled(pending > 0);
    ui->cancelButton->setText(
        pending > 0 ? tr("Start creating (%1)").arg(pending)
                    : tr("Start creating",
                         "Button that builds the folders which were added to the list above"));
}

void ManageDatabaseDialog::handleBuildStateChanged()
{
    if (m_buildQueue->isBuilding() || m_buildQueue->isStopping()) {
        progressButtonStates();
    } else {
        normalButtonStates();
    }
}

void ManageDatabaseDialog::progressButtonStates()
{
    setAcceptDrops(false);
    ui->treeWidget->setEnabled(false);
    ui->booksTree->setEnabled(false);
    ui->editTagsButton->setEnabled(false);
    ui->addButton->setEnabled(false);
    ui->moreButton->setEnabled(false);
    ui->purgeMissingAction->setEnabled(false);
    updateCatalogActions();

    ui->statusLabel->clear();
    ui->volumeNameLabel->clear();
    ui->progressBar->setRange(0, 0);
    ui->progressWidget->setVisible(true);
    updateStartButton();
}

void ManageDatabaseDialog::resetCatalogList()
{
    const QTreeWidgetItem *previous = ui->treeWidget->currentItem();
    const int previousId = previous ? previous->data(0, Qt::UserRole).toInt() : 0;
    const QSignalBlocker blocker(ui->treeWidget);
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
    for (const CatalogRecord &catalog : m_buildQueue->requests()) {
        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, "* " + catalog.name);
        item->setText(1,
                      tr("Not created yet",
                         "Representation of time indicating that the catalog is not currently "
                         "created and will be generated from now"));
        item->setText(2, catalog.path);
        item->setToolTip(2, catalog.path);
        // The row carries the id the request was added with, so it still names
        // the same request after the list around it changed.
        item->setData(0, Qt::UserRole, catalog.id);
        item->setBackground(0, QBrush(QColor("lightgreen")));
        QFont font = item->font(0);
        font.setItalic(true);
        item->setFont(0, font);
        ui->treeWidget->addTopLevelItem(item);
    }

    QTreeWidgetItem *selected = nullptr;
    for (int row = 0; row < ui->treeWidget->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = ui->treeWidget->topLevelItem(row);
        const int id = item->data(0, Qt::UserRole).toInt();
        if (id == previousId) {
            selected = item;
            break;
        }
    }
    if (!selected && ui->treeWidget->topLevelItemCount() > 0) {
        selected = ui->treeWidget->topLevelItem(0);
    }
    ui->treeWidget->setCurrentItem(selected);
    handleCatalogSelectionChanged();
}

void ManageDatabaseDialog::selectPendingCatalog(int requestId)
{
    for (int row = 0; row < ui->treeWidget->topLevelItemCount(); ++row) {
        auto *item = ui->treeWidget->topLevelItem(row);
        if (item->data(0, Qt::UserRole).toInt() == requestId) {
            ui->treeWidget->setCurrentItem(item);
            ui->treeWidget->scrollToItem(item);
            break;
        }
    }
}

void ManageDatabaseDialog::handleCatalogSelectionChanged()
{
    const auto *previous = ui->booksTree->currentItem();
    const int previousId = previous ? previous->data(0, Qt::UserRole).toInt() : 0;
    ui->booksTree->clear();
    ui->editTagsButton->setEnabled(false);
    updateCatalogActions();
    // The cover that was shown belonged to the catalog that was selected.
    updateCover();
    if (!m_catalogDatabase || m_buildQueue->isBuilding()) {
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
    QTreeWidgetItem *selected = nullptr;
    for (const QPair<VolumeThumbRecord, QStringList> &entry : volumes) {
        const VolumeThumbRecord &volume = entry.first;
        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, volume.name.isEmpty() ? volume.realname : volume.name);
        item->setToolTip(0, volume.realname);
        item->setText(1, entry.second.join(QStringLiteral(", ")));
        item->setData(0, Qt::UserRole, volume.id);
        ui->booksTree->addTopLevelItem(item);
        if (volume.id == previousId) {
            selected = item;
        }
    }
    if (ui->booksTree->topLevelItemCount() > 0) {
        // Start on the first book so that the editor is one press away.
        ui->booksTree->setCurrentItem(selected ? selected : ui->booksTree->topLevelItem(0));
    }
    updateCover();
}

void ManageDatabaseDialog::updateCatalogActions()
{
    const QTreeWidgetItem *selected = ui->treeWidget->currentItem();
    const bool enabled = selected && !m_buildQueue->isBuilding();
    ui->editAction->setEnabled(enabled);
    ui->deleteAction->setEnabled(enabled);
    ui->openInExplorerAction->setEnabled(enabled && !selected->text(2).isEmpty());
    ui->deleteAllAction->setEnabled(!m_buildQueue->isBuilding() &&
                                    (!m_catalogs.isEmpty() || !m_buildQueue->isEmpty()));
}

void ManageDatabaseDialog::handleCatalogContextMenu(const QPoint &position)
{
    if (m_buildQueue->isBuilding()) {
        return;
    }
    QTreeWidgetItem *item = ui->treeWidget->itemAt(position);
    if (!item) {
        return;
    }
    ui->treeWidget->setCurrentItem(item);
    QMenu menu(this);
    menu.addAction(ui->editAction);
    menu.addAction(ui->openInExplorerAction);
    menu.addSeparator();
    menu.addAction(ui->deleteAction);
    menu.exec(ui->treeWidget->viewport()->mapToGlobal(position));
}

void ManageDatabaseDialog::updateCover()
{
    // One cover at a time: the list holds the books, and the database holds
    // the covers of a catalog that can be far larger than the screen.
    const QTreeWidgetItem *current = ui->booksTree->currentItem();
    if (!current) {
        ui->coverPane->clearCover();
        return;
    }
    if (!m_catalogDatabase) {
        ui->coverPane->setStoredCover(QByteArray());
    } else {
        ui->coverPane->setStoredCover(
            m_catalogDatabase->volumeThumbnail(current->data(0, Qt::UserRole).toInt()));
    }
}

bool ManageDatabaseDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->treeWidget && event->type() == QEvent::ContextMenu &&
        static_cast<QContextMenuEvent *>(event)->reason() == QContextMenuEvent::Keyboard) {
        if (auto *current = ui->treeWidget->currentItem()) {
            handleCatalogContextMenu(ui->treeWidget->visualItemRect(current).center());
        }
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void ManageDatabaseDialog::handleOpenInExplorerActionTriggered()
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
    for (const TagRecord &tag : m_catalogDatabase->tagsByCount()) {
        knownTags << tag.name;
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
    if (!m_catalogDatabase->setVolumeDetails(volumeId, dialog.displayName(), dialog.tags())) {
        QMessageBox::warning(this, tr("Catalog database"), m_catalogDatabase->errorMessage());
        return;
    }

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
    ui->statusLabel->clear();
    // The queue announces the request it took, and the list follows it.
    selectPendingCatalog(m_buildQueue->add(catalog));
}

void ManageDatabaseDialog::dropEvent(QDropEvent *e)
{
    if (m_buildQueue->isBuilding() || !e->mimeData()->hasUrls()) {
        return;
    }
    QList<QUrl> urlList = e->mimeData()->urls();
    int lastRequestId = 0;
    for (int i = 0; i < urlList.size(); i++) {
        QUrl url = urlList[i];
        if (!url.isLocalFile()) {
            continue;
        }
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
        if (catalog.path.isEmpty()) {
            continue;
        }
        lastRequestId = m_buildQueue->add(catalog);
    }

    ui->statusLabel->clear();
    selectPendingCatalog(lastRequestId);
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

void ManageDatabaseDialog::handleCatalogStored(const CatalogRecord catalog)
{
    m_catalogs[catalog.id] = catalog;
}

void ManageDatabaseDialog::handleBuildFinished(bool canceled)
{
    m_catalogs = m_catalogDatabase->catalogs();

    if (canceled) {
        ui->statusLabel->setText(tr("Catalog creation was cancelled.",
                                    "Body of message box when catalog generation is canceled"));
    } else if (m_buildQueue->isEmpty()) {
        ui->statusLabel->setText(
            tr("Catalog creation completed.",
               "Body of message box when catalog generation finished successfully"));
    } else {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("Catalog creation incomplete"));
        msgBox.setText(tr("Catalog(s) left unstored: %1",
                          "Body of message box when some catalogs could not be stored")
                           .arg(m_buildQueue->requests().size()));
        msgBox.setInformativeText(m_catalogDatabase->errorMessage());
        msgBox.exec();
    }
}

void ManageDatabaseDialog::handleCancelButtonClicked()
{
    if (!m_catalogDatabase) {
        return;
    }
    if (m_buildQueue->isBuilding()) {
        m_buildQueue->stop();
        return;
    }
    if (m_buildQueue->isEmpty()) {
        return;
    }
    if (!m_catalogDatabase->ensureReady()) {
        reportCatalogDatabaseProblem();
        return;
    }
    m_buildQueue->start();
}

void ManageDatabaseDialog::reject()
{
    if (!m_catalogDatabase || m_buildQueue->isBuilding() || m_buildQueue->isStopping()) {
        // A running build is asked to stop and the dialog closes: the worker
        // rolls it back, and the panel reads the catalog when it is done.
        m_buildQueue->stop();
        QDialog::reject();
        return;
    }
    if (!m_buildQueue->isEmpty()) {
        const QMessageBox::StandardButton answer =
            QMessageBox::question(this,
                                  tr("Close"),
                                  tr("%1 added folder(s) are not created yet. Close and discard "
                                     "them?")
                                      .arg(m_buildQueue->requests().size()),
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

void ManageDatabaseDialog::handleEditActionTriggered()
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
        if (!m_catalogDatabase->updateCatalogName(id, catalog.name)) {
            QMessageBox::warning(this, tr("Catalog database"), m_catalogDatabase->errorMessage());
            return;
        }
        m_catalogs[id] = catalog;
    } else {
        // A catalog that is only waiting to be built stays in the request.
        catalog = m_buildQueue->request(id);
        if (catalog.name.isEmpty()) {
            return;
        }
        if (!databaseSettingDialog(catalog, false)) {
            return;
        }
        m_buildQueue->update(id, catalog);
    }
}

void ManageDatabaseDialog::handleDeleteActionTriggered()
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
        if (!m_catalogDatabase->deleteCatalog(id)) {
            QMessageBox::warning(this, tr("Catalog database"), m_catalogDatabase->errorMessage());
            return;
        }
        m_catalogs.remove(id);
    } else {
        m_buildQueue->remove(id);
    }
}

void ManageDatabaseDialog::handleDeleteAllActionTriggered()
{
    if (!m_catalogDatabase) {
        return;
    }
    if (!confirmRemoval(
            tr("Delete all catalogs"),
            tr("Delete every catalog from the list? The image files are not deleted."))) {
        return;
    }
    if (!m_catalogDatabase->deleteAllCatalogs()) {
        QMessageBox::warning(this, tr("Catalog database"), m_catalogDatabase->errorMessage());
        return;
    }
    m_catalogs.clear();
    m_buildQueue->clear();
}
