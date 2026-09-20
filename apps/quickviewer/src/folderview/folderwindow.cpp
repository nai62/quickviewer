#include <QtWidgets>
#include <QCollator>

#include "ui_folderwindow.h"
#include "ui_mainwindow.h"

#include "folderwindow.h"
#include "models/volume.h"
#include "models/qvapplication.h"
#include "qmousesequence.h"
#include "startupprofiler.h"

namespace {
QIcon clockIcon(const QPalette &palette)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(palette.color(QPalette::ButtonText));
    pen.setWidthF(1.8);
    painter.setPen(pen);
    painter.drawEllipse(QRectF(3.5, 3.5, 17.0, 17.0));
    painter.drawLine(QPointF(12.0, 12.0), QPointF(12.0, 7.0));
    painter.drawLine(QPointF(12.0, 12.0), QPointF(16.0, 14.0));
    return QIcon(pixmap);
}
}

FolderWindow::FolderWindow(QWidget *parent, Ui::MainWindow *uiMain)
    : QWidget(parent),
      ui(new Ui::FolderWindow),
      m_itemContextMenu(nullptr),
      m_historyButton(nullptr),
      m_itemModel(this),
      m_itemDelegate(parent, this)
{
    ui->setupUi(this);

    ui->folderView->setMouseTracking(true);
    ui->folderView->installEventFilter(this);

    // folderView
    m_itemModel.setTextStyle(
        ui->folderView->font(), ui->folderView->palette(), ui->folderView->devicePixelRatioF());
    ui->folderView->setModel(&m_itemModel);
    ui->folderView->setItemDelegate(&m_itemDelegate);
    connect(ui->folderView->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            &FolderWindow::updateTextRowRange);
    connect(ui->folderView,
            &FolderListView::unusedMouseButton,
            this,
            &FolderWindow::handleUnusedMouseButton);
    connect(ui->folderView,
            &QWidget::customContextMenuRequested,
            this,
            &FolderWindow::handleFolderViewContextMenuRequested);

    // The item context menu is a plain menu; its action lives in the form.
    m_itemContextMenu = new QMenu(this);
    m_itemContextMenu->addAction(ui->actionSetAsHomeFolder);

    StartupProfiler::mark("folder-window.history-button.begin");
    setupHistoryButton(uiMain);
    StartupProfiler::mark("folder-window.history-button.end");

    ui->horizontalLayout->addWidget(m_historyButton);

    connect(
        qApp->languageSelector(), &LanguageManager::languageChanged, this, [this](const QString &) {
            ui->retranslateUi(this);
            m_historyButton->setText(tr("History"));
            m_historyButton->setToolTip(tr("Open history"));
            if (m_volumes.size() == 1 && m_volumes.first().type == FolderItem::NoItems) {
                m_volumes.first().name =
                    tr("No folders or archives found.",
                       "Display when there is no display item in Folder Window");
                m_itemModel.setVolumes(&m_volumes);
            }
        });

    // Freeze the panel's effective minimum before a path or folder entries
    // are loaded. The constraint represents the controls needed by the UI,
    // not the contents of the current folder.
    setMinimumWidth(minimumSizeHint().width());
}

FolderWindow::~FolderWindow()
{
    // The view holds raw pointers to the model and the delegate. Qt resets the
    // model pointer when the model dies, but it never clears the delegate, and
    // the child widgets are destroyed after the members of this object: destroy
    // the view while the model and the delegate are still alive, otherwise it
    // reads released memory while it is being destroyed.
    delete ui->folderView;
    if (m_itemContextMenu) {
        delete m_itemContextMenu;
    }
    delete ui;
}

void FolderWindow::setupHistoryButton(Ui::MainWindow *uiMain)
{
    m_historyButton = new QToolButton(ui->frame);
    m_historyButton->setObjectName(QStringLiteral("historyButton"));
    m_historyButton->setText(tr("History"));
    m_historyButton->setToolTip(tr("Open history"));
    m_historyButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_historyButton->setPopupMode(QToolButton::InstantPopup);
    m_historyButton->setAutoRaise(true);

    QIcon historyIcon = QIcon::fromTheme(QStringLiteral("view-history"));
    if (historyIcon.isNull()) {
        historyIcon = QIcon::fromTheme(QStringLiteral("document-open-recent"));
    }
    if (historyIcon.isNull()) {
        historyIcon = QIcon::fromTheme(QStringLiteral("preferences-system-time"));
    }
    if (historyIcon.isNull()) {
        historyIcon = clockIcon(palette());
    }
    m_historyButton->setIcon(historyIcon);

    if (uiMain) {
        m_historyButton->setMenu(uiMain->menuHistory);
    } else {
        m_historyButton->setEnabled(false);
    }
}

bool FolderWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->folderView) {
        if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange ||
            event->type() == QEvent::DevicePixelRatioChange || event->type() == QEvent::Show) {
            m_itemModel.setTextStyle(ui->folderView->font(),
                                     ui->folderView->palette(),
                                     ui->folderView->devicePixelRatioF());
        }
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
            updateTextRowRange();
        }
    }
    return QObject::eventFilter(obj, event);
}

/**
 * The menu belongs to the entry under the pointer, and showing it never opens
 * that entry: the position is in the viewport's coordinates, which is what the
 * list reads a row from.
 */
void FolderWindow::handleFolderViewContextMenuRequested(const QPoint &pos)
{
    m_contextMenuIndex = ui->folderView->indexAt(pos);
    const bool isFolder = m_contextMenuIndex.isValid() &&
                          m_contextMenuIndex.row() < m_volumes.size() &&
                          m_volumes.at(m_contextMenuIndex.row()).type == FolderItem::Dir;
    ui->actionSetAsHomeFolder->setEnabled(isFolder);
    m_itemContextMenu->exec(ui->folderView->viewport()->mapToGlobal(pos));
}

/**
 * The mouse buttons the list does not use behave like the keys it does not use:
 * the window maps them to actions, and the back and forward buttons step a page
 * by default.
 */
void FolderWindow::handleUnusedMouseButton(Qt::MouseButtons buttons)
{
    QMouseValue value(QKeySequence(qApp->keyboardModifiers()), buttons, 0);
    if (QAction *action = qApp->mouseActions().getActionByValue(value)) {
        action->trigger();
    }
}

void FolderWindow::handleSetAsHomeFolderActionTriggered()
{
    const int row = m_contextMenuIndex.row();
    if (row < 0 || row >= m_volumes.size()) {
        return;
    }
    const FolderItem &item = m_volumes[row];
    QDir dir(m_currentPath);
    qApp->setHomeFolderPath(dir.filePath(item.name));
}

void FolderWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasFormat("text/uri-list")) {
        e->acceptProposedAction();
    }
}

void FolderWindow::dropEvent(QDropEvent *e)
{
    if (!e->mimeData()->hasUrls()) {
        return;
    }
    QList<QUrl> urlList = e->mimeData()->urls();
    for (int i = 0; i < urlList.size(); i++) {
        QUrl url = urlList[i];
        QFileInfo info(url.toLocalFile());
        if (info.isDir() || info.isFile()) {
            setFolderPath(info.absoluteFilePath(), false);
            break;
        }
    }
}

void FolderWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Docked width is saved from QSplitter::splitterMoved. Ignore automatic
    // layout resizes, which otherwise feed transient startup widths back into
    // the next launch.
    if (qApp->SaveFolderViewWidth() && isWindow() && isVisible()) {
        qApp->setFolderViewWidth(event->size().width());
    }
}

namespace {

// The panel mirrors the order of the viewer pages, so names use the same
// natural collation as Volume::applyPageSort().
bool nameLessThan(const QString &lhs, const QString &rhs)
{
    static const QCollator collator = [] {
        QCollator result;
        result.setNumericMode(true);
        return result;
    }();
    return collator.compare(lhs, rhs) < 0;
}

bool sortDescending(qvEnums::ImageSortBy sortBy)
{
    return sortBy == qvEnums::ImageSortBy::SortByFileNameDescending ||
           sortBy == qvEnums::ImageSortBy::SortByFileSizeDescending ||
           sortBy == qvEnums::ImageSortBy::SortByModifiedTimeDescending;
}

bool sortByModifiedTime(qvEnums::ImageSortBy sortBy)
{
    return sortBy == qvEnums::ImageSortBy::SortByModifiedTime ||
           sortBy == qvEnums::ImageSortBy::SortByModifiedTimeDescending;
}

bool sortByFileSize(qvEnums::ImageSortBy sortBy)
{
    return sortBy == qvEnums::ImageSortBy::SortByFileSize ||
           sortBy == qvEnums::ImageSortBy::SortByFileSizeDescending;
}

// Orders two entries of the same group (folders, or files) with the key of the
// viewer's image sort. Folders carry no size, so size sorts fall back to the
// name for them.
bool sortKeyLessThan(const FolderItem &lhs, const FolderItem &rhs, qvEnums::ImageSortBy sortBy)
{
    const bool descending = sortDescending(sortBy);
    if (sortByModifiedTime(sortBy) && lhs.updated_at != rhs.updated_at) {
        return descending ? lhs.updated_at > rhs.updated_at : lhs.updated_at < rhs.updated_at;
    }
    if (sortByFileSize(sortBy) && lhs.size != rhs.size) {
        return descending ? lhs.size > rhs.size : lhs.size < rhs.size;
    }
    return descending ? nameLessThan(rhs.name, lhs.name) : nameLessThan(lhs.name, rhs.name);
}

bool folderViewLessThan(const FolderItem &lhs, const FolderItem &rhs, qvEnums::ImageSortBy sortBy)
{
    // Folders are navigator entries, not pages of the viewer, so they stay above
    // the files in every sort mode.
    const bool lhsIsFolder = lhs.type == FolderItem::Dir;
    const bool rhsIsFolder = rhs.type == FolderItem::Dir;
    if (lhsIsFolder != rhsIsFolder) {
        return lhsIsFolder;
    }
    return sortKeyLessThan(lhs, rhs, sortBy);
}

} // namespace

void FolderWindow::setFolderPath(QString path, bool showParent)
{
#ifdef Q_OS_WIN
    if (path.isEmpty()) {
        m_volumes.clear();
        {
            m_currentPath = "";
            QList<QFileInfo> drives = QDir::drives();
            foreach (QFileInfo drive, drives) {
                m_volumes << FolderItem(
                    drive.absoluteFilePath(), FolderItem::Dir, drive.lastModified());
            }
        }
    } else
#else
    if (path.isEmpty()) {
        path = "/";
    }
#endif
    {
        QFileInfo fileinfo(path);
        if (!fileinfo.exists()) {
            return;
        }

        QDir dir;
        if (fileinfo.isDir()) {
            dir.setPath(QDir::toNativeSeparators(showParent ? fileinfo.absolutePath() : path));
        } else {
            dir.setPath(QDir::toNativeSeparators(fileinfo.dir().path()));
        }

        m_currentPath = dir.path();
        resetPathLabel(width());

        m_volumes.clear();
        {
            QStringList subfolders =
                dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::Unsorted);
            foreach (const QString &sf, subfolders) {
                QFileInfo fi(dir.absoluteFilePath(sf));
                m_volumes << FolderItem(sf, FolderItem::Dir, fi.lastModified());
            }
        }

        {
            foreach (const QString name,
                     dir.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::Unsorted)) {
                const bool isArchive = IFileLoader::isArchiveFile(name);
                const bool isImage = IFileLoader::isImageFile(name);
                if (!isArchive && !isImage) {
                    continue;
                }
                QFileInfo fi(dir.absoluteFilePath(name));
                const FolderItem::FileType type =
                    isArchive ? FolderItem::Archive : FolderItem::Image;
                m_volumes << FolderItem(name, type, fi.lastModified(), fi.size());
            }
        }
        sortVolumes();
    }

    if (m_volumes.empty()) {
        m_volumes << FolderItem(tr("No folders or archives found.",
                                   "Display when there is no display item in Folder Window"),
                                FolderItem::NoItems,
                                QDateTime());
    }
    m_itemModel.setVolumes(&m_volumes);
    updateTextRowRange();
    updateCurrentVolumeRow();

    if (showParent) {
        handleViewerSessionVolumeChanged(path);
    }
}

void FolderWindow::reset()
{
    m_itemModel.setVolumes(&m_volumes);
    updateTextRowRange();
}

void FolderWindow::resortVolumes()
{
    sortVolumes();
    m_itemModel.setVolumes(&m_volumes);
    updateTextRowRange();
    updateCurrentVolumeRow();
}

void FolderWindow::updateTextRowRange()
{
    constexpr int RowMargin = 8;
    constexpr int MinimumRows = 16;
    const int rows = m_itemModel.rowCount({});
    if (rows <= 0) {
        m_itemModel.setVisibleRowRange(0, -1);
        return;
    }
    // The list asks for text images row by row, so it tells the model which
    // rows it shows: a folder with thousands of names would otherwise have the
    // helper rasterize every one of them.
    const QRect viewport = ui->folderView->viewport()->rect();
    const QModelIndex top = ui->folderView->indexAt(viewport.topLeft());
    const QModelIndex bottom = ui->folderView->indexAt(viewport.bottomLeft());
    const int first = top.isValid() ? top.row() : 0;
    const int last = bottom.isValid() ? bottom.row() : qMin(rows - 1, first + MinimumRows);
    m_itemModel.setVisibleRowRange(qMax(0, first - RowMargin), qMin(rows - 1, last + RowMargin));
}

void FolderWindow::sortVolumes()
{
    // The panel uses the viewer's image sort so that its image rows keep the
    // page order. With "show subfolders" the viewer spans subdirectories that
    // this one-level list does not contain, so the orders only match for the
    // folder's own images.
    const qvEnums::ImageSortBy sortBy = qApp->ImageSortBy();
    std::sort(
        m_volumes.begin(), m_volumes.end(), [sortBy](const FolderItem &lhs, const FolderItem &rhs) {
            return folderViewLessThan(lhs, rhs, sortBy);
        });
}

void FolderWindow::resetPathLabel(int)
{
    //    QFontMetrics fontMetrics(ui->pathLabel->font());
    //    QString pathLabelTxt = fontMetrics.elidedText(
    //                QDir::toNativeSeparators(m_currentPath), Qt::ElideMiddle, maxWidth-10);
    //    ui->pathLabel->setText(pathLabelTxt);
    ui->pathLabel->setText(m_currentPath);
}

QString FolderWindow::itemPath(const QModelIndex &index) const
{
    QDir dir(m_currentPath);
    QString filename = m_volumes[index.row()].name;
    return dir.absoluteFilePath(filename);
}

namespace {

bool sameEntryName(const QString &lhs, const QString &rhs)
{
#ifdef Q_OS_WIN
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
#else
    return lhs == rhs;
#endif
}

} // namespace

int FolderWindow::currentVolumeRow() const
{
    if (m_currentVolumePath.isEmpty() || m_currentPath.isEmpty()) {
        return -1;
    }
    // The viewer can display pages below the displayed folder when "show
    // subfolders" is on. The panel keeps its one-level list, so the entry that
    // represents the page is the file itself, or the folder containing it.
    const QString relative =
        QDir(m_currentPath).relativeFilePath(QDir::fromNativeSeparators(m_currentVolumePath));
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) ||
        relative.startsWith(QStringLiteral(".."))) {
        return -1;
    }
    const QString name = relative.section(QLatin1Char('/'), 0, 0);
    for (int row = 0; row < m_volumes.size(); ++row) {
        if (sameEntryName(m_volumes[row].name, name)) {
            return row;
        }
    }
    return -1;
}

void FolderWindow::updateCurrentVolumeRow()
{
    const int row = currentVolumeRow();
    m_itemModel.setCurrentVolumeRow(row);
    if (row < 0) {
        return;
    }
    // Mark the entry the way a file page is marked: the delegate paints the
    // model role and the current index keeps keyboard navigation on the entry.
    ui->folderView->setCurrentIndex(m_itemModel.index(row, 0));
}

void FolderWindow::keyPressEvent(QKeyEvent *event)
{
    // Enter is the one key the list leaves to the panel: it opens the entry the
    // list has as current. Every other key belongs to the window.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        handleCurrentFolderItemTriggered();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FolderWindow::handleHomeButtonClicked()
{
    const QString path = qApp->HomeFolderPath();
    setFolderPath(path, false);
    emit openVolume(OpenTarget::container(path));
}

void FolderWindow::handleParentButtonClicked()
{
    if (m_currentPath.isEmpty()) {
        return;
    }
    QDir dir(m_currentPath);
    if (!dir.cdUp()) {
        setFolderPath("", false);
        emit openVolume(OpenTarget::container(QString()));
        return;
    }
    const QString parentPath = dir.absolutePath();
    setFolderPath(parentPath, false);
    emit openVolume(OpenTarget::container(parentPath));
}

void FolderWindow::handleReloadButtonClicked()
{
    if (m_currentPath.isEmpty()) {
        return;
    }
    setFolderPath(m_currentPath, false);
    emit reloadRequested(m_currentPath);
}

void FolderWindow::handleViewerSessionVolumeChanged(QString path)
{
    m_currentVolumePath =
        path.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(path));
    updateCurrentVolumeRow();
}

void FolderWindow::openFolderItem(const QModelIndex &index)
{
    const int row = index.row();
    if (!index.isValid() || row < 0 || row >= m_volumes.size()) {
        return;
    }

    const FolderItem &item = m_volumes[row];
    if (item.type == FolderItem::NoItems) {
        return;
    }

    const QString subpath = itemPath(index);
    emit openVolume(item.type == FolderItem::Image ? OpenTarget::fileInContainer(subpath)
                                                   : OpenTarget::container(subpath));
}

void FolderWindow::handleFolderViewItemSelected(const QModelIndex &index)
{
    openFolderItem(index);
}

void FolderWindow::handleCurrentFolderItemTriggered()
{
    openFolderItem(ui->folderView->currentIndex());
}

void FolderWindow::closeEvent(QCloseEvent *e)
{
    QWidget::closeEvent(e);
    emit closed();
}
