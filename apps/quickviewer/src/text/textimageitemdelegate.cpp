#include "textimageitemdelegate.h"
#include "fallbacktext.h"

namespace {
/** The room the helper leaves around the text it rasterizes, on each side. */
constexpr int ImageMargin = 2;

QPalette::ColorGroup colorGroup(const QStyleOptionViewItem &option)
{
    if (!(option.state & QStyle::State_Enabled)) {
        return QPalette::Disabled;
    }
    return (option.state & QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
}

/**
 * Draws one line of text from \a image into \a textRect, aligned the way the
 * cell aligns its text. A line wider than the cell is cut at the cell's end,
 * where an ellipsis says that more of it follows.
 */
void drawLineImage(QPainter *painter,
                   const QStyleOptionViewItem &option,
                   const QRect &textRect,
                   const QImage &image,
                   const QColor &color)
{
    const QSizeF size = image.deviceIndependentSize();
    const QSize content(qCeil(size.width()) - 2 * ImageMargin,
                        qCeil(size.height()) - 2 * ImageMargin);
    const bool rtl = option.direction == Qt::RightToLeft;
    painter->save();
    painter->setClipRect(textRect, Qt::IntersectClip);
    if (content.width() <= textRect.width()) {
        const QRect target = QStyle::alignedRect(
            option.direction,
            (option.displayAlignment & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter,
            content,
            textRect);
        painter->drawImage(QPointF(target.topLeft()) - QPointF(ImageMargin, ImageMargin), image);
        painter->restore();
        return;
    }
    // The font the cell is drawn in has every character of the ellipsis it
    // picks, so drawing it loads no fallback font.
    const QRawFont normal = FallbackText::primaryFont(option.font, false);
    const QRawFont bold = FallbackText::primaryFont(option.font, true);
    const QString ellipsis = FallbackText::supportsGlyph(normal, bold, 0x2026)
                                 ? QString(QChar(0x2026))
                                 : QStringLiteral("...");
    const QFontMetrics metrics(option.font);
    const int endWidth = metrics.horizontalAdvance(ellipsis);
    QRect imageRect = textRect;
    if (rtl) {
        imageRect.setLeft(textRect.left() + endWidth);
    } else {
        imageRect.setRight(textRect.right() - endWidth);
    }
    painter->setClipRect(imageRect, Qt::IntersectClip);
    const qreal y = textRect.center().y() - size.height() / 2;
    const qreal x =
        rtl ? textRect.right() + 1 - content.width() - ImageMargin : textRect.left() - ImageMargin;
    painter->drawImage(QPointF(x, y), image);
    painter->setClipping(false);
    painter->setClipRect(textRect);
    painter->setFont(option.font);
    painter->setPen(color);
    const QRect endRect(rtl ? textRect.left() : textRect.right() - endWidth + 1,
                        textRect.top(),
                        endWidth,
                        textRect.height());
    painter->drawText(endRect, Qt::AlignVCenter | Qt::AlignLeft, ellipsis);
    painter->restore();
}
} // namespace

TextImageItemDelegate::TextImageItemDelegate(QAbstractItemView *view, TextImageCache *cache)
    : QStyledItemDelegate(view),
      m_view(view),
      m_cache(cache ? cache : TextImageCache::instance())
{
    connect(m_cache,
            &TextImageCache::finished,
            this,
            [this](const QByteArray &key, const TextImageResult &) { handleTextReady(key); });
}

void TextImageItemDelegate::paint(QPainter *painter,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QString text = opt.text;
    const QString safeText = (opt.features & QStyleOptionViewItem::HasDisplay)
                                 ? FallbackText::placeholderFor(text, opt.font)
                                 : QString();
    if (safeText.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    // The style lays the cell out around the placeholder, which the font draws
    // whole, and never sees the text that would load a fallback font.
    opt.text = safeText;
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    const bool selected = opt.state & QStyle::State_Selected;
    const QColor color =
        opt.palette.color(colorGroup(opt), selected ? QPalette::HighlightedText : QPalette::Text);

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, widget) + 1;
    textRect.adjust(margin, 0, -margin, 0);
    const bool wrapped = opt.features & QStyleOptionViewItem::WrapText;
    QRect box = textRect;
    TextImageLayout layout;
    if (wrapped) {
        // Under an icon the text may take the rest of the cell: the real text
        // can need a line more than its placeholder.
        const bool underIcon = opt.decorationPosition == QStyleOptionViewItem::Top;
        if (underIcon) {
            box.setBottom(opt.rect.bottom());
        }
        layout.width = qMax(1, box.width());
        layout.height = qMax(1, box.height());
        layout.flags =
            (QStyle::visualAlignment(opt.direction, opt.displayAlignment) &
             Qt::AlignHorizontal_Mask) |
            (underIcon ? int(Qt::AlignTop) : int(opt.displayAlignment & Qt::AlignVertical_Mask)) |
            Qt::TextWrapAnywhere;
        layout.elideMode = opt.textElideMode;
    }
    const qreal ratio =
        widget ? widget->devicePixelRatioF() : painter->device()->devicePixelRatioF();
    const QByteArray key = TextImageCache::key(text, opt.font, ratio, layout);
    const TextImageResult result = m_cache->lookup(key);
    if (!result) {
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        want(key);
        return;
    }
    // The cell keeps the layout of its placeholder, drawn in no colour, and
    // the helper's image takes the place of that text.
    for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        opt.palette.setColor(group, QPalette::Text, Qt::transparent);
        opt.palette.setColor(group, QPalette::HighlightedText, Qt::transparent);
    }
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    const QImage image = tintedTextMask(result->images.at(opt.font.bold() ? 1 : 0), color);
    if (wrapped) {
        painter->save();
        painter->setClipRect(opt.rect, Qt::IntersectClip);
        painter->drawImage(QPointF(box.topLeft()) - QPointF(ImageMargin, ImageMargin), image);
        painter->restore();
    } else {
        drawLineImage(painter, opt, textRect, image, color);
    }
}

QSize TextImageItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    const QVariant hint = index.data(Qt::SizeHintRole);
    if (hint.isValid()) {
        return hint.toSize();
    }
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QString safeText = (opt.features & QStyleOptionViewItem::HasDisplay)
                                 ? FallbackText::placeholderFor(opt.text, opt.font)
                                 : QString();
    if (safeText.isEmpty()) {
        return QStyledItemDelegate::sizeHint(option, index);
    }
    opt.text = safeText;
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    return style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), widget);
}

bool TextImageItemDelegate::textImagesPending() const
{
    for (const QByteArray &key : m_asked) {
        if (m_cache->pending(key)) {
            return true;
        }
    }
    return false;
}

/**
 * Remembers a key the paint met without an image. Starting the helper from
 * inside the paint would hold up the very frame the placeholder is for, so the
 * keys are asked for once the paint has returned.
 */
void TextImageItemDelegate::want(const QByteArray &key) const
{
    m_wanted.insert(key);
    if (m_requestQueued) {
        return;
    }
    m_requestQueued = true;
    auto *self = const_cast<TextImageItemDelegate *>(this);
    QTimer::singleShot(0, self, &TextImageItemDelegate::requestWanted);
}

void TextImageItemDelegate::requestWanted()
{
    m_requestQueued = false;
    const QSet<QByteArray> wanted = std::exchange(m_wanted, {});
    for (const QByteArray &key : wanted) {
        // The cache turns a request away while it holds as many as it accepts
        // at once, or once a key is out of attempts; the next paint asks again.
        m_cache->request(key);
        if (m_cache->pending(key)) {
            m_asked.insert(key);
        } else if (!m_cache->lookup(key)) {
            m_turnedAway = true;
        }
    }
}

void TextImageItemDelegate::handleTextReady(const QByteArray &key)
{
    const bool ours = m_asked.remove(key);
    if (!m_view) {
        return;
    }
    if (ours || std::exchange(m_turnedAway, false)) {
        // The answer paints the cell, and a place in the cache's queue may be
        // free for a cell the cache turned away: the next paint asks again.
        m_view->viewport()->update();
    }
}
