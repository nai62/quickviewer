#ifndef CURSORSCROLLMAPPING_H
#define CURSORSCROLLMAPPING_H

#include <QtCore>

#include <optional>

class CursorScrollMapping
{
public:
    static QPoint zoomScrollPosition(
        QPoint cursorPosition,
        QSize viewportSize,
        QPoint minimum,
        QPoint maximum)
    {
        return QPoint(
            zoomScrollValue(cursorPosition.x(), viewportSize.width(), minimum.x(), maximum.x()),
            zoomScrollValue(cursorPosition.y(), viewportSize.height(), minimum.y(), maximum.y()));
    }

    static std::optional<QPoint> loupeScrollPosition(
        QPoint cursorPosition,
        QPoint loupeAnchor,
        QSize viewportSize,
        QRect baseSceneRect,
        QRectF magnifiedSceneRect,
        QPoint baseScrollPosition)
    {
        baseSceneRect.moveTo(
            baseSceneRect.left() - baseScrollPosition.x(),
            baseSceneRect.top() - baseScrollPosition.y());
        if (baseSceneRect.width() == 0 || baseSceneRect.height() == 0 || loupeAnchor.x() == 0 || loupeAnchor.y() == 0 || viewportSize.width() == loupeAnchor.x() || viewportSize.height() == loupeAnchor.y()) {
            return std::nullopt;
        }

        const QPoint anchorScenePosition(
            (loupeAnchor.x() - baseSceneRect.left()) * magnifiedSceneRect.width() / baseSceneRect.width() + magnifiedSceneRect.left(),
            (loupeAnchor.y() - baseSceneRect.top()) * magnifiedSceneRect.height() / baseSceneRect.height() + magnifiedSceneRect.top());
        const QPoint anchorScrollOffset = anchorScenePosition - loupeAnchor;

        return QPoint(
            loupeScrollValue(cursorPosition.x(), loupeAnchor.x(), viewportSize.width(), magnifiedSceneRect.left(), magnifiedSceneRect.right(), anchorScrollOffset.x()),
            loupeScrollValue(cursorPosition.y(), loupeAnchor.y(), viewportSize.height(), magnifiedSceneRect.top(), magnifiedSceneRect.bottom(), anchorScrollOffset.y()));
    }

private:
    static int zoomScrollValue(int cursorPosition, int viewportExtent, int minimum, int maximum)
    {
        if (viewportExtent <= 0) {
            return minimum;
        }
        const int mappedCursor = cursorPosition < viewportExtent / 4
                                     ? 0
                                     : (cursorPosition - viewportExtent / 4) * 2;
        return minimum + mappedCursor * (maximum - minimum) / viewportExtent;
    }

    static int loupeScrollValue(
        int cursorPosition,
        int loupeAnchor,
        int viewportExtent,
        qreal sceneMinimum,
        qreal sceneMaximum,
        int anchorScrollOffset)
    {
        return cursorPosition < loupeAnchor
                   ? sceneMinimum + (anchorScrollOffset - sceneMinimum) * (2 * cursorPosition - loupeAnchor) / loupeAnchor
                   : sceneMaximum - (sceneMaximum - anchorScrollOffset) * (loupeAnchor + viewportExtent - 2 * cursorPosition) / (viewportExtent - loupeAnchor);
    }
};

#endif // CURSORSCROLLMAPPING_H
