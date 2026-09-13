#include <QtConcurrent>

#include "renderedpage.h"
#include "qvapplication.h"
#include "qzimg.h"

#ifdef QV_WITH_LUMINOR
#    include "qluminor.h"
#endif

static int horizontalOffsetForAlignment(
    RenderedPage::PageAlign alignment,
    const QRect &viewport,
    const QSize &contentSize,
    bool clampCenteredOffset = false)
{
    if (alignment == RenderedPage::PageRight) {
        return 0;
    }
    const int offset = viewport.width() - contentSize.width();
    if (alignment == RenderedPage::PageCenter) {
        const int centeredOffset = offset / 2;
        return clampCenteredOffset ? qMax(0, centeredOffset) : centeredOffset;
    }
    return offset;
}

static int normalizedRotationDegrees(int m_rotationDegrees)
{
    const int remainder = m_rotationDegrees % 360;
    return remainder < 0 ? remainder + 360 : remainder;
}

static int combinedRotationDegrees(int baseRotationDegrees, int rotationOffset)
{
    const int combined = normalizedRotationDegrees(baseRotationDegrees) + normalizedRotationDegrees(rotationOffset);
    return combined >= 360 ? combined - 360 : combined;
}

RenderedPage::RenderedPage(QObject *parent, PageRenderSettings renderSettings)
    : QObject(parent),
      m_scene(nullptr),
      m_content(),
      m_graphicsItem(nullptr),
      m_rotationDegrees(0),
      m_signageText(),
      m_signageTextItem(nullptr),
      m_signageBackgroundItem(nullptr),
      m_drawScale(1.0),
      m_displayScale(1.0),
      m_separationState(NotSeparated),
      m_resizeGeneratingState(0),
      m_initialized(false),
      m_renderSettings(std::move(renderSettings))
{
}

RenderedPage::RenderedPage(QObject *parent, QGraphicsScene *graphicsScene, ImageContent imageContent, PageRenderSettings renderSettings)
    : QObject(parent),
      m_scene(graphicsScene),
      m_content(std::move(imageContent)),
      m_graphicsItem(nullptr),
      m_rotationDegrees(0),
      m_signageText(),
      m_signageTextItem(nullptr),
      m_signageBackgroundItem(nullptr),
      m_drawScale(1.0),
      m_displayScale(1.0),
      m_separationState(m_content.isLandscape() && qApp->SeparatePagesWhenWideImage() ? FirstHalf : NotSeparated),
      m_resizeGeneratingState(0),
      m_initialized(false),
      m_renderSettings(std::move(renderSettings))
{
    if (!m_content.loadedImageSize.width()) {
        QGraphicsTextItem *errorTextItem = m_scene->addText(tr("NOT IMAGE FILE", "Error messages to be displayed on screen when image loading fails"));
        errorTextItem->setDefaultTextColor(Qt::white);
        m_graphicsItem = errorTextItem;
        return;
    }
    if (m_content.exifInfo.ImageWidth > 0 && m_content.exifInfo.Orientation != 1) {
        switch (m_content.exifInfo.Orientation) {
        case 6: // 90 degrees clockwise
            m_rotationDegrees = 90;
            break;
        case 8: // 90 degrees counterclockwise
            m_rotationDegrees = 270;
            break;
        }
    }
    initializePage();
}

RenderedPage::~RenderedPage()
{
    dispose();
}

QGraphicsPixmapItem *RenderedPage::graphicsPixmapItem() const
{
    return dynamic_cast<QGraphicsPixmapItem *>(m_graphicsItem);
}

void RenderedPage::setCursor(const QCursor &cursor)
{
    if (m_graphicsItem) {
        m_graphicsItem->setCursor(cursor);
    }
}

void RenderedPage::updateSeparationForViewport(
    bool separateWideImages, QSize viewportSize)
{
    if (!separateWideImages || !m_content.isLandscape()) {
        return;
    }
    if (m_separationState == NotSeparated && viewportSize.width() < viewportSize.height()) {
        m_separationState = FirstHalf;
    }
    if (m_separationState != NotSeparated && viewportSize.width() > viewportSize.height()) {
        m_separationState = NotSeparated;
    }
}

void RenderedPage::showLastSeparatedHalf()
{
    if (m_separationState == FirstHalf) {
        m_separationState = SecondHalf;
    }
}

bool RenderedPage::advanceSeparatedHalf()
{
    if (m_separationState != FirstHalf) {
        return false;
    }
    m_separationState = SecondHalf;
    return true;
}

bool RenderedPage::rewindSeparatedHalf()
{
    if (m_separationState != SecondHalf) {
        return false;
    }
    m_separationState = FirstHalf;
    return true;
}

QPoint RenderedPage::offsetForRotation(int rotationOffset) const
{
    switch (combinedRotationDegrees(m_rotationDegrees, rotationOffset)) {
    case 90:
        return QPoint(m_content.loadedImage.height(), 0);
    case 180:
        return QPoint(m_content.loadedImage.width(), m_content.loadedImage.height());
    case 270:
        return QPoint(0, m_content.loadedImage.width());
    default:
        return QPoint();
    }
}

QSize RenderedPage::rotatedImageSize(int rotationOffset) const
{
    const int rotation = combinedRotationDegrees(m_rotationDegrees, rotationOffset);
    return rotation == 90 || rotation == 270
               ? QSize(m_content.loadedImage.height(), m_content.loadedImage.width())
               : m_content.loadedImage.size();
}

QRect RenderedPage::setPageLayoutFitting(QRect viewport, RenderedPage::PageAlign alignment, qvEnums::FitMode fitMode, qreal loupe, int rotationOffset)
{
    QRect viewport1 = viewport;
    const qreal pixelRatio = m_renderSettings.pixelRatio;
    if (pixelRatio != 1.0) {
        // Compensate for the world transform used on high-DPI displays.
        viewport1 = QRect(viewport.left() * pixelRatio, viewport.top() * pixelRatio, viewport.width() * pixelRatio, viewport.height() * pixelRatio);
    }

    if (!m_content.loadedImageSize.width()) {
        applyResize(1.0, 0, viewport.topLeft(), QSize(100, 100));
        return QRect(viewport.topLeft(), QSize(100, 100));
    }
    QSize currentSize = rotatedImageSize(rotationOffset);
    const bool separatePage = viewport.height() > viewport1.width() && m_separationState != NotSeparated;
    if (separatePage) {
        currentSize = QSize(currentSize.width() / 2, currentSize.height());
    }
    const QSize targetSize = fitMode == qvEnums::FitToRect
                                 ? currentSize.scaled(viewport1.size(), Qt::KeepAspectRatio)
                                 : QSize(viewport1.width(), currentSize.height() * viewport1.width() / currentSize.width());
    const qreal scale = m_drawScale = 1.0 * targetSize.width() / currentSize.width();
    m_displayScale = scale;
    if (loupe > 1.0) {
        return setPageLayoutManual(viewport, alignment, scale * loupe, rotationOffset, true);
    }
    if (scale > 1.0 && qApp->DontEnlargeSmallImagesOnFitting()) {
        return setPageLayoutManual(viewport, alignment, 1.0, rotationOffset);
    }

    QPoint rotationOffsetPosition = offsetForRotation(rotationOffset);
    rotationOffsetPosition *= scale;

    QRect drawRect;
    if (fitMode == qvEnums::FitToRect) {
        if (targetSize.height() == viewport1.height()) { // Fit to the top and bottom edges.
            const int horizontalOffset = horizontalOffsetForAlignment(
                alignment, viewport, targetSize);
            drawRect = QRect(QPoint(rotationOffsetPosition.x() + viewport.x() + horizontalOffset, rotationOffsetPosition.y()), targetSize);
        } else { // Fit to the left and right edges.
            const int horizontalOffset = horizontalOffsetForAlignment(
                alignment, viewport, targetSize);
            int verticalOffset = (viewport.height() - targetSize.height()) / 2;
            verticalOffset = pixelRatio != 1.0 ? 0 : verticalOffset; // If pixelRatio > 1, no correction is required
            drawRect = QRect(QPoint(rotationOffsetPosition.x() + viewport.x() + horizontalOffset, rotationOffsetPosition.y() + verticalOffset), targetSize);
        }
    } else {
        if (viewport.height() < targetSize.height() && targetSize.height() < viewport1.height()) {
            // Display magnification is automatically corrected, so special correction is required.
            const int horizontalOffset = horizontalOffsetForAlignment(
                alignment, viewport, targetSize);
            const int verticalOffset = pixelRatio == 1.0 ? 0 : (-viewport1.height() + targetSize.height()) / 2;
            drawRect = QRect(QPoint(rotationOffsetPosition.x() + viewport.x() + horizontalOffset, rotationOffsetPosition.y() + verticalOffset), targetSize);
        } else {
            const int horizontalOffset = horizontalOffsetForAlignment(
                alignment, viewport, targetSize);
            const int verticalOffset = pixelRatio == 1.0 ? 0 : (viewport.height() - viewport1.height()) / 2;
            drawRect = QRect(QPoint(rotationOffsetPosition.x() + viewport.x() + horizontalOffset, rotationOffsetPosition.y() + verticalOffset), targetSize);
        }
    }

    QPoint imagePos = drawRect.topLeft();
    if (separatePage && ((m_separationState == FirstHalf && qApp->RightSideBook()) || (m_separationState == SecondHalf && !qApp->RightSideBook()))) {
        // Display only the right side of the image
        imagePos.rx() -= targetSize.width();
    }
    applyResize(scale, rotationOffset, imagePos, targetSize);
    return drawRect;
}

QRect RenderedPage::setPageLayoutManual(QRect viewport, RenderedPage::PageAlign alignment, qreal scale, int rotationOffset, bool loupe)
{
    if (!m_content.loadedImageSize.width()) {
        applyResize(1.0, 0, viewport.topLeft(), QSize(100, 100));
        return QRect(viewport.topLeft(), QSize(100, 100));
    }
    QSize currentSize = rotatedImageSize(rotationOffset);
    const bool separatePage = viewport.height() > viewport.width() && m_separationState != NotSeparated;
    if (separatePage) {
        currentSize = QSize(currentSize.width() / 2, currentSize.height());
    }
    const QSize targetSize = currentSize * scale;
    m_drawScale = scale;
    m_displayScale = scale;

    QPoint rotationOffsetPosition = offsetForRotation(rotationOffset);
    rotationOffsetPosition *= scale;

    const int horizontalOffset = horizontalOffsetForAlignment(
        alignment, viewport, targetSize, true);
    const int verticalOffset = qMax(0, (viewport.height() - targetSize.height()) / 2);
    QRect drawRect(QPoint(rotationOffsetPosition.x() + viewport.x() + horizontalOffset, rotationOffsetPosition.y() + verticalOffset), targetSize);

    QPoint imagePos = drawRect.topLeft();
    if (separatePage && ((m_separationState == FirstHalf && qApp->RightSideBook()) || (m_separationState == SecondHalf && !qApp->RightSideBook()))) {
        // Display only the right side of the image
        imagePos.rx() -= targetSize.width();
    }
    applyResize(scale, rotationOffset, imagePos, targetSize, loupe);
    return drawRect;
}

void RenderedPage::setRenderSettings(PageRenderSettings renderSettings)
{
    if (!(m_renderSettings.retouchParameters == renderSettings.retouchParameters)) {
        m_content.resizedImage = QImage();
    }
    m_renderSettings = std::move(renderSettings);
}

static QZimg::FilterMode filterModeForShaderEffect(qvEnums::ShaderEffect effect)
{
    switch (effect) {
    case qvEnums::CpuBicubic:
        return QZimg::ResizeBicubic;
    case qvEnums::CpuSpline16:
        return QZimg::ResizeSpline16;
    case qvEnums::CpuSpline36:
        return QZimg::ResizeSpline36;
    case qvEnums::CpuLanczos3:
        return QZimg::ResizeLanczos3;
    case qvEnums::CpuLanczos4:
        return QZimg::ResizeLanczos4;
    case qvEnums::BilinearAndCpuBicubic:
        return QZimg::ResizeBicubic;
    case qvEnums::BilinearAndCpuSpline16:
        return QZimg::ResizeSpline16;
    case qvEnums::BilinearAndCpuSpline36:
        return QZimg::ResizeSpline36;
    case qvEnums::BilinearAndCpuLanczos:
        return QZimg::ResizeLanczos3;
    default:
        return QZimg::ResizeBicubic;
    }
}

void RenderedPage::applyResize(qreal scale, int rotationOffset, QPoint position, QSize targetSize, bool loupe)
{
    ensureInitialized();
    const int appliedRotation = combinedRotationDegrees(m_rotationDegrees, rotationOffset);
    const QSize resizeTargetSize = appliedRotation % 180
                                       ? QSize(targetSize.height(), targetSize.width())
                                       : targetSize;
    const qvEnums::ShaderEffect effect = m_content.movie.isNull() ? qApp->Effect() : qvEnums::Bilinear;
    QImage &sourceImage = imageWithRetouch();
    const qreal retouchedScale = sourceImage.size() == m_content.loadedImageSize
                                     ? scale
                                     : scale * m_content.loadedImageSize.width() / sourceImage.width();
    // only CPU resizing
    if (effect < qvEnums::UsingFixedShader) {
        if (loupe && !m_content.resizedImage.isNull()) {
            m_content.resizedImage = QImage();
            initializePage(true);
        } else {
            if (m_content.appliedResizeMode != qApp->Effect()) {
                m_content.resizedImage = QImage();
            }
            if (m_content.resizedImage.isNull() || m_content.resizedImage.size() != resizeTargetSize) {
                m_content.appliedResizeMode = qApp->Effect();
                m_content.resizedImage = QZimg::scaled(
                    sourceImage, resizeTargetSize, Qt::IgnoreAspectRatio, filterModeForShaderEffect(qApp->Effect()));
                m_scene->removeItem(m_graphicsItem);
                delete m_graphicsItem;
                m_graphicsItem = m_scene->addPixmap(QPixmap::fromImage(m_content.resizedImage));
                m_graphicsItem->setRotation(m_rotationDegrees);
            }
        }
        m_graphicsItem->setScale(m_content.resizedImage.isNull() ? retouchedScale : 1.0);
    }
    // CPU resizing after GPU preview
    if (effect > qvEnums::UsingCpuResizer && qApp->Effect() < qvEnums::UsingSomeShader) {
        if (!m_content.resizedImage.isNull() && m_content.resizedImage.size() != resizeTargetSize) {
            initializePage(true);
        }
        if (m_content.resizedImage.isNull() && m_resizeGeneratingState == 0) {
            m_resizeGeneratingState = 1;
            QFuture<QImage> future = QtConcurrent::run(
                QZimg::scaled, sourceImage, resizeTargetSize, Qt::IgnoreAspectRatio, filterModeForShaderEffect(qApp->Effect()));
            connect(&m_resizeWatcher, SIGNAL(finished()), this, SLOT(handleResizeFinished()));
            m_resizeWatcher.setFuture(future);
        }
        if (!m_content.resizedImage.isNull() && m_resizeGeneratingState == 2) {
            m_scene->removeItem(m_graphicsItem);
            delete m_graphicsItem;
            m_graphicsItem = m_scene->addPixmap(QPixmap::fromImage(m_content.resizedImage));
            m_graphicsItem->setRotation(m_rotationDegrees);
        }
        m_graphicsItem->setScale(m_content.resizedImage.isNull() ? retouchedScale : 1.0);
    }
    // only GPU resizing
    if ((effect > qvEnums::UsingFixedShader && effect < qvEnums::UsingCpuResizer) || effect > qvEnums::UsingSomeShader) {
        initializePage(true);
        m_graphicsItem->setScale(retouchedScale);
    }
    m_graphicsItem->setRotation(appliedRotation);
    m_graphicsItem->setPos(position);
}

QImage &RenderedPage::imageWithRetouch()
{
#ifndef QV_WITH_LUMINOR
    return m_content.loadedImage;
#else
    const RetouchParameters &params = m_renderSettings.retouchParameters;
    if (m_content.appliedRetouchParameters == params) {
        return params.isDefault() ? m_content.loadedImage : m_content.retouchedImage;
    }
    m_content.resizedImage = QImage();
    m_content.appliedRetouchParameters = params;
    if (!params.isDefault()) {
        m_content.retouchedImage = QLuminor::toLuminor(m_content.loadedImage, params.brightness, params.contrast, params.gamma);
        return m_content.retouchedImage;
    }
    m_content.retouchedImage = QImage();
    return m_content.loadedImage;
#endif
}

void RenderedPage::initializePage(bool resetResizedImage)
{
    if (m_graphicsItem) {
        m_scene->removeItem(m_graphicsItem);
        delete m_graphicsItem;
    }
    if (m_scene) {
        m_graphicsItem = m_scene->addPixmap(QPixmap::fromImage(qApp->Effect() > qvEnums::UsingFixedShader || m_content.resizedImage.isNull() ? imageWithRetouch() : m_content.resizedImage));
        m_graphicsItem->setRotation(m_rotationDegrees);
    }

    if (resetResizedImage) {
        m_content.resizedImage = QImage();
    }
    m_resizeGeneratingState = 0;
}

void RenderedPage::resetSignage(QRect viewport, RenderedPage::PageAlign alignment)
{
    if (m_signageText.isEmpty()) {
        if (m_signageTextItem) {
            m_scene->removeItem(m_signageTextItem);
            delete m_signageTextItem;
            m_signageTextItem = nullptr;
            m_scene->removeItem(m_signageBackgroundItem);
            delete m_signageBackgroundItem;
            m_signageBackgroundItem = nullptr;
        }
        return;
    }
    if (m_signageTextItem) {
        return;
    }
    m_signageTextItem = m_scene->addText(m_signageText);
    m_signageTextItem->setPos(alignment == RenderedPage::PageRight ? viewport.right() - m_signageTextItem->boundingRect().width() : 0, 0);
    m_signageTextItem->setDefaultTextColor(Qt::green);
    m_signageTextItem->setZValue(1);
    QBrush brush(QColor::fromRgb(0, 0, 0, 0x80));
    m_signageBackgroundItem = m_scene->addRect(m_signageTextItem->boundingRect(), Qt::PenStyle::NoPen, brush);
    m_signageBackgroundItem->setPos(m_signageTextItem->pos());
}

void RenderedPage::dispose()
{
    if (m_graphicsItem) {
        m_scene->removeItem(m_graphicsItem);
        delete m_graphicsItem;
        m_graphicsItem = nullptr;
    }
    if (m_signageTextItem) {
        m_scene->removeItem(m_signageTextItem);
        delete m_signageTextItem;
        m_signageTextItem = nullptr;
        m_scene->removeItem(m_signageBackgroundItem);
        delete m_signageBackgroundItem;
        m_signageBackgroundItem = nullptr;
    }
}

void RenderedPage::handleResizeFinished()
{
    m_content.resizedImage = m_resizeWatcher.result();

    m_resizeGeneratingState = 2;
    disconnect(&m_resizeWatcher, SIGNAL(finished()), this, SLOT(handleResizeFinished()));
    emit resizeFinished();
}

void RenderedPage::ensureInitialized()
{
    if (m_initialized) {
        return;
    }
    if (!m_content.movie.isNull()) {
        QMovie *movie = m_content.movie.data();
        connect(movie, SIGNAL(finished()), SLOT(handleAnimationFinished()));
        connect(movie, SIGNAL(frameChanged(int)), SLOT(handleAnimationFrameChanged(int)));
        movie->start();
    }
    m_initialized = true;
}

void RenderedPage::handleAnimationFinished()
{
    QGraphicsPixmapItem *pi = dynamic_cast<QGraphicsPixmapItem *>(m_graphicsItem);
    QMovie *movie = m_content.movie.data();
    movie->stop();
    movie->jumpToFrame(0);
    pi->setPixmap(movie->currentPixmap());
    movie->start();
}

void RenderedPage::handleAnimationFrameChanged(int frameNumber)
{
    QGraphicsPixmapItem *pi = dynamic_cast<QGraphicsPixmapItem *>(m_graphicsItem);
    QMovie *movie = m_content.movie.data();
    movie->jumpToFrame(frameNumber);
    pi->setPixmap(movie->currentPixmap());
}
