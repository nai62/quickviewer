#ifndef FOLDERTREEVIEW_H
#define FOLDERTREEVIEW_H

#include <QtWidgets>

class FolderTreeView : public QTreeView
{
    Q_OBJECT
public:
    FolderTreeView(QWidget *parent);

signals:
    void selected(const QModelIndex &index);
    /**
     * A mouse button the list does not use. The window maps those to actions,
     * like the keys the list does not use.
     */
    void unusedMouseButton(Qt::MouseButtons buttons);

protected:
    void selectionChanged(const QItemSelection &selected,
                          const QItemSelection &deselected) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
};

#endif // FOLDERTREEVIEW_H
