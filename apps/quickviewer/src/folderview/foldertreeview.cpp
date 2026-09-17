#include "foldertreeview.h"

FolderTreeView::FolderTreeView(QWidget *parent)
    : QTreeView(parent)
{
}

void FolderTreeView::selectionChanged(const QItemSelection &selection, const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selection, deselected);

    const auto list = selection.indexes();
    if (!list.isEmpty()) {
        emit selected(list.first());
    }
}
