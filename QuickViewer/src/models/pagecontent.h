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
 * @brief Decoded image data, metadata, and derived rendering caches
 */
struct ImageContent
{
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
    void initializeAnimation();
};

/**
 * @brief Rendered state for a page
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
        NotSeparated,
        FirstHalf,
        SecondHalf
    };

    QGraphicsScene *scene;
    ImageContent content;
    /**
     * @brief Graphics item registered with the scene for this page
     */
    QGraphicsItem *graphicsItem;
    /**
     * @brief Rotation in degrees from the decoded image orientation
     */
    int rotationDegrees;
    /**
     * @brief Fullscreen signage text
     */
    QString signageText;
    QGraphicsTextItem *signageTextItem;
    QGraphicsRectItem *signageBackgroundItem;
    /**
     * @brief Actual drawing scale
     */
    qreal drawScale;
    /**
     * @brief Scale displayed to the user
     */
    qreal displayScale;
    SeparationState separationState;

    explicit PageItem(QObject *parent = nullptr, const PageRenderContext *renderContext = nullptr);
    PageItem(QObject *parent, QGraphicsScene *graphicsScene, ImageContent imageContent, const PageRenderContext *renderContext = nullptr);
    ~PageItem() override;
    Q_DISABLE_COPY_MOVE(PageItem)

    QPoint offsetForRotation(int rotationOffset = 0) const;
    QSize rotatedImageSize(int rotationOffset = 0) const;

    /**
     * @brief Lay out an image fitted within the viewport
     */
    QRect setPageLayoutFitting(QRect viewport, PageAlign alignment, qvEnums::FitMode fitMode, qreal loupe, int rotationOffset = 0);
    QRect setPageLayoutManual(QRect viewport, PageAlign alignment, qreal scale, int rotationOffset = 0, bool loupe = false);

    void applyResize(qreal scale, int rotationOffset, QPoint position, QSize targetSize, bool loupe = false);
    void initializePage(bool resetResizedImage = false);
    void resetSignage(QRect viewport, PageItem::PageAlign alignment);
signals:
    void resizeFinished();
public slots:
    void handleResizeFinished();
    void handleAnimationFrameChanged(int frameNumber);
    void handleAnimationFinished();

private:
    QImage &imageWithRetouch();
    void ensureInitialized();
    void dispose();

    QFutureWatcher<QImage> m_resizeWatcher;
    int m_resizeGeneratingState;
    bool m_initialized;
    // Non-owning. The context must outlive this page item.
    const PageRenderContext *m_renderContext;
};

#endif // PAGECONTENT_H
