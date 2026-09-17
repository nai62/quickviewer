#include "folderitemdelegate.h"
#include "folderwindow.h"
#include "qvapplication.h"

FolderItemDelegate::FolderItemDelegate(QWidget *parent, FolderWindow *folderWindow)
    : QStyledItemDelegate(parent),
      m_folderWindow(folderWindow)
{
}
#define PROGRESS_WIDTH 100
#define PROGRESS_HEIGHT 10

void FolderItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
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
        QPoint begin(rect.left() + 30, rect.top() + PROGRESS_HEIGHT);
        //        painter->drawLine(begin, QPoint(begin.x()+100, begin.y()));

        int progressWidth = progress.completed ? PROGRESS_WIDTH : progress.resumePageIndex * PROGRESS_WIDTH / progress.totalPageCount;
        QBrush brRead(QColor::fromRgb(0x0, 0xff, 0x0, 0x40)), brUnread(QColor::fromRgb(0xff, 0x0, 0x0, 0x40));
        painter->save();
        painter->setPen(Qt::PenStyle::NoPen);
        painter->setBrush(brRead);
        painter->drawRect(begin.x(), begin.y(), progressWidth, rect.height() - PROGRESS_HEIGHT - 1);
        painter->setBrush(brUnread);
        painter->drawRect(begin.x() + progressWidth, begin.y(), PROGRESS_WIDTH - progressWidth, rect.height() - PROGRESS_HEIGHT - 1);
        painter->restore();
    } while (0);
}
