#ifndef VOLUMEITEMMODEL_H
#define VOLUMEITEMMODEL_H

#include <QtWidgets>
#include "catalogdatabase.h"
#include "qvenums.h"

class VolumeItemModel : public QAbstractItemModel
{
public:
    VolumeItemModel(QObject *parent);
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &) const override { return 1; }
    QModelIndex
    index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override { return QModelIndex(); }

    void setVolumes(QList<VolumeThumbRecord *> *volumes);
    void setViewMode(qvEnums::CatalogViewMode viewMode);
    /**
     * The box a cover is fitted and centred in. A stored cover is as wide as
     * the view shows it but as tall as the page it came from, so the view has
     * to say which shape a cell has.
     */
    void setCoverBox(const QSize &size) { m_coverBox = size; }

private:
    QPixmap coverPixmap(const VolumeThumbRecord &record) const;

    QList<VolumeThumbRecord *> *m_volumeSearch;
    qvEnums::CatalogViewMode m_catalogViewMode;
    QSize m_coverBox;
};

#endif // VOLUMEITEMMODEL_H
