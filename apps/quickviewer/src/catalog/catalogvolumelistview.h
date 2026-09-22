#ifndef CATALOGVOLUMELISTVIEW_H
#define CATALOGVOLUMELISTVIEW_H

#include <QListView>

/**
 * The list the catalog panel shows its books in.
 *
 * It keeps wheel input to itself: QAbstractItemView may ignore a wheel event
 * at the scroll boundary, which would let the event reach the window and turn
 * an image page while the pointer is over the list. FolderListView does the
 * same for the folder panel.
 */
class CatalogVolumeListView : public QListView
{
    Q_OBJECT
public:
    explicit CatalogVolumeListView(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
};

#endif // CATALOGVOLUMELISTVIEW_H
