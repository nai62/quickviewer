#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtCore>
#include <QtWidgets>

#include "volumemanager.h"
#include "exifdialog.h"
#include "models/pagemanager.h"
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
    ImageRetouch retouchParameters() const override { return m_retouchParams; }
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
    void on_volumeChanged_triggered(QString path);
    void on_visiblePagesChanged(VisiblePages pages);
    void refreshRenderedPages();

    // Navigation
    void on_nextPage_triggered();
    void on_prevPage_triggered();
    void onActionNextPageOrVolume_triggered();
    void onActionPrevPageOrVolume_triggered();
    void on_fastForwardPage_triggered();
    void on_fastBackwardPage_triggered();
    void on_firstPage_triggered();
    void on_lastPage_triggered();
    void on_nextOnlyOnePage_triggered();
    void on_prevOnlyOnePage_triggered();
    void on_rotatePage_triggered();
    void on_showSubfolders_triggered(bool enable);

    // SlideShow
    void on_slideShowChanging_triggered();

    // Volume
    void on_nextVolume_triggered();
    void on_prevVolume_triggered();

    // View
    void on_fitting_triggered(bool maximized);
    void on_fitToWindow_triggered(bool enable);
    void on_fitToWidth_triggered(bool enable);
    void on_dualView_triggered(bool viewdual);
    void on_rightSideBook_triggered(bool rightSideBook);
    void on_wideImageAsOneView_triggered(bool wideImage);
    void on_firstImageAsOneView_triggered(bool firstImage);
    void on_dontEnlargeSmallImagesOnFitting(bool enable);
    void onActionSeparatePagesWhenWideImage_triggered(bool enable);
    void onActionLoupe_triggered(bool enable);
    void onActionScrollWithCursorWhenZooming_triggered(bool enable);
    void onActionShowFullscreenSignage_triggered(bool enable);
    void onActionHideMouseCursorInFullscreen_triggered(bool enable);

    void on_scaleUp_triggered();
    void on_scaleDown_triggered();
    void on_openFiler_triggered();
    void on_copyPage_triggered();
    void on_copyFile_triggered();

    // Retouch
    void onBrightness_valueChanged(ImageRetouch params);

private:
    qreal manualZoomScale() const;
    void updateSceneForContent(bool allowScrolling, const QRect &contentRect);
    void configureScrollInteraction(bool scrollable, bool leavingLoupe, const QRect &contentRect);
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

    PageManager *m_pageManager;
    ShaderManager m_shaderManager;
    QTimer *m_slideshowTimer;

    // Rotation and scale applied by touch gestures.
    qreal m_committedGestureScale;
    qreal m_committedGestureRotationDegrees;
    qreal m_pendingGestureScale;
    qreal m_pendingGestureRotationDegrees;
    qreal m_loupeFactor;

    int m_sceneRectUpdateDepth;
    int m_resizeEventDepth;
    qreal m_previousDrawScale;
    qreal m_lastScreenPixelRatio;

    bool m_skipResizeEvent;
    bool m_isFullScreen;
    bool m_scrollMode;
    bool m_openSeparatedPageFromEnd;
    bool m_loupeActive;
    bool m_wasLoupeActive;

    // Loupe
    QPoint m_loupeAnchorPosition;
    QRect m_sceneRectBeforeLoupe;
    QPoint m_scrollPositionBeforeLoupe;

    ImageRetouch m_retouchParams;
};

#endif // IMAGEVIEW_H
