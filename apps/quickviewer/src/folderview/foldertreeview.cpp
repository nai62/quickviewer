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

void FolderTreeView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex pressed = indexAt(event->pos());
    const bool wasChosen = pressed.isValid() && pressed == currentIndex();
    QTreeView::mousePressEvent(event);
    // The panel marks the entry that holds the shown page, so pressing that row
    // changes no selection - and a selection change is what opens an entry.
    // Report the press here, or that marked row would be the one entry that
    // cannot be opened. A press that changed the selection is already reported
    // by selectionChanged().
    if (event->button() == Qt::LeftButton && wasChosen && selectionModel() &&
        selectionModel()->isSelected(pressed)) {
        emit selected(pressed);
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
