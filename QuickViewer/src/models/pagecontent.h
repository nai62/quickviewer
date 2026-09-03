#ifndef PAGECONTENT_H
#define PAGECONTENT_H

#include <utility>

#include <QtCore>
#include <QtWidgets>

#include "exif.h"
#include "qvmovie.h"
#include "qv_init.h"

struct ImageRetouch
{
    float brightness;
    float contrast;
    float gamma;
    ImageRetouch(float brightness = 0.0f, float contrast = 1.0f, float gamma = 1.0f)
        : brightness(brightness),
          contrast(contrast),
          gamma(gamma)
    {}
    bool isDefault() const
    {
        return *this == ImageRetouch();
    }
    bool operator==(const ImageRetouch &rhs) const
    {
        return brightness == rhs.brightness && contrast == rhs.contrast && gamma == rhs.gamma;
    }
};

class PageRenderContext
{
public:
    virtual ~PageRenderContext() = default;
    virtual qreal currentPixelRatio() const = 0;
    virtual ImageRetouch retouchParameters() const = 0;
};

/**
 * @brief The ImageContent struct
 * actual Image data and metadata
 */
struct ImageContent
{
public:
    /**
     * @brief Decoded image used for viewing
     */
    QImage image;
    /**
     * @brief Cached image with brightness, contrast, and gamma adjustments
     */
    QImage retouchedImage;
    /**
     * @brief Cached image resized for the current view
     */
    QImage resizedImage;
    /**
     * @brief Animation data, if the image reader supports animation
     */
    QvMovie movie;
    /**
     * @brief Original dimensions of the image
     */
    QSize originalSize;
    /**
     * @brief Dimensions of the decoded image, which may have been downsampled
     */
    QSize loadedImageSize;
    /**
     * @brief Path of the image
     */
    QString path;
    /**
     * @brief EXIF metadata for JPEG images
     */
    easyexif::EXIFInfo exifInfo;

    size_t fileSize = 0;
    ImageRetouch appliedRetouchParameters;
    qvEnums::ShaderEffect appliedResizeMode = qvEnums::Bilinear;

    ImageContent() = default;
    ImageContent(QString imagePath, size_t size)
        : path(std::move(imagePath)),
          fileSize(size)
    {}
    ImageContent(QImage loadedImage, QString imagePath, QSize sourceSize, easyexif::EXIFInfo metadata, size_t size)
        : image(std::move(loadedImage)),
          originalSize(sourceSize),
          loadedImageSize(image.size()),
          path(std::move(imagePath)),
          exifInfo(std::move(metadata)),
          fileSize(size)
    {}
    bool isLandscape() const { return originalSize.width() > originalSize.height(); }
    void initialize();
};

/**
 * @brief PageItem
 * contains the informations of a Page
 */
class PageItem : public QObject
{
    Q_OBJECT
public:
    enum PageAlign {
        PageCenter,
        PageLeft,
        PageRight
    };
    //    enum FitMode {
    //        NoFitting,
    //        FitToRect,
    //        FitToWidth
    //    };

    enum SeparationState {
        NoSeparated,
        FirstSeparated,
        SecondSeparated
    };

    QGraphicsScene *Scene;
    ImageContent Ic;
    /**
     * @brief GrItem
     * Page image is used as a QGraphicsItem. It will be registered with the scene.
     */
    QGraphicsItem *GrItem;
    //    /**
    //     * @brief Resized
    //     * Store the image changed to the specified size (newsize)
    //     */
    //    QPixmap ResizedPage;
    QFutureWatcher<QImage> generateWatcher;
    /**
     * @brief Rotate: rotation as digrees
     */
    int Rotate;
    /**
     * @brief GText is information as a text on fullscreen
     */
    QString Text;
    QGraphicsTextItem *GText;
    QGraphicsRectItem *GTextSurface;
    /**
     * @brief Actual drawing scale
     */
    qreal DrawScale;
    /**
     * @brief Notational scale
     */
    qreal NotationalScale;
    SeparationState Separation;

    explicit PageItem(QObject *parent = nullptr, const PageRenderContext *renderContext = nullptr);
    PageItem(QObject *parent, QGraphicsScene *s, ImageContent ic, const PageRenderContext *renderContext = nullptr);
    ~PageItem() override;
    Q_DISABLE_COPY_MOVE(PageItem)

    QPoint Offset(int rotateOffset = 0);
    QSize CurrentSize(int rotateOffset = 0);

    /**
     * @brief setPageLayout set each image on the page
     * @param viewport: the image must be inscribed in the viewport area
     */
    QRect setPageLayoutFitting(QRect viewport, PageAlign align, qvEnums::FitMode fitMode, qreal loupe, int rotateOffset = 0);
    QRect setPageLayoutManual(QRect viewport, PageAlign align, qreal scale, int rotateOffset = 0, bool loupe = false);

    void applyResize(qreal scale, int rotateOffset, QPoint pos, QSize newsize, bool loupe = false);
    QImage &applyRetouched();
    void initializePage(bool resetResized = false);
    void resetSignage(QRect viewport, PageItem::PageAlign fitting);
    void resetScene(QGraphicsScene *scene);
    void checkInitialize();
    void dispose();
signals:
    void resizeFinished();
public slots:
    void handleResizeFinished();
    void handleAnimationFrameChanged(int frameNumber);
    void handleAnimationFinished();

private:
    int m_resizeGeneratingState;
    bool initialized;
    // Non-owning. The context must outlive this page item.
    const PageRenderContext *m_renderContext;
};

#endif // PAGECONTENT_H
