#ifndef FOLDERITEMMODEL_H
#define FOLDERITEMMODEL_H

#include <QtWidgets>
#include <QtCore>

#include "folderitem.h"

class FolderItemModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ItemRole {
        CurrentVolumeRole = Qt::UserRole
    };

    FolderItemModel(QObject *parent);
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;

    void setVolumes(QList<FolderItem> *volumes);
    void setCurrentVolumeRow(int row);

private:
    QList<FolderItem> *m_searchedVolumes;
    int m_currentVolumeRow;
    QIcon m_folderIcon;
    QIcon m_archiveIcon;
    QIcon m_imageIcon;
};

#endif // FOLDERITEMMODEL_H
