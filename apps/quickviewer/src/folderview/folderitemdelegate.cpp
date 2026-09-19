#include "folderitemdelegate.h"
#include "folderwindow.h"
#include "qvapplication.h"

FolderItemDelegate::FolderItemDelegate(QWidget *parent, FolderWindow *folderWindow)
    : QStyledItemDelegate(parent),
      m_folderWindow(folderWindow)
{
}
constexpr int ProgressWidth = 100;
constexpr int ProgressHeight = 10;

void FolderItemDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const bool isCurrentVolume = index.data(FolderItemModel::CurrentVolumeRole).toBool();
    if (isCurrentVolume) {
        opt.font.setBold(true);
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
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    // Draw the read progress bar above both the active background and item.
    do {
        if (!qApp->ShowReadProgress() || index.column() != 0) {
            break;
        }
        const QString path = QDir::fromNativeSeparators(m_folderWindow->itemPath(index));
        if (!qApp->readProgressStore()->contains(path)) {
            break;
        }
        const ReadProgress progress = qApp->readProgressStore()->at(path);
        QRect rect(option.rect);
        QPoint begin(rect.left() + 30, rect.top() + ProgressHeight);
        //        painter->drawLine(begin, QPoint(begin.x()+100, begin.y()));

        int progressWidth =
            progress.completed ? ProgressWidth
                               : progress.resumePageIndex * ProgressWidth / progress.totalPageCount;
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
