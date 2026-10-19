#ifndef TEXTIMAGEITEMDELEGATE_H
#define TEXTIMAGEITEMDELEGATE_H

#include <QtWidgets>
#include "textimagecache.h"

/**
 * An item delegate for views whose text comes from the user's data, such as
 * the catalog's volume titles and the catalog manager's paths.
 *
 * A cell whose text holds a character the view's font cannot draw is laid out
 * and painted with a placeholder in its place, so neither the paint nor the
 * size hint loads a fallback font on the GUI thread. The helper rasterizes the
 * text afterwards - one line, or the lines a wrapping view fits into the cell -
 * and the cell paints that image where the style lays its text out. The model
 * keeps the real text, which screen readers, searches and tooltips use.
 */
class TextImageItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TextImageItemDelegate(QAbstractItemView *view, TextImageCache *cache = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    /** True while text this delegate painted waits for the helper. */
    bool textImagesPending() const;

private:
    void want(const QByteArray &key) const;
    void requestWanted();
    void handleTextReady(const QByteArray &key);

    QPointer<QAbstractItemView> m_view;
    TextImageCache *m_cache;
    /** Keys a paint met without an image; they are asked for once it returns. */
    mutable QSet<QByteArray> m_wanted;
    mutable bool m_requestQueued = false;
    /** Keys asked for and not answered yet; an answer repaints the view. */
    QSet<QByteArray> m_asked;
    /** A paint's key the cache turned away; the next answer repaints the view. */
    bool m_turnedAway = false;
};

#endif // TEXTIMAGEITEMDELEGATE_H
