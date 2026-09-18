#include "folderitemmodel.h"
#include "startupprofiler.h"

FolderItemModel::FolderItemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_searchedVolumes(nullptr),
      m_columns(1),
      m_currentVolumeRow(-1)
{
    StartupProfiler::mark("folder-item-icons.begin");
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
}

QVariant FolderItemModel::headerData(int section, Qt::Orientation, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        switch (section) {
        case 0:
            return tr("Name", "Title of the column in the folder list when displaying as an independent Window in Folder Window");
        case 1:
            return tr("Modified", "Title of the column in the folder list when displaying as an independent Window in Folder Window");
        }
        break;
    }
    return QVariant();
}

QVariant FolderItemModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    int column = index.column();
    if (!m_searchedVolumes) {
        return QVariant();
    }
    const FolderItem &fi = m_searchedVolumes->at(row);
    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case 0:
            return fi.name;
        case 1:
            return fi.updated_at;
        }
        break;
    case Qt::DecorationRole:
        if (column == 0) {
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
    return m_columns;
}

QModelIndex FolderItemModel::index(int row, int column, const QModelIndex &) const
{
    if (!m_searchedVolumes) {
        return QModelIndex();
    }
    return row < m_searchedVolumes->size() ? createIndex(row, column, (void *)&m_searchedVolumes->at(row)) : QModelIndex();
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
        emit dataChanged(
            index(previousRow, 0),
            index(previousRow, m_columns - 1),
            {CurrentVolumeRole});
    }
    if (m_currentVolumeRow >= 0) {
        emit dataChanged(
            index(m_currentVolumeRow, 0),
            index(m_currentVolumeRow, m_columns - 1),
            {CurrentVolumeRole});
    }
}
