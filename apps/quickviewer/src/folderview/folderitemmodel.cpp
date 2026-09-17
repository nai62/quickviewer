#include "folderitemmodel.h"

namespace {
QIcon folderItemIcon(QvFolderItem::FileType type)
{
    QStyle *style = QApplication::style();
    switch (type) {
    case QvFolderItem::Dir:
        return QIcon::fromTheme(QStringLiteral("folder"), style->standardIcon(QStyle::SP_DirIcon));
    case QvFolderItem::Archive:
        return QIcon::fromTheme(QStringLiteral("package-x-generic"), style->standardIcon(QStyle::SP_DriveHDIcon));
    case QvFolderItem::Image:
        return QIcon::fromTheme(QStringLiteral("image-x-generic"), style->standardIcon(QStyle::SP_FileIcon));
    case QvFolderItem::NoItems:
        break;
    }
    return QIcon();
}
}

FolderItemModel::FolderItemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_searchedVolumes(nullptr),
      m_columns(1)
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
            return folderItemIcon(fi.type);
        }
        break;
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
