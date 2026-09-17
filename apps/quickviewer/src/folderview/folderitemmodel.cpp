#include "folderitemmodel.h"

namespace {
QIcon themedIcon(const QString &name, QStyle::StandardPixmap fallback)
{
    QIcon icon = QIcon::fromTheme(name);
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(fallback);
    }
    return icon;
}
}

FolderItemModel::FolderItemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_searchedVolumes(nullptr),
      m_columns(1),
      m_currentVolumeRow(-1),
      m_folderIcon(themedIcon(QStringLiteral("folder"), QStyle::SP_DirIcon)),
      m_archiveIcon(themedIcon(QStringLiteral("package-x-generic"), QStyle::SP_DriveHDIcon)),
      m_imageIcon(themedIcon(QStringLiteral("image-x-generic"), QStyle::SP_FileIcon))
{
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
    const QvFolderItem &fi = m_searchedVolumes->at(row);
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
            case QvFolderItem::Dir:
                return m_folderIcon;
            case QvFolderItem::Archive:
                return m_archiveIcon;
            case QvFolderItem::Image:
                return m_imageIcon;
            case QvFolderItem::NoItems:
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

void FolderItemModel::setVolumes(QList<QvFolderItem> *volumes)
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
