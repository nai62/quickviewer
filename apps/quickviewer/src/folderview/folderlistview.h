#ifndef FOLDERLISTVIEW_H
#define FOLDERLISTVIEW_H

#include <QtWidgets>

/**
 * The one-entry-per-row list the folder panel shows.
 *
 * It keeps three kinds of input to itself: the left button opens the entry it
 * is clicked on, the right button chooses the entry its menu is for, and the
 * wheel scrolls it. Every other mouse button and every key, Enter included, is
 * left to the window, which maps them to the same actions they have outside the
 * panel.
 */
class FolderListView : public QListView
{
    Q_OBJECT
public:
    explicit FolderListView(QWidget *parent);

signals:
    /** The user asked for the entry at \a index to be shown. */
    void openRequested(const QModelIndex &index);
    /** A mouse button the list does not use, for the window to map. */
    void unusedMouseButton(Qt::MouseButtons buttons);
    /**
     * A menu was asked for. \a index is the entry the menu is for, which the
     * list reports from where the request points, or from the entry it has as
     * current when the request comes from the keyboard, and is invalid when the
     * request is for no entry at all. \a pos is where the menu belongs, in
     * global coordinates.
     */
    void contextMenuRequested(const QModelIndex &index, const QPoint &pos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif // FOLDERLISTVIEW_H
