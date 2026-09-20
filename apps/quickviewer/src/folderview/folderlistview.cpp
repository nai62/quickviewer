#include "folderlistview.h"

FolderListView::FolderListView(QWidget *parent)
    : QListView(parent)
{
}

void FolderListView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    switch (event->button()) {
    case Qt::LeftButton:
        QListView::mousePressEvent(event);
        if (index.isValid()) {
            // The list opens the row it is clicked on. Opening on the press, and
            // not on a selection change, keeps the row the panel already marks
            // openable: a click on it changes no selection.
            emit openRequested(index);
        }
        return;
    case Qt::RightButton:
        // A right click chooses the row its menu is for; it never opens it.
        QListView::mousePressEvent(event);
        event->accept();
        return;
    default:
        break;
    }
    // The remaining buttons belong to the window.
    emit unusedMouseButton(event->buttons() & ~(Qt::LeftButton | Qt::RightButton));
    event->accept();
}

void FolderListView::keyPressEvent(QKeyEvent *event)
{
    // No key belongs to the list: the window maps every one of them, exactly
    // like the keys that reach it from the viewer.
    event->ignore();
}

bool FolderListView::event(QEvent *event)
{
    // The window maps the keys, so the list must not claim one for its own
    // navigation: the arrow, page, home, end and space shortcuts step the
    // viewer's pages and volumes even while the list has focus.
    if (event->type() == QEvent::ShortcutOverride) {
        event->ignore();
        return true;
    }
    return QListView::event(event);
}

void FolderListView::wheelEvent(QWheelEvent *event)
{
    QListView::wheelEvent(event);
    // QAbstractItemView may ignore a wheel event at the scroll boundary, which
    // lets it reach MainWindow and turn an image page. FolderView owns wheel
    // input whenever the pointer is over it, even when it cannot scroll.
    event->accept();
}

void FolderListView::contextMenuEvent(QContextMenuEvent *event)
{
    // The mouse asks about the row under the pointer. The keyboard has no
    // pointer over a row, so it asks about the row the list has as current, and
    // the menu opens at that row rather than wherever the request was aimed.
    const bool fromMouse = event->reason() == QContextMenuEvent::Mouse;
    const QModelIndex index = fromMouse ? indexAt(event->pos()) : currentIndex();
    const QPoint pos = !fromMouse && index.isValid()
                           ? viewport()->mapToGlobal(visualRect(index).bottomLeft())
                           : event->globalPos();
    emit contextMenuRequested(index, pos);
    event->accept();
}
