#include <QtConcurrent>

#include "pagecontent.h"
#include "qvapplication.h"
#include "qzimg.h"

#ifdef QV_WITH_LUMINOR
#    include "qluminor.h"
#endif

static int horizontalOffsetForAlignment(
    PageItem::PageAlign alignment,
    const QRect &viewport,
    const QSize &contentSize,
    bool clampCenteredOffset = false)
{
    if (alignment == PageItem::PageRight) {
        return 0;
    }
    const int offset = viewport.width() - contentSize.width();
    if (alignment == PageItem::PageCenter) {
        const int centeredOffset = offset / 2;
        return clampCenteredOffset ? qMax(0, centeredOffset) : centeredOffset;
    }
    return offset;
}

static int normalizedRotationDegrees(int rotationDegrees)
{
    const int remainder = rotationDegrees % 360;
    return remainder < 0 ? remainder + 360 : remainder;
}

static int combinedRotationDegrees(int baseRotationDegrees, int rotationOffset)
{
    const int combined = normalizedRotationDegrees(baseRotationDegrees) + normalizedRotationDegrees(rotationOffset);
    return combined >= 360 ? combined - 360 : combined;
}

void ImageContent::initializeAnimation()
{
    if (!movie.isNull() && !movie.data()) {
        movie.load();
        QMovie *qm = movie.data();
        qm->jumpToFrame(0);
        QPixmap firstFrame = qm->currentPixmap();
        loadedImage = firstFrame.toImage();
        originalSize = loadedImageSize = firstFrame.size();
    }
}

PageItem::PageItem(QObject *parent, PageRenderSettings renderSettings)
    : QObject(parent),
      scene(nullptr),
      content(),
      graphicsItem(nullptr),
      rotationDegrees(0),
      signageText(),
      signageTextItem(nullptr),
      signageBackgroundItem(nullptr),
      drawScale(1.0),
      displayScale(1.0),
      separationState(NotSeparated),
      m_resizeGeneratingState(0),
      m_initialized(false),
      m_renderSettings(std::move(renderSettings))
{
}

PageItem::PageItem(QObject *parent, QGraphicsScene *graphicsScene, ImageContent imageContent, PageRenderSettings renderSettings)
    : QObject(parent),
      scene(graphicsScene),
      content(std::move(imageContent)),
      graphicsItem(nullptr),
      rotationDegrees(0),
      signageText(),
      signageTextItem(nullptr),
      signageBackgroundItem(nullptr),
      drawScale(1.0),
      displayScale(1.0),
      separationState(content.isLandscape() && qApp->SeparatePagesWhenWideImage() ? FirstHalf : NotSeparated),
      m_resizeGeneratingState(0),
      m_initialized(false),
      m_renderSettings(std::move(renderSettings))
{
    if (!content.loadedImageSize.width()) {
        QGraphicsTextItem *errorTextItem = scene->addText(tr("NOT IMAGE FILE", "Error messages to be displayed on screen when image loading fails"));
        errorTextItem->setDefaultTextColor(Qt::white);
        graphicsItem = errorTextItem;
        return;
    }
    if (content.exifInfo.ImageWidth > 0 && content.exifInfo.Orientation != 1) {
        switch (content.exifInfo.Orientation) {
        case 6: // 90 degrees clockwise
            rotationDegrees = 90;
            break;
        case 8: // 90 degrees counterclockwise
            rotationDegrees = 270;
            break;
        }
    }
    initializePage();
}

PageItem::~PageItem()
{
    dispose();
}

QPoint PageItem::offsetForRotation(int rotationOffset) const
{
    switch (combinedRotationDegrees(rotationDegrees, rotationOffset)) {
    case 90:
        return QPoint(content.loadedImage.height(), 0);
    case 180:
        return QPoint(content.loadedImage.width(), content.loadedImage.height());
    case 270:
        return QPoint(0, content.loadedImage.width());
    default:
        return QPoint();
    }
}

QSize PageItem::rotatedImageSize(int rotationOffset) const
{
    const int rotation = combinedRotationDegrees(rotationDegrees, rotationOffset);
    return rotation == 90 || rotation == 270
               ? QSize(content.loadedImage.height(), content.loadedImage.width())
               : content.loadedImage.size();
}

QRect PageItem::setPageLayoutFitting(QRect viewport, PageItem::PageAlign alignment, qvEnums::FitMode fitMode, qreal loupe, int rotationOffset)
{
    QRect viewport1 = viewport;
    const qreal pixelRatio = m_renderSettings.pixelRatio;
    if (pixelRatio != 1.0) {
        // Compensate for the world transform used on high-DPI displays.
        viewport1 = QRect(viewport.left() * pixelRatio, viewport.top() * pixelRatio, viewport.width() * pixelRatio, viewport.height() * pixelRatio);
    }

    if (!content.loadedImageSize.width()) {
        applyResize(1.0, 0, viewport.topLeft(), QSize(100, 100));
        return QRect(viewport.topLeft(), QSize(100, 100));
    }
    QSize currentSize = rotatedImageSize(rotationOffset);
    const bool separatePage = viewport.height() > viewport1.width() && separationState != NotSeparated;
    if (separatePage) {
        currentSize = QSize(currentSize.width() / 2, currentSize.height());
    }
    const QSize targetSize = fitMode == qvEnums::FitToRect
                                 ? currentSize.scaled(viewport1.size(), Qt::KeepAspectRatio)
                                 : QSize(viewport1.width(), currentSize.height() * viewport1.width() / currentSize.width());
    const qreal scale = drawScale = 1.0 * targetSize.width() / currentSize.width();
    displayScale = scale;
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
    if (separatePage && ((separationState == FirstHalf && qApp->RightSideBook()) || (separationState == SecondHalf && !qApp->RightSideBook()))) {
        // Display only the right side of the image
        imagePos.rx() -= targetSize.width();
    }
    applyResize(scale, rotationOffset, imagePos, targetSize);
    return drawRect;
}

QRect PageItem::setPageLayoutManual(QRect viewport, PageItem::PageAlign alignment, qreal scale, int rotationOffset, bool loupe)
{
    if (!content.loadedImageSize.width()) {
        applyResize(1.0, 0, viewport.topLeft(), QSize(100, 100));
        return QRect(viewport.topLeft(), QSize(100, 100));
    }
    QSize currentSize = rotatedImageSize(rotationOffset);
    const bool separatePage = viewport.height() > viewport.width() && separationState != NotSeparated;
    if (separatePage) {
        currentSize = QSize(currentSize.width() / 2, currentSize.height());
    }
    const QSize targetSize = currentSize * scale;
    drawScale = scale;
    displayScale = scale;

    QPoint rotationOffsetPosition = offsetForRotation(rotationOffset);
    rotationOffsetPosition *= scale;

    const int horizontalOffset = horizontalOffsetForAlignment(
        alignment, viewport, targetSize, true);
    const int verticalOffset = qMax(0, (viewport.height() - targetSize.height()) / 2);
    QRect drawRect(QPoint(rotationOffsetPosition.x() + viewport.x() + horizontalOffset, rotationOffsetPosition.y() + verticalOffset), targetSize);

    QPoint imagePos = drawRect.topLeft();
    if (separatePage && ((separationState == FirstHalf && qApp->RightSideBook()) || (separationState == SecondHalf && !qApp->RightSideBook()))) {
        // Display only the right side of the image
        imagePos.rx() -= targetSize.width();
    }
    applyResize(scale, rotationOffset, imagePos, targetSize, loupe);
    return drawRect;
}

void PageItem::setRenderSettings(PageRenderSettings renderSettings)
{
    if (!(m_renderSettings.retouchParameters == renderSettings.retouchParameters)) {
        content.resizedImage = QImage();
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

void PageItem::applyResize(qreal scale, int rotationOffset, QPoint position, QSize targetSize, bool loupe)
{
    ensureInitialized();
    const int appliedRotation = combinedRotationDegrees(rotationDegrees, rotationOffset);
    const QSize resizeTargetSize = appliedRotation % 180
                                       ? QSize(targetSize.height(), targetSize.width())
                                       : targetSize;
    const qvEnums::ShaderEffect effect = content.movie.isNull() ? qApp->Effect() : qvEnums::Bilinear;
    QImage &sourceImage = imageWithRetouch();
    const qreal retouchedScale = sourceImage.size() == content.loadedImageSize
                                     ? scale
                                     : scale * content.loadedImageSize.width() / sourceImage.width();
    // only CPU resizing
    if (effect < qvEnums::UsingFixedShader) {
        if (loupe && !content.resizedImage.isNull()) {
            content.resizedImage = QImage();
            initializePage(true);
        } else {
            if (content.appliedResizeMode != qApp->Effect()) {
                content.resizedImage = QImage();
            }
            if (content.resizedImage.isNull() || content.resizedImage.size() != resizeTargetSize) {
                content.appliedResizeMode = qApp->Effect();
                content.resizedImage = QZimg::scaled(
                    sourceImage, resizeTargetSize, Qt::IgnoreAspectRatio, filterModeForShaderEffect(qApp->Effect()));
                scene->removeItem(graphicsItem);
                delete graphicsItem;
                graphicsItem = scene->addPixmap(QPixmap::fromImage(content.resizedImage));
                graphicsItem->setRotation(rotationDegrees);
            }
        }
        graphicsItem->setScale(content.resizedImage.isNull() ? retouchedScale : 1.0);
    }
    // CPU resizing after GPU preview
    if (effect > qvEnums::UsingCpuResizer && qApp->Effect() < qvEnums::UsingSomeShader) {
        if (!content.resizedImage.isNull() && content.resizedImage.size() != resizeTargetSize) {
            initializePage(true);
        }
        if (content.resizedImage.isNull() && m_resizeGeneratingState == 0) {
            m_resizeGeneratingState = 1;
            QFuture<QImage> future = QtConcurrent::run(
                QZimg::scaled, sourceImage, resizeTargetSize, Qt::IgnoreAspectRatio, filterModeForShaderEffect(qApp->Effect()));
            connect(&m_resizeWatcher, SIGNAL(finished()), this, SLOT(handleResizeFinished()));
            m_resizeWatcher.setFuture(future);
        }
        if (!content.resizedImage.isNull() && m_resizeGeneratingState == 2) {
            scene->removeItem(graphicsItem);
            delete graphicsItem;
            graphicsItem = scene->addPixmap(QPixmap::fromImage(content.resizedImage));
            graphicsItem->setRotation(rotationDegrees);
        }
        graphicsItem->setScale(content.resizedImage.isNull() ? retouchedScale : 1.0);
    }
    // only GPU resizing
    if ((effect > qvEnums::UsingFixedShader && effect < qvEnums::UsingCpuResizer) || effect > qvEnums::UsingSomeShader) {
        initializePage(true);
        graphicsItem->setScale(retouchedScale);
    }
    graphicsItem->setRotation(appliedRotation);
    graphicsItem->setPos(position);
}

QImage &PageItem::imageWithRetouch()
{
#ifndef QV_WITH_LUMINOR
    return content.loadedImage;
#else
    const RetouchParameters &params = m_renderSettings.retouchParameters;
    if (content.appliedRetouchParameters == params) {
        return params.isDefault() ? content.loadedImage : content.retouchedImage;
    }
    content.resizedImage = QImage();
    content.appliedRetouchParameters = params;
    if (!params.isDefault()) {
        content.retouchedImage = QLuminor::toLuminor(content.loadedImage, params.brightness, params.contrast, params.gamma);
        return content.retouchedImage;
    }
    content.retouchedImage = QImage();
    return content.loadedImage;
#endif
}

void PageItem::initializePage(bool resetResizedImage)
{
    if (graphicsItem) {
        scene->removeItem(graphicsItem);
        delete graphicsItem;
    }
    if (scene) {
        graphicsItem = scene->addPixmap(QPixmap::fromImage(qApp->Effect() > qvEnums::UsingFixedShader || content.resizedImage.isNull() ? imageWithRetouch() : content.resizedImage));
        graphicsItem->setRotation(rotationDegrees);
    }

    if (resetResizedImage) {
        content.resizedImage = QImage();
    }
    m_resizeGeneratingState = 0;
}

void PageItem::resetSignage(QRect viewport, PageItem::PageAlign alignment)
{
    if (signageText.isEmpty()) {
        if (signageTextItem) {
            scene->removeItem(signageTextItem);
            delete signageTextItem;
            signageTextItem = nullptr;
            scene->removeItem(signageBackgroundItem);
            delete signageBackgroundItem;
            signageBackgroundItem = nullptr;
        }
        return;
    }
    if (signageTextItem) {
        return;
    }
    signageTextItem = scene->addText(signageText);
    signageTextItem->setPos(alignment == PageItem::PageRight ? viewport.right() - signageTextItem->boundingRect().width() : 0, 0);
    signageTextItem->setDefaultTextColor(Qt::green);
    signageTextItem->setZValue(1);
    QBrush brush(QColor::fromRgb(0, 0, 0, 0x80));
    signageBackgroundItem = scene->addRect(signageTextItem->boundingRect(), Qt::PenStyle::NoPen, brush);
    signageBackgroundItem->setPos(signageTextItem->pos());
}

void PageItem::dispose()
{
    if (graphicsItem) {
        scene->removeItem(graphicsItem);
        delete graphicsItem;
        graphicsItem = nullptr;
    }
    if (signageTextItem) {
        scene->removeItem(signageTextItem);
        delete signageTextItem;
        signageTextItem = nullptr;
        scene->removeItem(signageBackgroundItem);
        delete signageBackgroundItem;
        signageBackgroundItem = nullptr;
    }
}

void PageItem::handleResizeFinished()
{
    content.resizedImage = m_resizeWatcher.result();

    m_resizeGeneratingState = 2;
    disconnect(&m_resizeWatcher, SIGNAL(finished()), this, SLOT(handleResizeFinished()));
    emit resizeFinished();
}

void PageItem::ensureInitialized()
{
    if (m_initialized) {
        return;
    }
    if (!content.movie.isNull()) {
        QMovie *movie = content.movie.data();
        connect(movie, SIGNAL(finished()), SLOT(handleAnimationFinished()));
        connect(movie, SIGNAL(frameChanged(int)), SLOT(handleAnimationFrameChanged(int)));
        movie->start();
    }
    m_initialized = true;
}

void PageItem::handleAnimationFinished()
{
    QGraphicsPixmapItem *pi = dynamic_cast<QGraphicsPixmapItem *>(graphicsItem);
    QMovie *movie = content.movie.data();
    movie->stop();
    movie->jumpToFrame(0);
    pi->setPixmap(movie->currentPixmap());
    movie->start();
}

void PageItem::handleAnimationFrameChanged(int frameNumber)
{
    //    qDebug() << frameNumber;
    QGraphicsPixmapItem *pi = dynamic_cast<QGraphicsPixmapItem *>(graphicsItem);
    QMovie *movie = content.movie.data();
    movie->jumpToFrame(frameNumber);
    pi->setPixmap(movie->currentPixmap());
}
