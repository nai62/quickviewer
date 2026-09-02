#ifndef PAGECONTENT_H
#define PAGECONTENT_H

#include <QtCore>
#include <QtWidgets>

#include "exif.h"
#include "qvmovie.h"
#include "qv_init.h"

struct ImageRetouch
{
    float Brightness;
    float Contrast;
    float Gamma;
    ImageRetouch(float brightness = 0.0f, float contrast = 1.0f, float gamma = 1.0f)
        : Brightness(brightness),
          Contrast(contrast),
          Gamma(gamma)
    {}
    bool isDefault() const
    {
        return *this == ImageRetouch();
    }
    bool operator==(const ImageRetouch &rhs) const
    {
        return Brightness == rhs.Brightness && Contrast == rhs.Contrast && Gamma == rhs.Gamma;
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
     * @brief Image is a pixmap of the image for viewing
     */
    QImage Image;
    /**
     * @brief RetouchedImage is another view with changed pixels from Image
     */
    QImage RetouchedImage;
    /**
     * @brief ResizedImage is resized to actual view size from Image
     */
    QImage ResizedImage;
    /**
     * @brief Movie will be initialized when imageReader.supportsAnimation() == true
     */
    QvMovie Movie;
    /**
     * @brief BaseSize is original size of the image
     */
    QSize BaseSize;
    /**
     * @brief ImportSize is actual size of image for viewing
     */
    QSize ImportSize;
    /**
     * @brief Path is path of the image
     */
    QString Path;
    /**
     * @brief Info is Exif Information of the image(JPEG only)
     */
    easyexif::EXIFInfo Info;

    size_t FileLength;
    ImageRetouch RetouchParam;
    qvEnums::ShaderEffect ResizeMode;

    ImageContent()
        : FileLength(0),
          ResizeMode(qvEnums::Bilinear)
    {}
    ImageContent(QString path, size_t length)
        : Path(path),
          FileLength(length),
          ResizeMode(qvEnums::Bilinear)
    {}
    ImageContent(QImage image, QString path, QSize size, easyexif::EXIFInfo info, size_t length)
        : Image(image),
          BaseSize(size),
          ImportSize(image.size()),
          Path(path),
          Info(info),
          FileLength(length),
          ResizeMode(qvEnums::Bilinear)
    {}
    ImageContent(const ImageContent &rhs)
        : Image(rhs.Image),
          ResizedImage(rhs.ResizedImage),
          Movie(rhs.Movie),
          BaseSize(rhs.BaseSize),
          ImportSize(rhs.ImportSize),
          Path(rhs.Path),
          Info(rhs.Info),
          FileLength(rhs.FileLength),
          ResizeMode(rhs.ResizeMode)
    {}
    inline ImageContent &operator=(const ImageContent &rhs)
    {
        Image = rhs.Image;
        ResizedImage = rhs.ResizedImage;
        Movie = rhs.Movie;
        Path = rhs.Path;
        BaseSize = rhs.BaseSize;
        ImportSize = rhs.ImportSize;
        Info = rhs.Info;
        FileLength = rhs.FileLength;
        ResizeMode = rhs.ResizeMode;
        return *this;
    }
    bool wideImage() const { return BaseSize.width() > BaseSize.height(); }
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
