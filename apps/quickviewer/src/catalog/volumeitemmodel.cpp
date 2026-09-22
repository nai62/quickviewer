#include "volumeitemmodel.h"
#include "fileloader.h"
#include "qvapplication.h"

VolumeItemModel::VolumeItemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_volumeSearch(nullptr),
      m_catalogViewMode(qvEnums::CatalogViewMode::Icon)
{
}

QPixmap VolumeItemModel::coverPixmap(const VolumeThumbRecord &record) const
{
    const QImage cover = QImage::fromData(record.thumbnail, IFileLoader::jpegQtFormatName());
    if (cover.isNull()) {
        // A cover that cannot be decoded is no cover: the view then shows the
        // name alone, which is what the catalog window counts as unlistable.
        return QPixmap();
    }
    if (!m_coverBox.isValid()) {
        return QPixmap::fromImage(cover);
    }
    // The stored cover is as wide as the view shows it, but as tall as the page
    // it came from, and the item view pins a decoration to the top of its cell.
    // Draw the scaled cover into a filled box, so the room above and below it is
    // the same whatever shape the page has.
    QPixmap box(m_coverBox);
    box.fill(Qt::transparent);
    const QPixmap scaled =
        QPixmap::fromImage(cover.scaled(m_coverBox, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QPainter painter(&box);
    painter.drawPixmap(
        (box.width() - scaled.width()) / 2, (box.height() - scaled.height()) / 2, scaled);
    return box;
}

QVariant VolumeItemModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (!m_volumeSearch) {
        return QVariant();
    }
    const VolumeThumbRecord *vtr = m_volumeSearch->at(row);
    switch (role) {
    case Qt::DisplayRole:
        if (m_catalogViewMode == qvEnums::CatalogViewMode::IconNoText) {
            return QVariant();
        }
        return qApp->TitleWithoutOptions() ? vtr->name : vtr->realname;
    case Qt::DecorationRole:
        return coverPixmap(*vtr);
    case Qt::SizeHintRole: {
        const bool iconLongText = qApp->IconLongText();
        if (m_catalogViewMode == qvEnums::CatalogViewMode::List) {
            return iconLongText ? QSize(300, 100) : QSize(200, 100);
        }
        if (m_catalogViewMode == qvEnums::CatalogViewMode::Icon) {
            return iconLongText ? QSize(150, 170) : QSize(150, 120);
        }
        return QSize(100, 100);
    }
    }
    return QVariant();
}

int VolumeItemModel::rowCount(const QModelIndex &) const
{
    return !m_volumeSearch ? 0 : m_volumeSearch->size();
}

QModelIndex VolumeItemModel::index(int row, int column, const QModelIndex &) const
{
    if (column > 1 || !m_volumeSearch) {
        return QModelIndex();
    }
    return row < m_volumeSearch->size() ? createIndex(row, column, m_volumeSearch->at(row))
                                        : QModelIndex();
}

void VolumeItemModel::setVolumes(QList<VolumeThumbRecord *> *volumes)
{
    emit beginResetModel();
    m_volumeSearch = volumes;
    emit endResetModel();
}

void VolumeItemModel::setViewMode(qvEnums::CatalogViewMode viewMode)
{
    m_catalogViewMode = viewMode;
    emit submit();
}
