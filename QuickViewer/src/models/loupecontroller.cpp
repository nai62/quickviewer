#include "loupecontroller.h"

#include "cursorscrollmapping.h"

LoupeController::LoupeController() = default;

void LoupeController::adjustScaleFromWheel(int angleDeltaY)
{
    if (angleDeltaY < 0) {
        m_scaleFactor = qMax(MinimumScaleFactor, m_scaleFactor - ScaleStep);
    } else if (angleDeltaY > 0) {
        m_scaleFactor += ScaleStep;
    }
}

LoupeController::SceneUpdate LoupeController::prepareSceneUpdate(
    const QRect &contentRect,
    const QPoint &currentScrollPosition)
{
    if (!m_active) {
        m_sceneRectBeforeLoupe = contentRect;
    }
    const bool leavingLoupe = !m_active && m_wasActive;
    if (m_active && !m_wasActive) {
        m_scrollPositionBeforeLoupe = currentScrollPosition;
    }
    m_wasActive = m_active;
    return {leavingLoupe, m_scrollPositionBeforeLoupe};
}

std::optional<QPoint> LoupeController::scrollPositionForCursor(
    const QPoint &cursorPosition,
    const QSize &viewportSize,
    const QRectF &magnifiedSceneRect) const
{
    return CursorScrollMapping::loupeScrollPosition(
        cursorPosition,
        m_anchorPosition,
        viewportSize,
        m_sceneRectBeforeLoupe,
        magnifiedSceneRect,
        m_scrollPositionBeforeLoupe);
}
