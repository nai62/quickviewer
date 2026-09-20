#ifndef FOLDERITEMDELEGATE_H
#define FOLDERITEMDELEGATE_H

#include <QtWidgets>
#include <QtCore>

class FolderItemDelegate : public QStyledItemDelegate
{
public:
    explicit FolderItemDelegate(QWidget *parent);
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // FOLDERITEMDELEGATE_H
