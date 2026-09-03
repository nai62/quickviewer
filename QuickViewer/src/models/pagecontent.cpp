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

void ImageContent::initialize()
{
    if (!movie.isNull() && !movie.data()) {
        movie.load();
        QMovie *qm = movie.data();
        qm->jumpToFrame(0);
        QPixmap firstFrame = qm->currentPixmap();
        image = firstFrame.toImage();
        originalSize = loadedImageSize = firstFrame.size();
    }
}

PageItem::PageItem(QObject *parent, const PageRenderContext *renderContext)
    : QObject(parent),
      Scene(nullptr),
      Ic(),
      GrItem(nullptr),
      Rotate(0),
      Text(),
      GText(nullptr),
      GTextSurface(nullptr),
      DrawScale(1.0),
      NotationalScale(1.0),
      Separation(NoSeparated),
      m_resizeGeneratingState(0),
      initialized(false),
      m_renderContext(renderContext)
{
}

PageItem::PageItem(QObject *parent, QGraphicsScene *s, ImageContent ic, const PageRenderContext *renderContext)
    : QObject(parent),
      Scene(s),
      Ic(ic),
      GrItem(nullptr),
      Rotate(0),
      Text(),
      GText(nullptr),
      GTextSurface(nullptr),
      DrawScale(1.0),
      NotationalScale(1.0),
      Separation(Ic.isLandscape() && qApp->SeparatePagesWhenWideImage() ? FirstSeparated : NoSeparated),
      m_resizeGeneratingState(0),
      initialized(false),
      m_renderContext(renderContext)
{
    //    qDebug() << Ic.isLandscape() << qApp->SeparatePagesWhenWideImage();
    if (!Ic.loadedImageSize.width()) {
        QGraphicsTextItem *gtext = s->addText(tr("NOT IMAGE FILE", "Error messages to be displayed on screen when image loading fails"));
        gtext->setDefaultTextColor(Qt::white);
        //qDebug() << gtext;
        GrItem = gtext;
        return;
    }
    if (ic.exifInfo.ImageWidth > 0 && ic.exifInfo.Orientation != 1) {
        switch (ic.exifInfo.Orientation) {
        case 6: // left 90 digree turned
            Rotate = 90;
            break;
        case 8: // right 90 digree turned
            Rotate = 270;
            break;
        }
    }
    initializePage();
}

PageItem::~PageItem()
{
    dispose();
}

QPoint PageItem::Offset(int rotateOffset)
{
    int rot = (Rotate + rotateOffset) % 360;
    switch (rot) {
    case 90:
        return QPoint(Ic.image.height(), 0);
    case 180:
        return QPoint(Ic.image.width(), Ic.image.height());
    case 270:
        return QPoint(0, Ic.image.width());
    default:
        return QPoint();
    }
}

QSize PageItem::CurrentSize(int rotateOffset)
{
    int rot = (Rotate + rotateOffset) % 360;
    return rot == 90 || rot == 270 ? QSize(Ic.image.height(), Ic.image.width()) : Ic.image.size();
}

QRect PageItem::setPageLayoutFitting(QRect viewport, PageItem::PageAlign align, qvEnums::FitMode fitMode, qreal loupe, int rotateOffset)
{
    QRect viewport1 = viewport;
    const qreal pixelRatio = m_renderContext ? m_renderContext->currentPixelRatio() : 1.0;
    if (pixelRatio != 1.0) {
        // If pixelRatio > 1, you are changing the worldTransport and need to compensate the viewport
        viewport1 = QRect(viewport.left() * pixelRatio, viewport.top() * pixelRatio, viewport.width() * pixelRatio, viewport.height() * pixelRatio);
    }

    if (!Ic.loadedImageSize.width()) {
        applyResize(1.0, 0, viewport.topLeft(), QSize(100, 100));
        return QRect(viewport.topLeft(), QSize(100, 100));
    }
    QSize currentSize = CurrentSize(rotateOffset);
    bool goSeparate = viewport.height() > viewport1.width() && Separation != NoSeparated;
    if (goSeparate) {
        currentSize = QSize(currentSize.width() / 2, currentSize.height());
    }
    QSize newsize = fitMode == qvEnums::FitToRect
                        ? currentSize.scaled(viewport1.size(), Qt::KeepAspectRatio)
                        : QSize(viewport1.width(), currentSize.height() * viewport1.width() / currentSize.width());
    qreal scale = DrawScale = 1.0 * newsize.width() / currentSize.width();
    NotationalScale = scale;
    if (loupe > 1.0) {
        return setPageLayoutManual(viewport, align, scale * loupe, rotateOffset, true);
    }
    if (scale > 1.0 && qApp->DontEnlargeSmallImagesOnFitting()) {
        return setPageLayoutManual(viewport, align, 1.0, rotateOffset);
    }

    QPoint of = Offset(rotateOffset);
    of *= scale;

    QRect drawRect;
    if (fitMode == qvEnums::FitToRect) {
        if (newsize.height() == viewport1.height()) { // fitting on upper and bottom
            const int ofsinviewport = horizontalOffsetForAlignment(
                align, viewport, newsize);
            drawRect = QRect(QPoint(of.x() + viewport.x() + ofsinviewport, of.y()), newsize);
        } else { // fitting on left and right
            const int ofsinviewportX = horizontalOffsetForAlignment(
                align, viewport, newsize);
            int ofsinviewportY = (viewport.height() - newsize.height()) / 2;
            ofsinviewportY = pixelRatio != 1.0 ? 0 : ofsinviewportY; // If pixelRatio > 1, no correction is required
            drawRect = QRect(QPoint(of.x() + viewport.x() + ofsinviewportX, of.y() + ofsinviewportY), newsize);
        }
    } else {
        if (viewport.height() < newsize.height() && newsize.height() < viewport1.height()) {
            // Display magnification is automatically corrected, so special correction is required.
            const int ofsinviewportX = horizontalOffsetForAlignment(
                align, viewport, newsize);
            int ofsinviewportY = pixelRatio == 1.0 ? 0 : (-viewport1.height() + newsize.height()) / 2;
            drawRect = QRect(QPoint(of.x() + viewport.x() + ofsinviewportX, of.y() + ofsinviewportY), newsize);
        } else {
            const int ofsinviewportX = horizontalOffsetForAlignment(
                align, viewport, newsize);
            int ofsinviewportY = pixelRatio == 1.0 ? 0 : (viewport.height() - viewport1.height()) / 2;
            drawRect = QRect(QPoint(of.x() + viewport.x() + ofsinviewportX, of.y() + ofsinviewportY), newsize);
        }
    }

    QPoint imagePos = drawRect.topLeft();
    if (goSeparate && ((Separation == FirstSeparated && qApp->RightSideBook()) || (Separation == SecondSeparated && !qApp->RightSideBook()))) {
        // Display only the right side of the image
        imagePos = QPoint(imagePos.x() - newsize.width(), imagePos.y());
    }
    applyResize(scale, rotateOffset, imagePos, newsize);
    return drawRect;
}

QRect PageItem::setPageLayoutManual(QRect viewport, PageItem::PageAlign align, qreal scale, int rotateOffset, bool loupe)
{
    if (!Ic.loadedImageSize.width()) {
        applyResize(1.0, 0, viewport.topLeft(), QSize(100, 100));
        return QRect(viewport.topLeft(), QSize(100, 100));
    }
    QSize currentSize = CurrentSize(rotateOffset);
    bool goSeparate = viewport.height() > viewport.width() && Separation != NoSeparated;
    if (goSeparate) {
        currentSize = QSize(currentSize.width() / 2, currentSize.height());
    }
    QSize newsize = currentSize * scale;
    DrawScale = scale;
    NotationalScale = scale;

    QPoint of = Offset(rotateOffset);
    of *= scale;

    const int ofsinviewport = horizontalOffsetForAlignment(
        align, viewport, newsize, true);
    int offsetY = qMax(0, (viewport.height() - newsize.height()) / 2);
    QRect drawRect(QPoint(of.x() + viewport.x() + ofsinviewport, of.y() + offsetY), newsize);

    QPoint imagePos = drawRect.topLeft();
    if (goSeparate && ((Separation == FirstSeparated && qApp->RightSideBook()) || (Separation == SecondSeparated && !qApp->RightSideBook()))) {
        // Display only the right side of the image
        imagePos = QPoint(imagePos.x() - newsize.width(), imagePos.y());
    }
    applyResize(scale, rotateOffset, imagePos, newsize, loupe);
    return drawRect;
}

static QZimg::FilterMode ShaderEffect2FilterMode(qvEnums::ShaderEffect effect)
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

void PageItem::applyResize(qreal scale, int rotateOffset, QPoint pos, QSize newsize, bool loupe)
{
    checkInitialize();
    //    QSize newsize2 = Ic.exifInfo.Orientation==6 || Ic.exifInfo.Orientation==8 ? QSize(newsize.height(), newsize.width()) : newsize;
    QSize newsize2 = (Rotate + rotateOffset) % 180 ? QSize(newsize.height(), newsize.width()) : newsize;
    qvEnums::ShaderEffect effect = Ic.movie.isNull() ? qApp->Effect() : qvEnums::Bilinear;
    QImage &srcImage = applyRetouched();
    qreal retouchedScale = srcImage.size() == Ic.loadedImageSize ? scale : scale * Ic.loadedImageSize.width() / srcImage.width();
    // only CPU resizing
    if (effect < qvEnums::UsingFixedShader) {
        if (loupe && !Ic.resizedImage.isNull()) {
            Ic.resizedImage = QImage();
            initializePage(true);
        } else {
            if (Ic.appliedResizeMode != qApp->Effect()) {
                Ic.resizedImage = QImage();
            }
            if (Ic.resizedImage.isNull() || (!Ic.resizedImage.isNull() && Ic.resizedImage.size() != newsize2)) {
                Ic.appliedResizeMode = qApp->Effect();
                QImage resized = QZimg::scaled(srcImage, newsize2, Qt::IgnoreAspectRatio, ShaderEffect2FilterMode(qApp->Effect()));
                Ic.resizedImage = resized;
                Scene->removeItem(GrItem);
                delete GrItem;
                GrItem = Scene->addPixmap(QPixmap::fromImage(Ic.resizedImage));
                GrItem->setRotation(Rotate);
            }
        }
        GrItem->setScale(Ic.resizedImage.isNull() ? retouchedScale : 1.0);
    }
    // CPU resizing after GPU preview
    if (effect > qvEnums::UsingCpuResizer && qApp->Effect() < qvEnums::UsingSomeShader) {
        if (!Ic.resizedImage.isNull() && Ic.resizedImage.size() != newsize) {
            initializePage(true);
        }
        if (Ic.resizedImage.isNull() && m_resizeGeneratingState == 0) {
            m_resizeGeneratingState = 1;
            QFuture<QImage> future = QtConcurrent::run(QZimg::scaled, srcImage, newsize2, Qt::IgnoreAspectRatio, ShaderEffect2FilterMode(qApp->Effect()));
            connect(&generateWatcher, SIGNAL(finished()), this, SLOT(handleResizeFinished()));
            generateWatcher.setFuture(future);
        }
        if (!Ic.resizedImage.isNull() && m_resizeGeneratingState == 2) {
            Scene->removeItem(GrItem);
            delete GrItem;
            GrItem = Scene->addPixmap(QPixmap::fromImage(Ic.resizedImage));
            GrItem->setRotation(Rotate);
        }
        GrItem->setScale(Ic.resizedImage.isNull() ? retouchedScale : 1.0);
    }
    // only GPU resizing
    if ((effect > qvEnums::UsingFixedShader && effect < qvEnums::UsingCpuResizer) || effect > qvEnums::UsingSomeShader) {
        //        if(!Ic.resizedImage.isNull())
        //            initializePage(true);
        initializePage(true);
        GrItem->setScale(retouchedScale);
    }
    GrItem->setRotation(Rotate + rotateOffset);
    GrItem->setPos(pos);
}

QImage &PageItem::applyRetouched()
{
#ifndef QV_WITH_LUMINOR
    return Ic.image;
#else
    const ImageRetouch params = m_renderContext ? m_renderContext->retouchParameters() : ImageRetouch();
    if (Ic.appliedRetouchParameters == params) {
        return params.isDefault() ? Ic.image : Ic.retouchedImage;
    }
    Ic.resizedImage = QImage();
    Ic.appliedRetouchParameters = params;
    if (!params.isDefault()) {
        Ic.retouchedImage = QLuminor::toLuminor(Ic.image, params.brightness, params.contrast, params.gamma);
        return Ic.retouchedImage;
    } else {
        Ic.retouchedImage = QImage();
        return Ic.image;
    }
#endif
}

void PageItem::initializePage(bool resetResized)
{
    if (GrItem) {
        Scene->removeItem(GrItem);
        delete GrItem;
    }
    if (Scene) {
        GrItem = Scene->addPixmap(QPixmap::fromImage(qApp->Effect() > qvEnums::UsingFixedShader || Ic.resizedImage.isNull() ? applyRetouched() : Ic.resizedImage));
        GrItem->setRotation(Rotate);
    }

    if (resetResized) {
        Ic.resizedImage = QImage();
    }
    m_resizeGeneratingState = 0;
}

void PageItem::resetSignage(QRect viewport, PageItem::PageAlign fitting)
{
    if (Text.isEmpty()) {
        if (GText) {
            Scene->removeItem(GText);
            delete GText;
            GText = nullptr;
            Scene->removeItem(GTextSurface);
            delete GTextSurface;
            GTextSurface = nullptr;
        }
        return;
    }
    if (GText) {
        return;
    }
    GText = Scene->addText(Text);
    GText->setPos(fitting == PageItem::PageRight ? viewport.right() - GText->boundingRect().width() : 0, 0);
    //qDebug() << GText->pos() << Text;
    GText->setDefaultTextColor(Qt::green);
    GText->setZValue(1);
    QBrush brush(QColor::fromRgb(0, 0, 0, 0x80));
    GTextSurface = Scene->addRect(GText->boundingRect(), Qt::PenStyle::NoPen, brush);
    GTextSurface->setPos(GText->pos());
}

void PageItem::dispose()
{
    if (GrItem) {
        Scene->removeItem(GrItem);
        delete GrItem;
        GrItem = nullptr;
    }
    if (GText) {
        Scene->removeItem(GText);
        delete GText;
        GText = nullptr;
        Scene->removeItem(GTextSurface);
        delete GTextSurface;
        GTextSurface = nullptr;
    }
}

void PageItem::resetScene(QGraphicsScene *)
{
}

void PageItem::handleResizeFinished()
{
    Ic.resizedImage = generateWatcher.result();

    m_resizeGeneratingState = 2;
    disconnect(&generateWatcher, SIGNAL(finished()), this, SLOT(handleResizeFinished()));
    emit resizeFinished();
}

void PageItem::checkInitialize()
{
    if (initialized) {
        return;
    }
    if (!Ic.movie.isNull()) {
        QMovie *movie = Ic.movie.data();
        connect(movie, SIGNAL(finished()), SLOT(handleAnimationFinished()));
        connect(movie, SIGNAL(frameChanged(int)), SLOT(handleAnimationFrameChanged(int)));
        movie->start();
    }
    initialized = true;
}

void PageItem::handleAnimationFinished()
{
    QGraphicsPixmapItem *pi = dynamic_cast<QGraphicsPixmapItem *>(GrItem);
    QMovie *movie = Ic.movie.data();
    movie->stop();
    movie->jumpToFrame(0);
    pi->setPixmap(movie->currentPixmap());
    movie->start();
}

void PageItem::handleAnimationFrameChanged(int frameNumber)
{
    //    qDebug() << frameNumber;
    QGraphicsPixmapItem *pi = dynamic_cast<QGraphicsPixmapItem *>(GrItem);
    QMovie *movie = Ic.movie.data();
    movie->jumpToFrame(frameNumber);
    pi->setPixmap(movie->currentPixmap());
}
