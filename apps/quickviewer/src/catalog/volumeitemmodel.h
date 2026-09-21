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

private:
    QList<VolumeThumbRecord *> *m_volumeSearch;
    qvEnums::CatalogViewMode m_catalogViewMode;
};

#endif // VOLUMEITEMMODEL_H
