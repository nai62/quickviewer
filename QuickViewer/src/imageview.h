#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtCore>
#include <QtWidgets>

#include "volumemanager.h"
#include "exifdialog.h"
#include "models/pagemanager.h"
#include "models/loupecontroller.h"
#include "models/shadermanager.h"
#include "models/renderedpages.h"
#include "models/renderedpagemetrics.h"

/**
 * @brief The ImageView class
 * It provides to show 1 or 2 images once, using OpenGL.
 * It is made on QGraphicView, each images is used as QGraphicsItem
 */
class ImageView : public QGraphicsView, public PageRenderContext
{
    Q_OBJECT
public:
    enum RendererType {
        Native,
        OpenGL,
    };
    enum class AddRenderedPageResult {
        Rejected,
        AddedPortrait,
        AddedLandscape,
    };
    typedef QPair<uint, uint> ZoomFraction;
    explicit ImageView(QWidget *parent = Q_NULLPTR);
    void setRenderer(RendererType type = Native);
    void setPageManager(PageManager *manager);
    Qt::AnchorPoint hoverState() const { return m_hoverState; }
    void setResizeEventsSkipped(bool skipped) { m_skipResizeEvent = skipped; }
    bool isSlideShow() const { return m_slideshowTimer != nullptr; }
    void toggleSlideShow();
    bool isFullscreen() const { return m_isFullScreen; }
    void setFullscreenState(bool fullscreen) { m_isFullScreen = fullscreen; }
    void resetBackgroundColor();
    bool isScrollMode() const { return m_scrollMode; }
    int renderedPageCount() const;
    VisiblePages renderedPageContents() const;
    RenderedPageMetrics renderedPageMetrics() const;
    void updateGestureTransform(qreal scale, qreal rotationDegrees);
    void commitGestureTransform();
    void resetGestureTransform();
    RetouchParameters retouchParameters() const override { return m_retouchParams; }
    qreal currentPixelRatio() const override { return m_lastScreenPixelRatio; }
    void setCursor(const QCursor &cursor);
    AddRenderedPageResult addRenderedPage(ImageContent content, bool append);
    void clearRenderedPages();

signals:
    /**
     * @brief anchorHovered a signal when the mouse moved to one of 4 edges on this widget
     */
    void anchorHovered(Qt::AnchorPoint anchor) const;
    //    void pageChanged() const;

    void fittingChanged(qvEnums::FitMode mode) const;
    void scrollModeChanged(bool scrolling) const;
    void zoomingChanged() const;
    void slideShowStarted() const;
    void slideShowStopped() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void handleVolumeChanged(QString path);
    void handleVisiblePagesChanged(VisiblePages pages);
    void refreshRenderedPages();

    // Navigation
    void handleNextPageActionTriggered();
    void handlePrevPageActionTriggered();
    void handleNextPageOrVolumeActionTriggered();
    void handlePrevPageOrVolumeActionTriggered();
    void handleFastForwardActionTriggered();
    void handleFastBackwardActionTriggered();
    void handleFirstPageActionTriggered();
    void handleLastPageActionTriggered();
    void handleNextOnePageActionTriggered();
    void handlePrevOnePageActionTriggered();
    void handleRotateActionTriggered();
    void handleShowSubfoldersActionTriggered(bool checked);

    // SlideShow
    void handleSlideShowTimerTimeout();

    // Volume
    void handleNextVolumeActionTriggered();
    void handlePrevVolumeActionTriggered();

    // View
    void handleFittingActionTriggered(bool checked);
    void handleFitToWindowActionTriggered(bool checked);
    void handleFitToWidthActionTriggered(bool checked);
    void handleDualViewActionTriggered(bool checked);
    void handleRightSideBookActionTriggered(bool checked);
    void handleWideImageAsOneViewActionTriggered(bool checked);
    void handleFirstImageAsOneViewActionTriggered(bool checked);
    void handleDontEnlargeSmallImagesOnFittingActionTriggered(bool checked);
    void handleSeparatePagesWhenWideImageActionTriggered(bool checked);
    void handleLoupeToolActionTriggered(bool checked);
    void handleScrollWithCursorWhenZoomingActionTriggered(bool checked);
    void handleShowFullscreenSignageActionTriggered(bool checked);
    void handleHideMouseCursorInFullscreenActionTriggered(bool checked);

    void handleZoomInActionTriggered();
    void handleZoomOutActionTriggered();
    void handleOpenFilerActionTriggered();
    void handleCopyPageActionTriggered();
    void handleCopyFileActionTriggered();

    // Retouch
    void handleRetouchParametersChanged(RetouchParameters params);

private:
    qreal manualZoomScale() const;
    void updateSceneForContent(bool allowScrolling, const QRect &contentRect);
    void configureScrollInteraction(
        bool scrollable,
        const LoupeController::SceneUpdate &loupeUpdate,
        const QRect &contentRect);
    void preserveViewportCenter(qreal newScale, int previousHorizontalScroll, int previousVerticalScroll);
    void updateLoupeScrollFromCursor();
    void updateZoomScrollFromCursor();

    RendererType m_renderer;
    QPointer<QWidget> m_rendererViewport;
    RenderedPages m_renderedPages;

    Qt::AnchorPoint m_hoverState;
    /**
     * @brief for manual ZoomIn or ZoomOut
     */
    QList<ZoomFraction> m_zoomLevels;
    QVector<int> m_pageRotations;
    int m_zoomLevelIndex;
    QCursor m_loupeCursor;
    LoupeController m_loupeController;

    PageManager *m_pageManager;
    ShaderManager m_shaderManager;
    QTimer *m_slideshowTimer;

    // Rotation and scale applied by touch gestures.
    qreal m_committedGestureScale;
    qreal m_committedGestureRotationDegrees;
    qreal m_pendingGestureScale;
    qreal m_pendingGestureRotationDegrees;
    int m_sceneRectUpdateDepth;
    int m_resizeEventDepth;
    qreal m_previousDrawScale;
    qreal m_lastScreenPixelRatio;

    bool m_skipResizeEvent;
    bool m_isFullScreen;
    bool m_scrollMode;
    bool m_openSeparatedPageFromEnd;
    RetouchParameters m_retouchParams;
};

#endif // IMAGEVIEW_H
