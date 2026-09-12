#ifndef LOUPECONTROLLER_H
#define LOUPECONTROLLER_H

#include <QtCore>

#include <optional>

class LoupeController
{
public:
    struct SceneUpdate
    {
        bool leavingLoupe = false;
        QPoint scrollPositionToRestore;
    };

    LoupeController();

    bool isActive() const { return m_active; }
    void activate() { m_active = true; }
    void deactivate() { m_active = false; }

    qreal scaleFactor() const { return m_scaleFactor; }
    void adjustScaleFromWheel(int angleDeltaY);

    SceneUpdate prepareSceneUpdate(const QRect &contentRect, const QPoint &currentScrollPosition);
    void setAnchorPosition(const QPoint &position) { m_anchorPosition = position; }
    std::optional<QPoint> scrollPositionForCursor(
        const QPoint &cursorPosition,
        const QSize &viewportSize,
        const QRectF &magnifiedSceneRect) const;

private:
    static constexpr qreal InitialScaleFactor = 3.0;
    static constexpr qreal MinimumScaleFactor = 1.5;
    static constexpr qreal ScaleStep = 0.5;

    qreal m_scaleFactor = InitialScaleFactor;
    bool m_active = false;
    bool m_wasActive = false;
    QPoint m_anchorPosition;
    QRect m_sceneRectBeforeLoupe;
    QPoint m_scrollPositionBeforeLoupe;
};

#endif // LOUPECONTROLLER_H
