#include "folderitemdelegate.h"
#include "folderitemmodel.h"
#include "qvapplication.h"

namespace {
/**
 * The helper returns white coverage masks, so one result serves every palette
 * colour. Tinting is cheap but the list repaints on every hover, so the recent
 * results are kept.
 */
QImage tintedTextMask(const QImage &mask, const QColor &color)
{
    static QCache<QPair<qint64, QRgb>, QImage> cache(2 * 1024); // KiB
    const QPair<qint64, QRgb> key(mask.cacheKey(), color.rgba());
    if (const QImage *cached = cache.object(key)) {
        return *cached;
    }
    QImage image(mask.size(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(mask.devicePixelRatio());
    image.fill(color);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawImage(QPoint(0, 0), mask);
    painter.end();
    cache.insert(key, new QImage(image), qMax(1, int(image.sizeInBytes() / 1024)));
    return image;
}
} // namespace

FolderItemDelegate::FolderItemDelegate(QWidget *parent)
    : QStyledItemDelegate(parent)
{
}
constexpr int ProgressWidth = 100;
constexpr int ProgressHeight = 10;
/** Space above and below a row's name. */
constexpr int RowTextMargin = 2;

void FolderItemDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    // DisplayRole may contain the real name for accessibility/search. Neither
    // style layout nor sizeHint may shape that name on the GUI thread.
    opt.text = index.data(FolderItemModel::SafeTextRole).toString();
    opt.font.setStyleStrategy(
        QFont::StyleStrategy(opt.font.styleStrategy() | QFont::NoFontMerging));
    opt.fontMetrics = QFontMetrics(opt.font);

    const bool isCurrentVolume = index.data(FolderItemModel::CurrentVolumeRole).toBool();
    if (isCurrentVolume) {
        opt.font.setBold(true);
        opt.fontMetrics = QFontMetrics(opt.font);
        QColor activeBackground = opt.palette.color(QPalette::Highlight);
        activeBackground.setAlpha(32);

        painter->save();
        painter->fillRect(option.rect, activeBackground);
        painter->restore();

        opt.backgroundBrush = Qt::NoBrush;
        opt.state &= ~QStyle::State_Selected;
    }

    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();

    // Keep the platform hover rendering above the active-volume background.
    const FolderTextResult text =
        index.data(FolderItemModel::TextImagesRole).value<FolderTextResult>();
    if (text && text->images.size() == FolderTextImages::ImageCount) {
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, widget) + 1;
        textRect.adjust(margin, 0, -margin, 0);
        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        const bool selected = opt.state & QStyle::State_Selected;
        const QPalette::ColorGroup group = !(opt.state & QStyle::State_Enabled) ? QPalette::Disabled
                                           : (opt.state & QStyle::State_Active)
                                               ? QPalette::Active
                                               : QPalette::Inactive;
        // The current row is drawn bold, which is the second mask.
        const QImage image = tintedTextMask(
            text->images.at(isCurrentVolume ? 1 : 0),
            opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        const QSizeF size = image.deviceIndependentSize();
        painter->save();
        painter->setClipRect(textRect, Qt::IntersectClip);
        const bool clipped = size.width() - 4 > textRect.width();
        QFontMetrics metrics(opt.font);
        const QString ellipsis = QString(QChar(0x2026));
        const int endWidth = clipped ? metrics.horizontalAdvance(ellipsis) : 0;
        const bool rtl = opt.direction == Qt::RightToLeft;
        QRect imageRect = textRect;
        if (rtl) {
            imageRect.adjust(endWidth, 0, 0, 0);
        } else {
            imageRect.adjust(0, 0, -endWidth, 0);
        }
        painter->setClipRect(imageRect, Qt::IntersectClip);
        const qreal x = rtl ? textRect.right() - size.width() + 3 : textRect.left() - 2;
        painter->drawImage(QPointF(x, textRect.center().y() - size.height() / 2), image);
        if (clipped) {
            painter->setClipping(false);
            painter->setClipRect(textRect);
            painter->setFont(opt.font);
            painter->setPen(
                opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
            const QRect endRect(rtl ? textRect.left() : textRect.right() - endWidth + 1,
                                textRect.top(),
                                endWidth,
                                textRect.height());
            painter->drawText(endRect, Qt::AlignVCenter | Qt::AlignLeft, ellipsis);
        }
        painter->restore();
    } else {
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }
    // Draw the read progress bar above both the active background and item.
    do {
        if (!qApp->ShowReadProgress() || index.column() != 0) {
            break;
        }
        const QVariant progressValue = index.data(FolderItemModel::ReadProgressRole);
        if (!progressValue.canConvert<ReadProgress>()) {
            break;
        }
        const ReadProgress progress = progressValue.value<ReadProgress>();
        QRect rect(option.rect);
        QPoint begin(rect.left() + 30, rect.top() + ProgressHeight);
        //        painter->drawLine(begin, QPoint(begin.x()+100, begin.y()));

        // An entry can carry no page count: a progress file that was interrupted,
        // edited, or written by a caller that only knows the page index. Dividing
        // by it would crash the paint, so such an entry shows no progress.
        int progressWidth = ProgressWidth;
        if (!progress.completed) {
            progressWidth =
                progress.totalPageCount > 0
                    ? qBound(0,
                             progress.resumePageIndex * ProgressWidth / progress.totalPageCount,
                             ProgressWidth)
                    : 0;
        }
        QBrush brRead(QColor::fromRgb(0x0, 0xff, 0x0, 0x40)),
            brUnread(QColor::fromRgb(0xff, 0x0, 0x0, 0x40));
        painter->save();
        painter->setPen(Qt::PenStyle::NoPen);
        painter->setBrush(brRead);
        painter->drawRect(begin.x(), begin.y(), progressWidth, rect.height() - ProgressHeight - 1);
        painter->setBrush(brUnread);
        painter->drawRect(begin.x() + progressWidth,
                          begin.y(),
                          ProgressWidth - progressWidth,
                          rect.height() - ProgressHeight - 1);
        painter->restore();
    } while (0);
}

QSize FolderItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text = index.data(FolderItemModel::SafeTextRole).toString();
    opt.font.setStyleStrategy(
        QFont::StyleStrategy(opt.font.styleStrategy() | QFont::NoFontMerging));
    opt.fontMetrics = QFontMetrics(opt.font);
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), widget);
    // The panel holds one line of text per row. The style lays an item out as
    // if it also carried an icon and the room around one, which leaves the row
    // about a third taller than the name it shows, so keep the text and a small
    // margin instead.
    size.setHeight(opt.fontMetrics.height() + RowTextMargin * 2);
    const FolderTextResult text =
        index.data(FolderItemModel::TextImagesRole).value<FolderTextResult>();
    if (text && !text->images.isEmpty()) {
        size.setWidth(
            qMax(size.width(), qCeil(text->images.first().deviceIndependentSize().width()) + 24));
    }
    return size;
}
