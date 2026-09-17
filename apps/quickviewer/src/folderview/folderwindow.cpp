#include <QtWidgets>

#include "ui_folderwindow.h"
#include "ui_mainwindow.h"

#include "folderwindow.h"
#include "models/volume.h"
#include "models/qvapplication.h"

FolderWindow::FolderWindow(QWidget *parent, Ui::MainWindow *uiMain)
    : QWidget(parent),
      ui(new Ui::FolderWindow),
      m_sortModeMenu(nullptr),
      m_itemContextMenu(nullptr),
      m_historyButton(nullptr),
      m_itemModel(this),
      m_itemDelegate(parent, this)
{
    ui->setupUi(this);

#ifdef Q_OS_MACOS
    ui->menuBar->setNativeMenuBar(false);
#endif

    ui->folderView->setRootIsDecorated(false);
    ui->folderView->setIndentation(0);
    ui->folderView->setMouseTracking(true);
    ui->folderView->installEventFilter(this);

    // folderView
    ui->folderView->setModel(&m_itemModel);
    ui->folderView->setItemDelegate(&m_itemDelegate);

    // menus
    ui->menuBar->removeAction(ui->menuSort->menuAction());
    m_sortModeMenu = ui->menuSort;
    ui->menuBar->removeAction(ui->menuItemContext->menuAction());
    m_itemContextMenu = ui->menuItemContext;

    setupHistoryButton(uiMain);

    QFont sortFont = ui->sortModeButton->font();
    sortFont.setBold(false);
    sortFont.setUnderline(false);
    ui->sortModeButton->setFont(sortFont);
    ui->horizontalLayout->removeWidget(ui->sortModeButton);
    ui->horizontalLayout->addWidget(m_historyButton);
    ui->horizontalLayout->addWidget(ui->sortModeButton);

    resetSortMode();

    connect(qApp->languageSelector(), &LanguageManager::languageChanged, this, [this](const QString &) {
        ui->retranslateUi(this);
        m_historyButton->setToolTip(tr("Open history"));
        resetSortMode();
        if (m_volumes.size() == 1 && m_volumes.first().type == QvFolderItem::NoItems) {
            m_volumes.first().name = tr("No folders or archives found.", "Display when there is no display item in Folder Window");
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
    if (m_sortModeMenu) {
        delete m_sortModeMenu;
    }
    if (m_itemContextMenu) {
        delete m_itemContextMenu;
    }
    delete ui;
}

void FolderWindow::setupHistoryButton(Ui::MainWindow *uiMain)
{
    m_historyButton = new QToolButton(ui->frame);
    m_historyButton->setObjectName(QStringLiteral("historyButton"));
    m_historyButton->setToolTip(tr("Open history"));
    m_historyButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_historyButton->setPopupMode(QToolButton::InstantPopup);
    m_historyButton->setAutoRaise(true);
    m_historyButton->setMaximumSize(24, 24);
    m_historyButton->setIconSize(QSize(24, 24));
    m_historyButton->setIcon(QIcon(QStringLiteral(":/icons/24/3dot_icon_24")));

    if (uiMain) {
        m_historyButton->setMenu(uiMain->menuHistory);
    } else {
        m_historyButton->setEnabled(false);
    }
}

void FolderWindow::setAsToplevelWindow()
{
    ui->menuBar->setVisible(true);
    ui->folderView->header()->setVisible(true);
    QRect rect = geometry();
    ui->folderView->setColumnWidth(0, rect.width() - 150);
    ui->folderView->setColumnWidth(1, 150);
    m_itemModel.setColumns(2);
}

void FolderWindow::setAsInnerWidget()
{
    ui->menuBar->setVisible(false);
    ui->folderView->header()->setVisible(false);
    m_itemModel.setColumns(1);
}

static QModelIndex selectedIdx;

bool FolderWindow::eventFilter(QObject *obj, QEvent *event)
{
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
    const QvFolderItem &item = m_volumes[row];
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
    resetPathLabel(event->size().width());
}

static bool filenameLessThan(const QvFolderItem &lhs, const QvFolderItem &rhs)
{
    if (lhs.type != rhs.type) {
        return lhs.type < rhs.type;
    }
    return IFileLoader::caseInsensitiveLessThan(lhs.name, rhs.name);
}

static bool updatedAtGreaterThan(const QvFolderItem &lhs, const QvFolderItem &rhs)
{
    if (lhs.updated_at != rhs.updated_at) {
        return lhs.updated_at > rhs.updated_at;
    }
    if (lhs.type != rhs.type) {
        return lhs.type < rhs.type;
    }
    return IFileLoader::caseInsensitiveLessThan(lhs.name, rhs.name);
}

//IFileLoader::caseInsensitiveLessThan

void FolderWindow::setFolderPath(QString path, bool showParent)
{
#ifdef Q_OS_WIN
    if (path.isEmpty()) {
        m_volumes.clear();
        {
            m_currentPath = "";
            QList<QFileInfo> drives = QDir::drives();
            foreach (QFileInfo drive, drives) {
                m_volumes << QvFolderItem(drive.absoluteFilePath(), QvFolderItem::Dir, drive.lastModified());
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
            QStringList subfolders = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::Unsorted);
            foreach (const QString &sf, subfolders) {
                QFileInfo fi(dir.absoluteFilePath(sf));
                m_volumes << QvFolderItem(sf, QvFolderItem::Dir, fi.lastModified());
            }
        }

        {
            foreach (const QString name, dir.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::Unsorted)) {
                const bool isArchive = IFileLoader::isArchiveFile(name);
                const bool isImage = IFileLoader::isImageFile(name);
                if (!isArchive && !isImage) {
                    continue;
                }
                QFileInfo fi(dir.absoluteFilePath(name));
                const QvFolderItem::FileType type = isArchive ? QvFolderItem::Archive : QvFolderItem::Image;
                m_volumes << QvFolderItem(name, type, fi.lastModified());
            }
        }
        qvEnums::FolderViewSort sortmode = qApp->FolderSortMode();
        if (sortmode == qvEnums::OrderByName) {
            std::sort(m_volumes.begin(), m_volumes.end(), filenameLessThan);
        } else {
            std::sort(m_volumes.begin(), m_volumes.end(), updatedAtGreaterThan);
        }
    }

    if (m_volumes.empty()) {
        m_volumes << QvFolderItem(tr("No folders or archives found.", "Display when there is no display item in Folder Window"), QvFolderItem::NoItems, QDateTime());
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

void FolderWindow::resetSortMode()
{
    ui->actionOrderByName->setText(tr("Name"));
    ui->actionOrderByUpdatedAt->setText(tr("Modified"));

    qvEnums::FolderViewSort sortMode = qApp->FolderSortMode();
    ui->actionOrderByName->setChecked(sortMode == qvEnums::OrderByName);
    ui->actionOrderByUpdatedAt->setChecked(sortMode == qvEnums::OrderByUpdatedAt);
    ui->sortModeButton->setText((sortMode == qvEnums::OrderByName
                                     ? ui->actionOrderByName->text()
                                     : ui->actionOrderByUpdatedAt->text()) +
                                QStringLiteral(" ▾"));
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

void FolderWindow::updateCurrentVolumeRow()
{
    int currentVolumeRow = -1;
    for (int row = 0; row < m_volumes.size() && !m_currentVolumePath.isEmpty(); ++row) {
        const QString item = QDir::cleanPath(QDir::fromNativeSeparators(itemPath(m_itemModel.index(row, 0))));
#ifdef Q_OS_WIN
        const bool isCurrentVolume = item.compare(m_currentVolumePath, Qt::CaseInsensitive) == 0;
#else
        const bool isCurrentVolume = item == m_currentVolumePath;
#endif
        if (isCurrentVolume) {
            currentVolumeRow = row;
            break;
        }
    }
    m_itemModel.setCurrentVolumeRow(currentVolumeRow);
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
}

void FolderWindow::handleHomeButtonClicked()
{
    const QString path = qApp->HomeFolderPath();
    setFolderPath(path, false);
    emit openVolume(path);
}

void FolderWindow::handleParentButtonClicked()
{
    if (m_currentPath.isEmpty()) {
        return;
    }
    QDir dir(m_currentPath);
    if (!dir.cdUp()) {
        setFolderPath("", false);
        emit openVolume("");
        return;
    }
    const QString parentPath = dir.absolutePath();
    setFolderPath(parentPath, false);
    emit openVolume(parentPath);
}

void FolderWindow::handleReloadButtonClicked()
{
    if (m_currentPath.isEmpty()) {
        return;
    }
    setFolderPath(m_currentPath, false);
}

void FolderWindow::handleViewerSessionVolumeChanged(QString path)
{
    m_currentVolumePath = path.isEmpty()
                              ? QString()
                              : QDir::cleanPath(QDir::fromNativeSeparators(path));
    updateCurrentVolumeRow();

    QFileInfo info(QDir::toNativeSeparators(path));
    if (!info.exists() || m_currentPath != info.absolutePath()) {
        return;
    }
    QString name = info.fileName();
    int row = -1;
    foreach (const QvFolderItem &item, m_volumes) {
        row++;
        if (name != item.name) {
            continue;
        }
        QModelIndex midx = m_itemModel.index(row, 0, QModelIndex());
        const QSignalBlocker blocker(ui->folderView);
        ui->folderView->setCurrentIndex(midx);

        break;
    }
}

void FolderWindow::openFolderItem(const QModelIndex &index)
{
    const int row = index.row();
    if (!index.isValid() || row < 0 || row >= m_volumes.size()) {
        return;
    }

    if (m_volumes[row].type == QvFolderItem::NoItems) {
        return;
    }

    const QString subpath = itemPath(index);
    emit openVolume(subpath);
}

void FolderWindow::handleFolderViewItemSelected(const QModelIndex &index)
{
    openFolderItem(index);
}

void FolderWindow::handleCurrentFolderItemTriggered()
{
    openFolderItem(ui->folderView->currentIndex());
}

void FolderWindow::handleSortModeButtonClicked()
{
    QWidget *widget = ui->sortModeButton;

    QPoint p = widget->mapToGlobal(QPoint(0, widget->height()));
    m_sortModeMenu->exec(p);
}

void FolderWindow::handleOrderByNameActionTriggered()
{
    qApp->setFolderSortMode(qvEnums::OrderByName);
    resetSortMode();
    handleReloadButtonClicked();
}

void FolderWindow::handleOrderByUpdatedAtActionTriggered()
{
    qApp->setFolderSortMode(qvEnums::OrderByUpdatedAt);
    resetSortMode();
    handleReloadButtonClicked();
}

void FolderWindow::closeEvent(QCloseEvent *e)
{
    QWidget::closeEvent(e);
    emit closed();
}
