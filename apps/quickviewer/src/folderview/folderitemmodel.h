#ifndef FOLDERITEMMODEL_H
#define FOLDERITEMMODEL_H

#include <QtWidgets>
#include <QtCore>

class QvFolderItem
{
public:
    enum FileType {
        Dir,
        Archive,
        Image,
        NoItems
    };

    QString name;
    FileType type; // 0:folder, 1:archive
    QDateTime updated_at;
    qint64 size;

    QvFolderItem()
        : type(Dir),
          size(0)
    {}
    QvFolderItem(QString n, FileType t, QDateTime u, qint64 s = 0)
        : name(n),
          type(t),
          updated_at(u),
          size(s)
    {}
};

class FolderItemModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ItemRole {
        CurrentVolumeRole = Qt::UserRole
    };

    FolderItemModel(QObject *parent);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;

    void setVolumes(QList<QvFolderItem> *volumes);
    void setCurrentVolumeRow(int row);
    void setColumns(int c) { m_columns = c; }

private:
    QList<QvFolderItem> *m_searchedVolumes;
    int m_columns;
    int m_currentVolumeRow;
    QIcon m_folderIcon;
    QIcon m_archiveIcon;
    QIcon m_imageIcon;
};

#endif // FOLDERITEMMODEL_H
