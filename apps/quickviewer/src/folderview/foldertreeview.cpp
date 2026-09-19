#include "foldertreeview.h"

FolderTreeView::FolderTreeView(QWidget *parent)
    : QTreeView(parent)
{
}

void FolderTreeView::selectionChanged(const QItemSelection &selection,
                                      const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selection, deselected);

    const auto list = selection.indexes();
    if (!list.isEmpty()) {
        emit selected(list.first());
    }
}

void FolderTreeView::wheelEvent(QWheelEvent *event)
{
    QTreeView::wheelEvent(event);
    // QAbstractItemView may ignore a wheel event at the scroll boundary,
    // which lets it reach MainWindow and turn an image page. FolderView owns
    // wheel input whenever the pointer is over it, even when it cannot scroll.
    event->accept();
}
