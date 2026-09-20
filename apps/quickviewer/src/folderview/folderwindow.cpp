#include <QtWidgets>
#include <QCollator>

#include "ui_folderwindow.h"
#include "ui_mainwindow.h"

#include "folderwindow.h"
#include "models/volume.h"
#include "models/qvapplication.h"
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

    ui->folderView->setRootIsDecorated(false);
    ui->folderView->setIndentation(0);
    ui->folderView->setUniformRowHeights(true);
    ui->folderView->setMouseTracking(true);
    ui->folderView->installEventFilter(this);

    // folderView
    ui->folderView->setModel(&m_itemModel);
    ui->folderView->setItemDelegate(&m_itemDelegate);

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

void FolderWindow::requestListFallbackFonts()
{
    m_fallbackFontRequestPending = false;
    m_itemModel.requestFallbackFonts();
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

static QModelIndex selectedIdx;

bool FolderWindow::eventFilter(QObject *obj, QEvent *event)
{
    // The list is on screen from here on; a name that needs a fallback font is
    // shown with a placeholder and the font is loaded on a worker thread instead
    // of inside a paint of the list.
    if (event->type() == QEvent::Paint && obj == ui->folderView) {
        // Queued, so the load starts once this paint is done. A load running
        // while the list paints makes that paint wait for the font database -
        // the first row of the list took about 200 ms that way - and the list is
        // drawn with a placeholder until the load finishes anyway.
        if (!m_fallbackFontRequestPending) {
            m_fallbackFontRequestPending = true;
            QTimer::singleShot(0, this, &FolderWindow::requestListFallbackFonts);
        }
    }
    //    qDebug() << obj << event << event->type();
    //    QMouseEvent *mouseEvent = nullptr;
    QContextMenuEvent *contextEvent = nullptr;
    switch (event->type()) {
    default:
        break;
    case QEvent::ContextMenu:
        contextEvent = dynamic_cast<QContextMenuEvent *>(event);
        QPoint inner = ui->folderView->mapFromGlobal(QCursor::pos());
        selectedIdx = ui->folderView->indexAt(inner);
        m_itemContextMenu->exec(QCursor::pos());
        return true;
    }
    return QObject::eventFilter(obj, event);
}

void FolderWindow::handleSetAsHomeFolderActionTriggered()
{
    int row = selectedIdx.row();
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
    updateCurrentVolumeRow();

    if (showParent) {
        handleViewerSessionVolumeChanged(path);
    }
}

void FolderWindow::reset()
{
    m_itemModel.setVolumes(&m_volumes);
}

void FolderWindow::resortVolumes()
{
    sortVolumes();
    m_itemModel.setVolumes(&m_volumes);
    updateCurrentVolumeRow();
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
    // The signals are blocked because selecting an entry must not open it.
    const QSignalBlocker blocker(ui->folderView);
    ui->folderView->setCurrentIndex(m_itemModel.index(row, 0));
}

const static QKeySequence seqReturn("Return");
const static QKeySequence seqEnter("Num+Enter");
const static QKeySequence seqBackspace("Backspace");

void FolderWindow::keyPressEvent(QKeyEvent *event)
{
    QKeySequence seq(event->key() | event->modifiers());
    qDebug() << seq;
    if (seq == seqReturn || seq == seqEnter) {
        handleCurrentFolderItemTriggered();
        return;
    }
    if (seq == seqBackspace) {
        handleParentButtonClicked();
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
