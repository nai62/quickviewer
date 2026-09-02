#include <QtWidgets>
#ifndef QV_WITHOUT_OPENGL
#    include <QtOpenGL>
#endif

#include "imageview.h"
#include "models/cursorscrollmapping.h"
#include "qvapplication.h"

ImageView::ImageView(QWidget *parent)
    : QGraphicsView(parent),
      m_renderer(Native),
      m_rendererViewport(nullptr),
      m_hoverState(Qt::AnchorHorizontalCenter),
      m_loupeCursor(QCursor(QPixmap(":/icons/loupe_cursor"), 20, 23)),
      m_pageManager(nullptr),
      m_shaderManager(this),
      m_slideshowTimer(nullptr),
      m_committedGestureScale(1.0),
      m_committedGestureRotationDegrees(0.0),
      m_pendingGestureScale(1.0),
      m_pendingGestureRotationDegrees(0.0),
      m_loupeFactor(3.0),
      m_sceneRectUpdateDepth(0),
      m_resizeEventDepth(0),
      m_previousDrawScale(0),
      m_lastScreenPixelRatio(1.0),
      m_skipResizeEvent(false),
      m_isFullScreen(false),
      m_scrollMode(false),
      m_openSeparatedPageFromEnd(false),
      m_loupeActive(false),
      m_wasLoupeActive(false)
{
    m_zoomLevels
        << ZoomFraction(1, 6)    //  16.6%
        << ZoomFraction(1, 5)    //  20.0%
        << ZoomFraction(1, 4)    //  25.0%
        << ZoomFraction(1, 3)    //  33.3%
        << ZoomFraction(1, 2)    //  50.0%
        << ZoomFraction(3, 4)    //  75.0%
        << ZoomFraction(1, 1)    // 100  %
        << ZoomFraction(3, 2)    // 150  %
        << ZoomFraction(2, 1)    // 200  %
        << ZoomFraction(3, 1)    // 300  %
        << ZoomFraction(4, 1)    // 400  %
        << ZoomFraction(6, 1)    // 600  %
        << ZoomFraction(8, 1);   // 800  %
    m_zoomLevelIndex = 6; // 100

    QGraphicsScene *scene = new QGraphicsScene(this);
    setScene(scene);
    //    setTransformationAnchor(AnchorUnderMouse);
    //    setDragMode(ScrollHandDrag);
    //    setViewportUpdateMode(FullViewportUpdate);
    setAcceptDrops(false);
//    setDragMode(DragDropMode::InternalMove);
#ifdef QV_WITHOUT_OPENGL
    setRenderer(Native);
#else
    if (qApp->Effect() > qvEnums::UsingFixedShader) {
        setRenderer(OpenGL);
    }
#endif

    setMouseTracking(true);
    resetBackgroundColor();
    setAttribute(Qt::WA_AcceptTouchEvents);
}

int ImageView::renderedPageCount() const
{
    return m_renderedPages.count();
}

VisiblePages ImageView::renderedPageContents() const
{
    return m_renderedPages.contents();
}

RenderedPageMetrics ImageView::renderedPageMetrics() const
{
    return m_renderedPages.metrics();
}

void ImageView::setRenderer(RendererType type)
{
#ifdef QV_WITHOUT_OPENGL
    type = RendererType::Native;
#endif
    m_renderer = type;
    if (m_rendererViewport) {
        return;
    }
#ifndef QV_WITHOUT_OPENGL
    if (m_renderer == OpenGL) {
        m_rendererViewport = new QGLWidget(QGLFormat(QGL::SampleBuffers));
    } else
#endif
    {
        m_rendererViewport = new QWidget;
    }
    setViewport(m_rendererViewport);
}

void ImageView::setPageManager(PageManager *manager)
{
    if (!manager) {
        return;
    }
    m_pageManager = manager;
    m_pageManager->setViewportSize(viewport()->size());
    connect(manager, &PageManager::visiblePagesChanged, this, &ImageView::handleVisiblePagesChanged);
    connect(manager, SIGNAL(readyForPaint()), this, SLOT(refreshRenderedPages()));
    connect(manager, SIGNAL(volumeChanged(QString)), this, SLOT(handleVolumeChanged(QString)));
    connect(this, SIGNAL(slideShowStarted()), manager, SLOT(onSlideShowStarted()));
    connect(this, SIGNAL(slideShowStopped()), manager, SLOT(onSlideShowStopped()));
    handleVisiblePagesChanged(manager->visiblePages());
}

void ImageView::toggleSlideShow()
{
    if (!m_pageManager) {
        return;
    }
    if (m_slideshowTimer) {
        delete m_slideshowTimer;
        m_slideshowTimer = nullptr;
        emit slideShowStopped();
        return;
    }
    emit slideShowStarted();
    m_slideshowTimer = new QTimer();
    connect(m_slideshowTimer, SIGNAL(timeout()), this, SLOT(handleSlideShowTimerTimeout()));
    m_slideshowTimer->start(qApp->SlideShowWait());
}

void ImageView::resetBackgroundColor()
{
    //    QColor bg = qApp->BackgroundColor();
    //    setStyleSheet(QString("background-color:") + bg.name(QColor::HexArgb));
    if (!qApp->UseCheckeredPattern()) {
        setBackgroundBrush(QBrush(qApp->BackgroundColor(), Qt::SolidPattern));
        return;
    }
    QPixmap pix(16, 16);
    pix.fill(qApp->BackgroundColor());
    QPainter paint(&pix);
    QBrush brush2(qApp->BackgroundColor2(), Qt::SolidPattern);
    paint.fillRect(QRect(0, 0, 8, 8), brush2);
    paint.fillRect(QRect(8, 8, 8, 8), brush2);
    QBrush brush(pix);
    setBackgroundBrush(brush);
}

void ImageView::handleVolumeChanged(QString)
{
    if (!m_pageManager) {
        return;
    }
    m_pageRotations = QVector<int>(m_pageManager->size());
}

ImageView::AddRenderedPageResult ImageView::addRenderedPage(ImageContent content, bool append)
{
    const int pageCount = renderedPageCount();
    if (m_pageManager == nullptr || pageCount >= 2) {
        return AddRenderedPageResult::Rejected;
    }
    const bool landscape = content.Image.width() > content.Image.height();
    if (!m_renderedPages.add(
            std::move(content), append, this, scene(), this, m_openSeparatedPageFromEnd, this, [this] { refreshRenderedPages(); })) {
        return AddRenderedPageResult::Rejected;
    }

    m_shaderManager.prepareInitialize();

    return landscape ? AddRenderedPageResult::AddedLandscape
                     : AddRenderedPageResult::AddedPortrait;
}

void ImageView::clearRenderedPages()
{
    m_renderedPages.clear();
    // horizontalScrollBar()->setValue(0);
    // verticalScrollBar()->setValue(0);
}

void ImageView::handleVisiblePagesChanged(VisiblePages pages)
{
    clearRenderedPages();
    for (int index = 0; index < pages.count(); ++index) {
        const ImageContent *content = pages.at(index);
        if (content) {
            addRenderedPage(*content, true);
        }
    }
}
//static int paintCnt=0;
void ImageView::refreshRenderedPages()
{
    //    qDebug() << "refreshRenderedPages " << paintCnt++;
    if (qApp->Effect() > qvEnums::UsingFixedShader) {
        setRenderer(OpenGL);
    }
    const int renderedCount = renderedPageCount();
    if (renderedCount > 0 && m_pageManager) {
        const int currentPage = m_pageManager->currentPage();
        RenderedPageLayout layout;
        layout.viewport = QRect(QPoint(), viewport()->size());
        layout.fitMode = qApp->Fitting()
                             ? qApp->ImageFitMode()
                             : qvEnums::NoFitting;
        layout.manualScale = manualZoomScale();
        layout.scaleFactor = m_loupeActive ? m_loupeFactor : 1.0;
        layout.loupe = m_loupeActive;
        layout.separateWideImages = qApp->SeparatePagesWhenWideImage();
        layout.rightSideBook = qApp->RightSideBook();
        for (int index = 0; index < renderedCount; ++index) {
            layout.rotations.push_back(
                m_pageRotations.value(currentPage + index, 0));
            layout.signage.push_back(
                qApp->ShowFullscreenSignage() && m_isFullScreen
                    ? m_pageManager->pageSignage(index)
                    : QString());
        }
        const QRect sceneRect = m_renderedPages.layout(
            layout, [this](QGraphicsPixmapItem *item, const ImageContent &content, QSize drawSize) {
                m_shaderManager.prepare(item, content, drawSize);
            });
        // if Size of Image overs Size of View, use Image's size
        updateSceneForContent(
            !(qApp->Fitting() && qApp->ImageFitMode() == qvEnums::FitToRect) || m_loupeActive || m_lastScreenPixelRatio > 1.0, sceneRect);
    }
    // QGraphicsView updates the cursor internally,
    // but QV cannot trap this event, so it forcibly clears the cursor.
    if (m_isFullScreen && qApp->HideMouseCursorInFullscreen()) {
        setCursor(Qt::BlankCursor);
    }
    m_shaderManager.prepareFinished();
}

void ImageView::updateSceneForContent(bool allowScrolling, const QRect &contentRect)
{
    // refreshRenderedPages() and updateSceneForContent() may be called multiple times, and the scroll value from the second time onwards will not be accurate.
    // Therefore, the original scroll value is traced the first time, and the scroll value is corrected when the last call is completed.
    const int previousHorizontalScroll = horizontalScrollBar()->value();
    const int previousVerticalScroll = verticalScrollBar()->value();
    m_sceneRectUpdateDepth++;

    if (!m_loupeActive) {
        m_sceneRectBeforeLoupe = contentRect;
    }
    const bool leavingLoupe = !m_loupeActive && m_wasLoupeActive;
    if (m_loupeActive && !m_wasLoupeActive) {
        m_scrollPositionBeforeLoupe = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
    }
    m_wasLoupeActive = m_loupeActive;
    const bool scrollable = allowScrolling && (size().width() < contentRect.width() || size().height() < contentRect.height());
    const QRectF previousSceneRect = scene()->sceneRect();
    const QRectF updatedSceneRect = scrollable
                                        ? QRectF(QPoint(qMin(0, contentRect.left()), 0), QSize(qMax(size().width(), contentRect.width()), qMax(size().height(), contentRect.height())))
                                        : QRectF(QPoint(), size());
    scene()->setSceneRect(updatedSceneRect);
    configureScrollInteraction(scrollable, leavingLoupe, contentRect);

    // Correct the scroll bar so that it keep at the center of the viewport
    // when the image display magnification is changed.
    const std::optional<qreal> firstDrawScale = m_renderedPages.firstDrawScale();
    if (m_sceneRectUpdateDepth == 1 && firstDrawScale) {
        preserveViewportCenter(*firstDrawScale, previousHorizontalScroll, previousVerticalScroll);
    }

    if (m_scrollMode != scrollable) {
        emit scrollModeChanged(m_scrollMode = scrollable);
    }
    if (previousSceneRect != updatedSceneRect) {
        emit zoomingChanged();
    }

    m_sceneRectUpdateDepth--;
}

void ImageView::configureScrollInteraction(bool scrollable, bool leavingLoupe, const QRect &contentRect)
{
    if (!scrollable) {
        setDragMode(QGraphicsView::NoDrag);
        return;
    }
    if (m_loupeActive) {
        m_loupeAnchorPosition = mapFromGlobal(QCursor::pos());
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setDragMode(QGraphicsView::NoDrag);
        updateLoupeScrollFromCursor();
        return;
    }
    if (qApp->ScrollWithCursorWhenZooming()) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setDragMode(QGraphicsView::NoDrag);
        updateZoomScrollFromCursor();
        return;
    }

    const bool hideScrollBars = m_isFullScreen && qApp->HideScrollBarInFullscreen();
    setHorizontalScrollBarPolicy(!hideScrollBars && size().width() < contentRect.width() + verticalScrollBar()->width() ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(!hideScrollBars && size().height() < contentRect.height() + horizontalScrollBar()->height() ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    setDragMode(QGraphicsView::ScrollHandDrag);
    if (leavingLoupe) {
        horizontalScrollBar()->setValue(m_scrollPositionBeforeLoupe.x());
        verticalScrollBar()->setValue(m_scrollPositionBeforeLoupe.y());
    }
}

void ImageView::preserveViewportCenter(qreal newScale, int previousHorizontalScroll, int previousVerticalScroll)
{
    if (!qApp->Fitting() && !m_loupeActive && m_previousDrawScale > 0 && m_previousDrawScale != newScale) {
        const int halfViewportWidth = 0.5 * viewport()->width();
        const int halfViewportHeight = 0.5 * viewport()->height();
        horizontalScrollBar()->setValue((previousHorizontalScroll + halfViewportWidth) / m_previousDrawScale * newScale - halfViewportWidth);
        verticalScrollBar()->setValue((previousVerticalScroll + halfViewportHeight) / m_previousDrawScale * newScale - halfViewportHeight);
    }
    m_previousDrawScale = newScale;
}

void ImageView::updateLoupeScrollFromCursor()
{
    const std::optional<QPoint> scrollPosition = CursorScrollMapping::loupeScrollPosition(
        mapFromGlobal(QCursor::pos()), m_loupeAnchorPosition, size(), m_sceneRectBeforeLoupe, scene()->sceneRect(), m_scrollPositionBeforeLoupe);
    if (!scrollPosition) {
        return;
    }
    horizontalScrollBar()->setValue(scrollPosition->x());
    verticalScrollBar()->setValue(scrollPosition->y());
}

void ImageView::updateZoomScrollFromCursor()
{
    if (width() <= 0 || height() <= 0) {
        return;
    }
    const QPoint scrollPosition = CursorScrollMapping::zoomScrollPosition(
        mapFromGlobal(QCursor::pos()), size(), QPoint(horizontalScrollBar()->minimum(), verticalScrollBar()->minimum()), QPoint(horizontalScrollBar()->maximum(), verticalScrollBar()->maximum()));
    horizontalScrollBar()->setValue(scrollPosition.x());
    verticalScrollBar()->setValue(scrollPosition.y());
}
void ImageView::updateGestureTransform(qreal scale, qreal rotationDegrees)
{
    m_pendingGestureScale = scale;
    m_pendingGestureRotationDegrees = rotationDegrees;
    setTransform(
        QTransform()
            .scale(m_committedGestureScale * scale, m_committedGestureScale * scale)
            .rotate(m_committedGestureRotationDegrees + rotationDegrees));
}

void ImageView::commitGestureTransform()
{
    m_committedGestureScale *= m_pendingGestureScale;
    m_committedGestureRotationDegrees += m_pendingGestureRotationDegrees;
    m_pendingGestureScale = 1.0;
    m_pendingGestureRotationDegrees = 0.0;
}

void ImageView::resetGestureTransform()
{
    m_committedGestureScale = 1.0;
    m_committedGestureRotationDegrees = 0.0;
    m_pendingGestureScale = 1.0;
    m_pendingGestureRotationDegrees = 0.0;
    setTransform(QTransform());
}

void ImageView::setCursor(const QCursor &cursor)
{
    // QGraphicsView is made up of layers of widgets, views, and items, all of which have setCursor()
    QGraphicsView::setCursor(cursor);
    if (m_isFullScreen && qApp->HideMouseCursorInFullscreen()) {
        viewport()->setCursor(cursor);
        m_renderedPages.setCursor(cursor);
    }
}

void ImageView::paintEvent(QPaintEvent *event)
{
    QGraphicsView::paintEvent(event);
    if (m_pageManager) {
        m_pageManager->notifyInitialImagePainted();
    }
}

void ImageView::resizeEvent(QResizeEvent *event)
{
    if (scene() && !m_isFullScreen) {
        scene()->setSceneRect(QRect(QPoint(), event->size()));
    }
    QGraphicsView::resizeEvent(event);
    if (m_pageManager) {
        m_pageManager->setViewportSize(event->size());
    }
    if (m_resizeEventDepth == 0) {
        m_resizeEventDepth++;
        qreal newRatio = screen()->devicePixelRatio();
        if (m_lastScreenPixelRatio != newRatio) {

            QTransform scaling(1.0 / newRatio, 0, 0, 1.0 / newRatio, 0, 0);
            setTransform(scaling);
            m_lastScreenPixelRatio = newRatio;
        }
        if (!m_skipResizeEvent && m_pageManager) {
            refreshRenderedPages();
            m_pageManager->pageChanged();
        }
        m_resizeEventDepth--;
    }
}

void ImageView::handleNextPageActionTriggered()
{
    if (qApp->SeparatePagesWhenWideImage() && m_renderedPages.advanceSeparatedPage()) {
        refreshRenderedPages();
        return;
    }
    if (m_pageManager) {
        m_pageManager->nextPage();
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
}

void ImageView::handlePrevPageActionTriggered()
{
    if (qApp->SeparatePagesWhenWideImage() && m_renderedPages.rewindSeparatedPage()) {
        refreshRenderedPages();
        return;
    }
    m_openSeparatedPageFromEnd = true;
    if (m_pageManager) {
        m_pageManager->prevPage();
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
    m_openSeparatedPageFromEnd = false;
}

void ImageView::handleNextPageOrVolumeActionTriggered()
{
    if (qApp->SeparatePagesWhenWideImage() && m_renderedPages.advanceSeparatedPage()) {
        refreshRenderedPages();
        return;
    }
    if (m_pageManager) {
        if (!m_pageManager->nextPage() && m_pageManager->nextVolume()) {
            m_pageManager->firstPage();
        }
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
}

void ImageView::handlePrevPageOrVolumeActionTriggered()
{
    if (qApp->SeparatePagesWhenWideImage() && m_renderedPages.rewindSeparatedPage()) {
        refreshRenderedPages();
        return;
    }
    if (m_pageManager) {
        if (!m_pageManager->prevPage() && m_pageManager->prevVolume()) {
            m_pageManager->lastPage();
        }
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
}

void ImageView::handleFastForwardActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->fastForwardPage();
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
}

void ImageView::handleFastBackwardActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->fastBackwardPage();
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
}

void ImageView::handleFirstPageActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->firstPage();
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
}

void ImageView::handleLastPageActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->lastPage();
    }
    if (isSlideShow()) {
        toggleSlideShow();
    }
}

void ImageView::handleNextOnePageActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->nextOnlyOnePage();
    }
}

void ImageView::handlePrevOnePageActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->prevOnlyOnePage();
    }
}

void ImageView::handleRotateActionTriggered()
{
    if (!m_pageManager || m_pageRotations.empty()) {
        return;
    }
    const int page = m_pageManager->currentPage();
    if (page < 0 || page >= m_pageRotations.size()) {
        return;
    }
    m_pageRotations[page] += 90;
    refreshRenderedPages();
}

void ImageView::handleShowSubfoldersActionTriggered(bool checked)
{
    qApp->setShowSubfolders(checked);
    if (!m_pageManager) {
        return;
    }
    if (m_pageManager->isFolder()) {
        m_pageManager->reloadVolumeAfterRemoveImage();
    }
}

void ImageView::handleSlideShowTimerTimeout()
{
    if (!m_pageManager) {
        return;
    }
    int page = m_pageManager->currentPage();
    m_pageManager->nextPage();
    if (page == m_pageManager->currentPage()) {
        m_pageManager->firstPage();
    }
}

void ImageView::handleNextVolumeActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->nextVolume();
    }
}

void ImageView::handlePrevVolumeActionTriggered()
{
    if (m_pageManager) {
        m_pageManager->prevVolume();
    }
}

void ImageView::handleShowFullscreenSignageActionTriggered(bool checked)
{
    qApp->setShowFullscreenSignage(checked);
    refreshRenderedPages();
}

void ImageView::handleHideMouseCursorInFullscreenActionTriggered(bool checked)
{
    qApp->setHideMouseCursorInFullscreen(checked);
}

#define HOVER_BORDER 20
//#define NOT_HOVER_AREA 100

void ImageView::mouseMoveEvent(QMouseEvent *e)
{
    QGraphicsView::mouseMoveEvent(e);
    //    qDebug() << "qApp->HideMouseCursorInFullscreen()" << qApp->HideMouseCursorInFullscreen();
    int NOT_HOVER_AREA = width() / 3;
    int hover_border = qApp->LargeToolbarIcons() ? 3 * HOVER_BORDER : HOVER_BORDER;
    if (e->pos().x() < hover_border && e->pos().y() < height() - hover_border) {
        if (m_hoverState != Qt::AnchorLeft) {
            emit anchorHovered(Qt::AnchorLeft);
        }
        m_hoverState = Qt::AnchorLeft;
        if (m_isFullScreen && qApp->HideMouseCursorInFullscreen()) {
            setCursor(Qt::BlankCursor);
        } else {
            setCursor(Qt::PointingHandCursor);
        }
        return;
    }
    if (e->pos().x() > width() - hover_border) {
        if (m_hoverState != Qt::AnchorRight) {
            emit anchorHovered(Qt::AnchorRight);
        }
        m_hoverState = Qt::AnchorRight;
        if (m_isFullScreen && qApp->HideMouseCursorInFullscreen()) {
            setCursor(Qt::BlankCursor);
        } else {
            setCursor(Qt::PointingHandCursor);
        }

        return;
    }
    if (m_isFullScreen && qApp->HideMouseCursorInFullscreen()) {
        setCursor(Qt::BlankCursor);
    } else if (qApp->LoupeTool()) {
        setCursor(m_loupeCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    //    qDebug() << qApp->ScrollWithCursorWhenZooming() << scene()->sceneRect() << size();
    if (m_loupeActive) {
        updateLoupeScrollFromCursor();
    } else if (qApp->ScrollWithCursorWhenZooming() && (scene()->sceneRect().width() > width() || scene()->sceneRect().height() > height())) {
        updateZoomScrollFromCursor();
    }

    if (e->pos().y() < hover_border) {
        if (m_hoverState != Qt::AnchorTop) {
            emit anchorHovered(Qt::AnchorTop);
        }
        m_hoverState = Qt::AnchorTop;
        return;
    }
    if (e->pos().y() > height() - hover_border && e->pos().x() > NOT_HOVER_AREA) {
        if (m_hoverState != Qt::AnchorBottom) {
            emit anchorHovered(Qt::AnchorBottom);
        }
        m_hoverState = Qt::AnchorBottom;
        return;
    }
    if (m_hoverState != Qt::AnchorHorizontalCenter) {
        emit anchorHovered(Qt::AnchorHorizontalCenter);
    }
    m_hoverState = Qt::AnchorHorizontalCenter;
}

void ImageView::wheelEvent(QWheelEvent *event)
{
    int delta_y = event->angleDelta().y();
    int delta = delta_y < 0 ? -Q_MOUSE_DELTA : delta_y > 0 ? Q_MOUSE_DELTA
                                                           : 0;
    QMouseValue mv(QKeySequence(qApp->keyboardModifiers()), event->buttons(), delta);
    QAction *action = qApp->mouseActions().getActionByValue(mv);
    if (action != nullptr) {
        QString text = action->objectName();
        if (text == "actionZoomIn" || text == "actionZoomOut") {
            action->trigger();
            event->accept();
            return;
        }
    }
    if (m_loupeActive) {
        if (delta_y < 0) {
            m_loupeFactor = qMax(1.5, m_loupeFactor - 0.5);
        }
        if (delta_y > 0) {
            m_loupeFactor += 0.5;
        }
        refreshRenderedPages();
        return;
    }
    if (qApp->ScrollWithCursorWhenZooming()) {
        QMouseValue mv(QKeySequence(qApp->keyboardModifiers()), event->buttons(), delta);
        QAction *action = qApp->mouseActions().getActionByValue(mv);
        if (action) {
            action->trigger();
            event->accept();
            return;
        }
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    if (!qApp->LoupeTool() || (event->buttons() != Qt::LeftButton)) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    m_loupeActive = true;
    refreshRenderedPages();
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!qApp->LoupeTool() || (event->buttons() & Qt::LeftButton)) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }
    m_loupeActive = false;
    refreshRenderedPages();
}

void ImageView::handleFittingActionTriggered(bool checked)
{
    if (checked) {
        qApp->setFitting(checked);
        refreshRenderedPages();
    } else {
        // When turning off fitting mode, use the scale up event handler instead.
        handleZoomInActionTriggered();
    }
}

void ImageView::handleFitToWindowActionTriggered(bool checked)
{
    if (!checked) {
        return;
    }
    qApp->setImageFitMode(qvEnums::FitToRect);
    emit fittingChanged(qvEnums::FitToRect);
    qApp->setFitting(true);
    refreshRenderedPages();
}

void ImageView::handleFitToWidthActionTriggered(bool checked)
{
    if (!checked) {
        return;
    }
    qApp->setImageFitMode(qvEnums::FitToWidth);
    emit fittingChanged(qvEnums::FitToWidth);
    qApp->setFitting(true);
    refreshRenderedPages();
}

void ImageView::handleDualViewActionTriggered(bool checked)
{
    qApp->setDualView(checked);

    if (m_pageManager) {
        m_pageManager->reloadCurrentPage();
    }
    refreshRenderedPages();
}

void ImageView::handleRightSideBookActionTriggered(bool checked)
{
    qApp->setRightSideBook(checked);
    refreshRenderedPages();
}

void ImageView::handleZoomInActionTriggered()
{
    const std::optional<qreal> firstDrawScale = m_renderedPages.firstDrawScale();
    if (!firstDrawScale) {
        return;
    }
    if (qApp->Fitting()) {
        qApp->setFitting(false);
        emit fittingChanged(qApp->ImageFitMode());
        const qreal scale = *firstDrawScale;
        m_zoomLevelIndex = 0;
        qDebug() << m_zoomLevelIndex << (m_zoomLevels.size() - 1) << scale << manualZoomScale();
        while (m_zoomLevelIndex < m_zoomLevels.size() - 1 && manualZoomScale() < scale) {
            m_zoomLevelIndex++;
        }
        refreshRenderedPages();
        return;
    }
    if (m_zoomLevelIndex < m_zoomLevels.size() - 1) {
        m_zoomLevelIndex++;
    }
    refreshRenderedPages();
}

void ImageView::handleZoomOutActionTriggered()
{
    const std::optional<qreal> firstDrawScale = m_renderedPages.firstDrawScale();
    if (!firstDrawScale) {
        return;
    }
    if (qApp->Fitting()) {
        qApp->setFitting(false);
        emit fittingChanged(qApp->ImageFitMode());
        const qreal scale = *firstDrawScale;
        m_zoomLevelIndex = m_zoomLevels.size() - 1;
        while (m_zoomLevelIndex > 0 && manualZoomScale() > scale) {
            m_zoomLevelIndex--;
        }
        refreshRenderedPages();
        return;
    }
    if (m_zoomLevelIndex > 0) {
        m_zoomLevelIndex--;
    }
    refreshRenderedPages();
}

void ImageView::handleWideImageAsOneViewActionTriggered(bool checked)
{
    qApp->setWideImageAsOnePageInDualView(checked);
    if (m_pageManager) {
        m_pageManager->reloadCurrentPage();
    }
    refreshRenderedPages();
}

void ImageView::handleFirstImageAsOneViewActionTriggered(bool checked)
{
    qApp->setFirstImageAsOnePageInDualView(checked);
    if (m_pageManager) {
        m_pageManager->reloadCurrentPage();
    }
    refreshRenderedPages();
}

void ImageView::handleDontEnlargeSmallImagesOnFittingActionTriggered(bool checked)
{
    qApp->setDontEnlargeSmallImagesOnFitting(checked);
    refreshRenderedPages();
}

void ImageView::handleSeparatePagesWhenWideImageActionTriggered(bool checked)
{
    qApp->setSeparatePagesWhenWideImage(checked);
    refreshRenderedPages();
}

void ImageView::handleLoupeToolActionTriggered(bool checked)
{
    qApp->setLoupeTool(checked);
    if (!checked) {
        m_loupeActive = false;
        refreshRenderedPages();
    }
}

void ImageView::handleScrollWithCursorWhenZoomingActionTriggered(bool checked)
{
    qApp->setScrollWithCursorWhenZooming(checked);
    refreshRenderedPages();
}

void ImageView::handleOpenFilerActionTriggered()
{
    if (!m_pageManager) {
        return;
    }
    QString path = m_pageManager->volumePath();
    if (m_pageManager->isFolder()) {
        path = m_pageManager->currentPagePath();
    }
#if defined(Q_OS_WIN)
    const QString explorer = QLatin1String("explorer.exe ");
    QFileInfo fi(path);

    // canonicalFilePath returns empty if the file does not exist
    if (!fi.canonicalFilePath().isEmpty()) {
        QString nativeArgs;
        if (!fi.isDir()) {
            nativeArgs += QLatin1String("/select,");
        }
        nativeArgs += QLatin1Char('"');
        nativeArgs += QDir::toNativeSeparators(fi.canonicalFilePath());
        nativeArgs += QLatin1Char('"');

        qDebug() << "OO Open explorer commandline:" << explorer << nativeArgs;
        QProcess p;
        // QProcess on Windows tries to wrap the whole argument/program string
        // with quotes if it detects a space in it, but explorer wants the quotes
        // only around the path. Use setNativeArguments to bypass this logic.
        p.setNativeArguments(nativeArgs);
        p.start(explorer);
        p.waitForFinished(5000);
    }
#else
    if (!QFileInfo(path).isDir()) {
        QDir dir(path);
        dir.cdUp();
        path = dir.path();
    }
    QUrl url = QString("file:///%1").arg(path);
    QDesktopServices::openUrl(url);
#endif
}

void ImageView::handleCopyPageActionTriggered()
{
    const QImage image = m_renderedPages.firstImage();
    if (image.isNull()) {
        return;
    }
    QClipboard *clipboard = qApp->clipboard();
    clipboard->setImage(image);
}

void ImageView::handleCopyFileActionTriggered()
{
    if (!m_pageManager) {
        return;
    }
    const QString currentPath = m_pageManager->currentPagePath();
    if (currentPath.isEmpty()) {
        return;
    }
    QClipboard *clipboard = qApp->clipboard();
    QMimeData *mimeData = new QMimeData();
    QString path = QString("file:///%1").arg(currentPath);
    mimeData->setData("text/uri-list", path.toUtf8());
    clipboard->setMimeData(mimeData);
}

void ImageView::handleBrightnessChanged(ImageRetouch params)
{
    m_retouchParams = params;
    refreshRenderedPages();
}

qreal ImageView::manualZoomScale() const
{
    // Some OS allow you to change the display magnification.
    // In this case, the content drawn is automatically scaled by devicePixelRatio,
    // but avoid scaling only the image.
    // QScreen* screen0 = screen();
    // return 1.0*m_zoomLevels[m_zoomLevelIndex].first/m_zoomLevels[m_zoomLevelIndex].second/screen0->devicePixelRatio();
    return 1.0 * m_zoomLevels[m_zoomLevelIndex].first / m_zoomLevels[m_zoomLevelIndex].second;
}
