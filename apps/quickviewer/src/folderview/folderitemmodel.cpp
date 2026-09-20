#include "folderitemmodel.h"
#include "startupprofiler.h"

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <shellapi.h>
#endif

namespace {
#ifdef Q_OS_WIN
QImage shellIconImage(const wchar_t *path, DWORD attributes)
{
    SHFILEINFOW info = {};
    if (!SHGetFileInfoW(path,
                        attributes,
                        &info,
                        sizeof(info),
                        SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_USEFILEATTRIBUTES)) {
        return QImage();
    }
    const QImage image = QImage::fromHICON(info.hIcon);
    DestroyIcon(info.hIcon);
    return image;
}

FolderIconImages loadShellIcons()
{
    // The shell asks for an initialized COM apartment on the calling thread.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    FolderIconImages images;
    images.folder = shellIconImage(L"C:\\folder", FILE_ATTRIBUTE_DIRECTORY);
    images.archive = shellIconImage(L"archive.zip", FILE_ATTRIBUTE_NORMAL);
    images.image = shellIconImage(L"image.png", FILE_ATTRIBUTE_NORMAL);
    if (SUCCEEDED(com)) {
        CoUninitialize();
    }
    return images;
}

QFuture<FolderIconImages> &iconLoadFuture()
{
    static QFuture<FolderIconImages> future;
    if (!future.isValid()) {
        future = QtConcurrent::run(loadShellIcons);
    }
    return future;
}
#endif
} // namespace

void FolderItemModel::startIconLoad()
{
#ifdef Q_OS_WIN
    (void)iconLoadFuture();
#endif
}

FolderItemModel::FolderItemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_searchedVolumes(nullptr),
      m_currentVolumeRow(-1)
{
#ifdef Q_OS_WIN
    // The shell renders the icons on another thread; this thread only turns the
    // finished images into pixmaps. Until they arrive the list draws without
    // them, which is why the load starts while the window is still being built.
    connect(&m_iconWatcher,
            &QFutureWatcher<FolderIconImages>::finished,
            this,
            &FolderItemModel::handleIconLoadFinished);
    const QFuture<FolderIconImages> &future = iconLoadFuture();
    if (future.isFinished()) {
        applyIconImages(future.result());
    } else {
        m_iconWatcher.setFuture(future);
    }
#else
    loadIconsFromProvider();
#endif
}

void FolderItemModel::handleIconLoadFinished()
{
    applyIconImages(m_iconWatcher.future().result());
}

void FolderItemModel::applyIconImages(const FolderIconImages &images)
{
    StartupProfiler::mark("folder-item-icons.begin");
    if (images.folder.isNull() || images.archive.isNull() || images.image.isNull()) {
        // The shell did not answer for every icon (another platform, or a
        // refusal): fall back to the provider, which is what this used to do.
        loadIconsFromProvider();
        return;
    }
    m_folderIcon = QIcon(QPixmap::fromImage(images.folder));
    m_archiveIcon = QIcon(QPixmap::fromImage(images.archive));
    m_imageIcon = QIcon(QPixmap::fromImage(images.image));
    StartupProfiler::mark("folder-item-icons.end");
    if (rowCount(QModelIndex()) > 0) {
        emit dataChanged(index(0, 0), index(rowCount(QModelIndex()) - 1, 0), {Qt::DecorationRole});
    }
}

void FolderItemModel::loadIconsFromProvider()
{
    QFileIconProvider iconProvider;
    m_folderIcon = iconProvider.icon(QFileIconProvider::Folder);
    m_archiveIcon = iconProvider.icon(QFileInfo(QStringLiteral("archive.zip")));
    m_imageIcon = iconProvider.icon(QFileInfo(QStringLiteral("image.png")));

    const QIcon fileIcon = iconProvider.icon(QFileIconProvider::File);
    if (m_folderIcon.isNull()) {
        m_folderIcon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    }
    if (m_archiveIcon.isNull()) {
        m_archiveIcon = fileIcon;
    }
    if (m_imageIcon.isNull()) {
        m_imageIcon = fileIcon;
    }
    StartupProfiler::mark("folder-item-icons.end");
    if (rowCount(QModelIndex()) > 0) {
        emit dataChanged(index(0, 0), index(rowCount(QModelIndex()) - 1, 0), {Qt::DecorationRole});
    }
}

QVariant FolderItemModel::data(const QModelIndex &index, int role) const
{
    if (!m_searchedVolumes) {
        return QVariant();
    }
    const int row = index.row();
    const FolderItem &fi = m_searchedVolumes->at(row);
    switch (role) {
    case Qt::DisplayRole:
        return fi.name;
    case Qt::DecorationRole:
        switch (fi.type) {
        case FolderItem::Dir:
            return m_folderIcon;
        case FolderItem::Archive:
            return m_archiveIcon;
        case FolderItem::Image:
            return m_imageIcon;
        case FolderItem::NoItems:
            break;
        }
        break;
    case CurrentVolumeRole:
        return row == m_currentVolumeRow;
    }
    return QVariant();
}

int FolderItemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return !m_searchedVolumes ? 0 : m_searchedVolumes->size();
}

int FolderItemModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QModelIndex FolderItemModel::index(int row, int column, const QModelIndex &) const
{
    if (!m_searchedVolumes) {
        return QModelIndex();
    }
    return row < m_searchedVolumes->size()
               ? createIndex(row, column, (void *)&m_searchedVolumes->at(row))
               : QModelIndex();
}

QModelIndex FolderItemModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

void FolderItemModel::setVolumes(QList<FolderItem> *volumes)
{
    if (!volumes) {
        return;
    }
    emit beginResetModel();
    m_searchedVolumes = volumes;
    emit endResetModel();
}

void FolderItemModel::setCurrentVolumeRow(int row)
{
    if (!m_searchedVolumes || row < 0 || row >= m_searchedVolumes->size()) {
        row = -1;
    }
    if (row == m_currentVolumeRow) {
        return;
    }

    const int previousRow = m_currentVolumeRow;
    m_currentVolumeRow = row;
    if (previousRow >= 0) {
        emit dataChanged(index(previousRow, 0), index(previousRow, 0), {CurrentVolumeRole});
    }
    if (m_currentVolumeRow >= 0) {
        emit dataChanged(
            index(m_currentVolumeRow, 0), index(m_currentVolumeRow, 0), {CurrentVolumeRole});
    }
}
