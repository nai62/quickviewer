#include <QtWidgets>
#include "ui_catalogwindow.h"
#include "ui_mainwindow.h"

#include "catalogwindow.h"
#include "managedatabasedialog.h"
#include "qvapplication.h"
#include "searchwords.h"
#include "volumetagdialog.h"
#include "flowlayout.h"

#ifdef Q_OS_WIN
#    include <Windows.h>
#endif

static void clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    if (!layout) {
        return;
    }
    while ((item = layout->takeAt(0))) {
        if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

/** The magnifier of the search field, drawn from the palette of \a owner. */
static QIcon searchFieldIcon(const QWidget &owner)
{
    // A line edit paints its action at the size the style asks for and scales
    // the pixmap to it, so the shape of the drawing is what decides how big the
    // magnifier looks. A line of text sets the canvas, and the drawing keeps to
    // the middle three quarters of it, so the magnifier reads as one more glyph
    // in that line instead of filling the height of the field.
    const qreal side = qMax(12, owner.fontMetrics().height());
    const qreal span = side * 3.0 / 4.0;
    const qreal inset = (side - span) / 2.0;
    const qreal lens = span * 5.0 / 8.0;
    const qreal ratio = owner.devicePixelRatioF();
    QPixmap pixmap(QSize(qCeil(side * ratio), qCeil(side * ratio)));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(owner.palette().color(QPalette::PlaceholderText), qMax(1.0, side / 12.0));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(inset, inset, lens, lens));
    painter.drawLine(QPointF(inset + lens * 0.78, inset + lens * 0.78),
                     QPointF(inset + span, inset + span));
    return QIcon(pixmap);
}

CatalogWindow::CatalogWindow(QWidget *parent, Ui::MainWindow *uiMain)
    : QWidget(parent),
      ui(new Ui::CatalogWindow),
      m_folderViewMenu(this),
      m_itemModel(this)
{
    ui->setupUi(this);
    connect(
        qApp->languageSelector(), &LanguageManager::languageChanged, this, [this](const QString &) {
            ui->retranslateUi(this);
            ui->searchEdit->setPlaceholderText(
                tr("Search titles", "Gray text that prompts a keyword search of Volume"));
            ui->searchEdit->setToolTip(tr("Type part of a title and press Enter to search.",
                                          "Tooltip of the field that searches the titles of the "
                                          "catalog"));
            if (m_volumes.isEmpty()) {
                ui->statusLabel->setText(
                    tr("Drop an image folder here to create a catalog.",
                       "Status bar text briefly explaining how to use CatalogWindow"));
            } else {
                resetVolumes();
            }
        });

#ifdef Q_OS_MACOS
    ui->menuBar->setNativeMenuBar(false);
#endif

    // VolumeView
    m_itemModel.setViewMode(qApp->CatalogViewModeSetting());
    // Every display mode draws its cover at the icon size, so that is the box
    // the model fits a cover into.
    m_itemModel.setCoverBox(ui->volumeList->iconSize());
    ui->volumeList->setModel(&m_itemModel);
    ui->volumeList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->volumeList,
            &QListView::customContextMenuRequested,
            this,
            &CatalogWindow::handleVolumeListContextMenu);

    // SearchEdit
    connect(ui->searchEdit,
            &QLineEdit::editingFinished,
            this,
            &CatalogWindow::handleSearchLineEditEditingFinished);
    ui->searchEdit->setPlaceholderText(
        tr("Search titles", "Gray text that prompts a keyword search of Volume"));
    // A search field should look like one: a magnifier, a hint of what it
    // searches, and a way to clear what was typed.
    ui->searchEdit->addAction(searchFieldIcon(*ui->searchEdit), QLineEdit::LeadingPosition);
    ui->searchEdit->setClearButtonEnabled(true);
    ui->searchEdit->setToolTip(tr("Type part of a title and press Enter to search.",
                                  "Tooltip of the field that searches the titles of the catalog"));

    // TagFrame
    if (!qApp->ShowTagBar()) {
        ui->tagFrame->setVisible(false);
    }
    if (ui->tagFrame->layout()) {
        clearLayout(ui->tagFrame->layout());
        delete ui->tagFrame->layout();
    }
    QLayout *layout = new FlowLayout;
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    ui->tagFrame->setLayout(layout);

    // Status Bar
    ui->statusBar->addPermanentWidget(ui->statusLabel);
    ui->statusLabel->setText(tr("Drop an image folder here to create a catalog.",
                                "Status bar text briefly explaining how to use CatalogWindow"));

    ui->menu_View->addAction(uiMain->actionShowTagBar);
    ui->menu_View->addAction(uiMain->actionCatalogIconLongText);
    ui->menu_View->addAction(uiMain->actionCatalogTitleWithoutOptions);
    ui->menu_View->addAction(uiMain->actionSearchTitleWithOptions);

    m_folderViewMenu.addAction(ui->actionFolderViewList);
    m_folderViewMenu.addAction(ui->actionFolderViewIcon);
    m_folderViewMenu.addAction(ui->actionFolderViewIconNoText);
}

CatalogWindow::~CatalogWindow()
{
    delete ui;
}

void CatalogWindow::setCatalogDatabase(CatalogDatabase *catalogDatabase)
{
    m_catalogDatabase = catalogDatabase;
    if (!m_catalogDatabase->ensureReady()) {
        // The window stays usable: the status bar carries the reason the list
        // is empty, and the user can put a readable database in place.
        m_volumes.clear();
        m_volumeSearch.clear();
        resetVolumes();
        ui->statusLabel->setText(m_catalogDatabase->errorMessage());
        return;
    }
    refreshCatalog();

    resetViewMode();
}

void CatalogWindow::refreshCatalog()
{
    m_volumes = m_catalogDatabase->volumes();
    initTagButtons();
    searchByWord(true);
}

void CatalogWindow::resetViewMode()
{
    switch (qApp->CatalogViewModeSetting()) {
    case qvEnums::CatalogViewMode::List:
        handleFolderViewListActionTriggered();
        break;
    case qvEnums::CatalogViewMode::Icon:
        handleFolderViewIconActionTriggered();
        break;
    case qvEnums::CatalogViewMode::IconNoText:
        handleFolderViewIconNoTextActionTriggered();
        break;
    }
}

void CatalogWindow::setAsToplevelWindow()
{
    ui->menuBar->setVisible(true);
    ui->statusLabel->setWordWrap(false);
}

void CatalogWindow::setAsInnerWidget()
{
    ui->menuBar->setVisible(false);
    ui->statusLabel->setWordWrap(true);
    ui->searchEdit->setFocus();
}

bool CatalogWindow::isCatalogSearching()
{
    return ui->searchEdit->hasFocus();
}

void CatalogWindow::resetTagButtons(QStringList buttons, QStringList checks)
{
    // Reset Tag Buttons
    clearLayout(ui->tagFrame->layout());
    for (int i = 0; i < buttons.size(); i++) {
        QPushButton *b = new QPushButton;
        QString name = buttons[i];
        b->setText(name);
        b->setCheckable(true);
        if (checks.contains(name)) {
            b->setChecked(true);
        }
        connect(b, &QPushButton::clicked, this, &CatalogWindow::handleTagButtonClicked);
        ui->tagFrame->layout()->addWidget(b);
    }
}

void CatalogWindow::initTagButtons()
{
    // Whatever the user pressed stays pressed while the bar is rebuilt: the
    // tags a reload no longer offers drop out on their own.
    const QStringList checked = getTagWords();
    QStringList buttons;
    if (qApp->ShowTagBar()) {
        QMap<int, TagRecord *> tags = m_catalogDatabase->tagsByCount();
        // One tag alone is not worth a bar, but the buttons of the state
        // before still have to go: a tag can lose its last volume.
        if (tags.size() > 1) {
            int cnt = 0;
            for (int i : tags.keys()) {
                buttons << tags[i]->name;
                if (cnt++ >= 7) {
                    break;
                }
            }
        }
    }
    resetTagButtons(buttons, checked);
}

QStringList CatalogWindow::getTagWords()
{
    QStringList result;
    QLayout *layout = ui->tagFrame->layout();
    for (int i = 0; i < layout->count(); i++) {
        QLayoutItem *item = layout->itemAt(i);
        if (!item->widget()) {
            continue;
        }
        QPushButton *b = dynamic_cast<QPushButton *>(item->widget());
        if (!b || !b->isChecked()) {
            continue;
        }
        result << b->text();
    }
    return result;
}

void CatalogWindow::resetVolumes()
{
    m_itemModel.setVolumes(&m_volumeSearch);
    if (m_volumes.isEmpty()) {
        ui->statusLabel->setText(tr("Drop an image folder here to create a catalog.",
                                    "Status bar text briefly explaining how to use CatalogWindow"));
        return;
    }
    // A volume without a cover cannot be shown, so it does not belong in the
    // number the status bar compares against.
    const int listable = listableVolumeCount();
    const int shown = int(m_volumeSearch.size());
    QString volumestxt;
    if (shown == listable) {
        // Nothing is filtered out, so the count of the two is the same.
        volumestxt = tr("%1 volumes", "Text of the status bar showing how many volumes are listed")
                         .arg(listable);
    } else {
        volumestxt =
            tr("%1 of %2 volumes",
               "Text of the status bar showing [the number of hits]/[total number] of Volume")
                .arg(shown)
                .arg(listable);
    }
    const int hidden = int(m_volumes.size()) - listable;
    if (hidden > 0) {
        volumestxt += QStringLiteral(" ");
        volumestxt += tr("(%1 without a cover are not shown)",
                         "Status bar note about volumes the catalog holds but does not list")
                          .arg(hidden);
    }
    ui->statusLabel->setText(volumestxt);
}

int CatalogWindow::listableVolumeCount() const
{
    int count = 0;
    for (const VolumeThumbRecord &volume : m_volumes) {
        if (!volume.thumbnail.isEmpty()) {
            ++count;
        }
    }
    return count;
}

void CatalogWindow::searchByWord(bool doForce)
{
    QString search = ui->searchEdit->text();

    const QStringList tagWords = getTagWords();

    search = search.trimmed();
    if (!doForce && search == m_lastSearchWord && tagWords == m_lastTagWords) {
        return;
    }
    m_lastSearchWord = search;
    m_lastTagWords = tagWords;

    //    int cnt = 0;
    m_volumeSearch.clear();
    SearchWords searchwords(search);
    for (const VolumeThumbRecord &vtr : m_volumes) {
        if (vtr.thumbnail.isEmpty()) {
            continue;
        }
        const bool hasTags =
            std::all_of(tagWords.cbegin(), tagWords.cend(), [&vtr](const QString &tag) {
                return vtr.tags.contains(tag, Qt::CaseInsensitive);
            });
        if (!hasTags) {
            continue;
        }
        QString title = qApp->SearchTitleWithOptions() && !vtr.name.isEmpty() ? vtr.nameNoCase
                                                                              : vtr.realnameNoCase;
        if (!searchwords.match(title)) {
            continue;
        }
        //        if(qApp->MaxShowFrontpage() < ++cnt)
        //            break;
        m_volumeSearch.append(const_cast<VolumeThumbRecord *>(&vtr));
    }
    resetVolumes();
}

void CatalogWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasFormat("text/uri-list")) {
        e->acceptProposedAction();
    }
}

void CatalogWindow::dropEvent(QDropEvent *e)
{
    ManageDatabaseDialog dialog(this);
    dialog.setCatalogDatabase(m_catalogDatabase);
    dialog.dropEvent(e);
    dialog.exec();

    refreshCatalog();
}

void CatalogWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    qApp->setCatalogViewWidth(event->size().width());
}

void CatalogWindow::handleFolderViewButtonClicked()
{
    QWidget *widget = ui->folderViewButton;

    QPoint p = widget->mapToGlobal(QPoint(0, widget->height()));
    m_folderViewMenu.exec(p);
}

void CatalogWindow::handleFolderViewListActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::CatalogViewMode::List);
    ui->actionFolderViewList->setChecked(true);
    ui->actionFolderViewIcon->setChecked(false);
    ui->actionFolderViewIconNoText->setChecked(false);
    m_itemModel.setViewMode(qvEnums::CatalogViewMode::List);
    ui->volumeList->setResizeMode(QListView::Adjust);
    // A list lays its rows out top to bottom. Leaving the wrapping of the icon
    // layout on makes the view hold one grid cell empty above the first row.
    ui->volumeList->setFlow(QListView::TopToBottom);
    ui->volumeList->setWrapping(false);
    if (qApp->IconLongText()) {
        ui->volumeList->setGridSize(QSize(300, 100));
        ui->volumeList->setTextElideMode(Qt::ElideRight);
    } else {
        ui->volumeList->setGridSize(QSize(200, 100));
        ui->volumeList->setTextElideMode(Qt::ElideNone);
    }
    ui->volumeList->setViewMode(QListView::ListMode);
    ui->volumeList->setUniformItemSizes(true);

    resetVolumes();
}

void CatalogWindow::handleFolderViewIconActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::CatalogViewMode::Icon);
    ui->actionFolderViewList->setChecked(false);
    ui->actionFolderViewIcon->setChecked(true);
    ui->actionFolderViewIconNoText->setChecked(false);
    m_itemModel.setViewMode(qvEnums::CatalogViewMode::Icon);
    ui->volumeList->setResizeMode(QListView::Adjust);
    ui->volumeList->setFlow(QListView::LeftToRight);
    ui->volumeList->setWrapping(true);
    if (qApp->IconLongText()) {
        ui->volumeList->setGridSize(QSize(150, 170));
        ui->volumeList->setTextElideMode(Qt::ElideRight);
    } else {
        ui->volumeList->setGridSize(QSize(150, 120));
        ui->volumeList->setTextElideMode(Qt::ElideNone);
    }
    ui->volumeList->setViewMode(QListView::IconMode);
    ui->volumeList->setUniformItemSizes(true);

    resetVolumes();
}

void CatalogWindow::handleFolderViewIconNoTextActionTriggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::CatalogViewMode::IconNoText);
    ui->actionFolderViewList->setChecked(false);
    ui->actionFolderViewIcon->setChecked(false);
    ui->actionFolderViewIconNoText->setChecked(true);
    m_itemModel.setViewMode(qvEnums::CatalogViewMode::IconNoText);
    ui->volumeList->setFlow(QListView::LeftToRight);
    ui->volumeList->setWrapping(true);
    ui->volumeList->setResizeMode(QListView::Adjust);
    ui->volumeList->setViewMode(QListView::IconMode);
    ui->volumeList->setGridSize(QSize(100, 100));
    ui->volumeList->setUniformItemSizes(true);

    resetVolumes();
}

void CatalogWindow::handleManageCatalogButtonClicked()
{
    ManageDatabaseDialog dialog(this);
    dialog.setCatalogDatabase(m_catalogDatabase);
    dialog.exec();

    refreshCatalog();
}

void CatalogWindow::handleSearchEditTextChanged(QString)
{
    //    if(m_volumes.size() < qApp->MaxSearchByCharChanged())
    searchByWord();
}

void CatalogWindow::handleSearchLineEditEditingFinished()
{
    searchByWord();
}

void CatalogWindow::handleVolumeListItemDoubleClicked(const QModelIndex &index)
{
    int row = index.row();
    if (!index.isValid() || index.model() != &m_itemModel || row < 0 ||
        row >= m_volumeSearch.size()) {
        return;
    }
    emit openVolume(OpenTarget::container(m_volumeSearch[row]->path));

    // reset tag buttons as current book
    QList<TagRecord> tags = m_catalogDatabase->getTagsFromVolumeId(m_volumeSearch[row]->id);
    QStringList tagtxt;
    for (const TagRecord &t : tags) {
        tagtxt << t.name;
    }

    resetTagButtons(tagtxt, getTagWords());
}

void CatalogWindow::handleVolumeListContextMenu(const QPoint &position)
{
    const QModelIndex index = ui->volumeList->indexAt(position);
    if (!index.isValid()) {
        return;
    }
    QMenu menu;
    QAction *edit =
        menu.addAction(tr("Edit tags...", "Context menu entry of a book in the catalog list"));
    if (menu.exec(ui->volumeList->viewport()->mapToGlobal(position)) != edit) {
        return;
    }
    editVolumeTags(index.row());
}

void CatalogWindow::editVolumeTags(int row)
{
    if (!m_catalogDatabase || row < 0 || row >= m_volumeSearch.size()) {
        return;
    }
    const VolumeThumbRecord *volume = m_volumeSearch.at(row);
    const int volumeId = volume->id;

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
    dialog.setVolume(volume->name, volume->realname, knownTags, volumeTags);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (!m_catalogDatabase->setVolumeDetails(volumeId, dialog.displayName(), dialog.tags())) {
        QMessageBox::warning(this, tr("Catalog database"), m_catalogDatabase->errorMessage());
        return;
    }

    // The tags and the titles the list shows both changed.
    refreshCatalog();
}

void CatalogWindow::handleSearchTitleWithOptionsActionTriggered(bool checked)
{
    qApp->setSearchTitleWithOptions(checked);
    searchByWord(true);
}

void CatalogWindow::handleCatalogTitleWithoutOptionsActionTriggered(bool checked)
{
    qApp->setTitleWithoutOptions(checked);
    searchByWord(true);
}

void CatalogWindow::handleTagButtonClicked()
{
    searchByWord();
}

void CatalogWindow::handleShowTagBarActionTriggered(bool checked)
{
    qApp->setShowTagBar(checked);
    ui->tagFrame->setVisible(checked);
    if (checked) {
        initTagButtons();
    } else {
        resetTagButtons(QStringList(), QStringList());
    }
    searchByWord();
}

void CatalogWindow::closeEvent(QCloseEvent *e)
{
    QWidget::closeEvent(e);
    emit closed();
}
