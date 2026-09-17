#include <QtWidgets>

#include "ui_folderwindow.h"
#include "ui_mainwindow.h"

#include "folderwindow.h"
#include "models/volume.h"
#include "models/qvapplication.h"

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

    ui->label->hide();
    ui->frame->setStyleSheet(QString());
    ui->folderView->setRootIsDecorated(false);
    ui->folderView->setIndentation(0);
    ui->folderView->setMouseTracking(true);
    ui->folderView->installEventFilter(this);

    // folderView
    ui->folderView->setModel(&m_itemModel);
    ui->folderView->setItemDelegate(&m_itemDelegate);
    QObject::disconnect(ui->folderView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(handleFolderViewItemDoubleClicked(QModelIndex)));

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
        ui->label->hide();
        m_historyButton->setText(tr("History"));
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
                                    : ui->actionOrderByUpdatedAt->text())
                                + QStringLiteral(" \u25BE"));
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

bool FolderWindow::isCurrentVolume(const QModelIndex &index) const
{
    if (!index.isValid() || m_currentVolumePath.isEmpty()) {
        return false;
    }

    const QString item = QDir::cleanPath(QDir::fromNativeSeparators(itemPath(index)));
#ifdef Q_OS_WIN
    return item.compare(m_currentVolumePath, Qt::CaseInsensitive) == 0;
#else
    return item == m_currentVolumePath;
#endif
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
    ui->folderView->viewport()->update();

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

    const QvFolderItem &item = m_volumes[row];
    if (item.type == QvFolderItem::NoItems) {
        return;
    }

    const QString subpath = itemPath(index);
    emit openVolume(subpath);

    if (item.type == QvFolderItem::Dir) {
        setFolderPath(subpath, false);
    }
}

void FolderWindow::handleFolderViewItemSelected(const QModelIndex &index)
{
    openFolderItem(index);
}

void FolderWindow::handleFolderViewItemDoubleClicked(const QModelIndex &index)
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
