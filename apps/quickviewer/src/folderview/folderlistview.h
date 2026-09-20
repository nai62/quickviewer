#ifndef FOLDERLISTVIEW_H
#define FOLDERLISTVIEW_H

#include <QtWidgets>

/**
 * The one-entry-per-row list the folder panel shows.
 *
 * It keeps three kinds of input to itself: the left button opens the entry it
 * is clicked on, the right button chooses the entry its menu is for, and the
 * wheel scrolls it. Everything else - the other mouse buttons and every key,
 * Enter included - is left to the window, which maps keys and mouse buttons to
 * actions the same way it does outside the panel.
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

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
};

#endif // FOLDERLISTVIEW_H
