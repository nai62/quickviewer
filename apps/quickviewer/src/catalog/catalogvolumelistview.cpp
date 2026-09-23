#include "catalogvolumelistview.h"

CatalogVolumeListView::CatalogVolumeListView(QWidget *parent)
    : QListView(parent)
{
}

void CatalogVolumeListView::wheelEvent(QWheelEvent *event)
{
    QListView::wheelEvent(event);
    // The list owns wheel input whenever the pointer is over it, even when it
    // cannot scroll.
    event->accept();
}
