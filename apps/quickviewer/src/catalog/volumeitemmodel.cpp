#include "volumeitemmodel.h"
#include "fileloader.h"
#include "qvapplication.h"

namespace {
/** How much cover the cache keeps, in KiB: room for a few hundred covers. */
constexpr int CoverCacheKib = 8 * 1024;

/** The room a cover takes in the cache, in KiB. */
int coverCostKib(const QPixmap &cover)
{
    const qint64 bytes = qint64(cover.width()) * cover.height() * cover.depth() / 8;
    return qMax(1, int((bytes + 1023) / 1024));
}

/** The stored cover of \a record, decoded and fitted into \a box. */
QPixmap fittedCover(const VolumeThumbRecord &record, const QSize &box)
{
    const QImage cover = QImage::fromData(record.thumbnail, IFileLoader::jpegQtFormatName());
    if (cover.isNull()) {
        // A cover that cannot be decoded is no cover: the view then shows the
        // name alone, which is what the catalog window counts as unlistable.
        return QPixmap();
    }
    if (!box.isValid()) {
        return QPixmap::fromImage(cover);
    }
    // The stored cover is as wide as the view shows it, but as tall as the page
    // it came from, and the item view pins a decoration to the top of its cell.
    // Draw the scaled cover into a filled box, so the room above and below it is
    // the same whatever shape the page has.
    QPixmap fitted(box);
    fitted.fill(Qt::transparent);
    const QPixmap scaled =
        QPixmap::fromImage(cover.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QPainter painter(&fitted);
    painter.drawPixmap(
        (fitted.width() - scaled.width()) / 2, (fitted.height() - scaled.height()) / 2, scaled);
    return fitted;
}
} // namespace

VolumeItemModel::VolumeItemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_volumeSearch(nullptr),
      m_catalogViewMode(qvEnums::CatalogViewMode::Icon)
{
    m_covers.setMaxCost(CoverCacheKib);
}

QPixmap VolumeItemModel::coverPixmap(const VolumeThumbRecord &record) const
{
    // The view asks for the cover of every row it shows again on every repaint,
    // so a cover that has been read once is kept. The id names the row that
    // holds the JPEG in the database, and the database never hands the same id
    // out twice, so an entry still in the cache still belongs to this volume.
    if (record.thumb_id > 0) {
        if (const QPixmap *cached = m_covers.object(record.thumb_id)) {
            return *cached;
        }
    }
    const QPixmap cover = fittedCover(record, m_coverBox);
    if (record.thumb_id > 0 && !cover.isNull()) {
        m_covers.insert(record.thumb_id, new QPixmap(cover), coverCostKib(cover));
    }
    return cover;
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

void VolumeItemModel::setCoverBox(const QSize &size)
{
    if (m_coverBox == size) {
        return;
    }
    // Every cover in the cache was fitted into the box that is being replaced.
    m_coverBox = size;
    m_covers.clear();
}
