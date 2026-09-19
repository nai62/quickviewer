#ifndef QZIMG_H
#define QZIMG_H

#include <QtGui>

class QZimgPrivate;

class QZimg : public QObject
{
    Q_OBJECT
public:
    enum FilterMode {
        ResizePoint,
        ResizeBicubic,
        ResizeSpline16,
        ResizeSpline36,
        ResizeLanczos3,
        ResizeLanczos4,
    };
#if QT_VERSION_MAJOR >= 5
    Q_ENUM(FilterMode)
#endif

    explicit QZimg(QObject *parent = 0);
    ~QZimg();

    /**
     * @brief Creates the image the resampler writes into. stridePack is unused:
     * the resampler reads and writes QImages through their own accessors, and
     * zimg allocates its aligned working buffers itself.
     * @param size of new image
     * @param fmt of new image
     */
    static QImage createPackedImage(QSize size, QImage::Format fmt, int stridePack = 64);

    /**
     * @brief Returns an image in one of the 4 byte formats the resampler accepts:
     * ARGB32 and RGB32 are returned unchanged, anything else is converted.
     * stridePack is unused, as in createPackedImage.
     */
    static QImage toPackedImage(const QImage &src, int stridePack = 64);

    /**
     * @brief Scales src to newsize, or returns a null image when src is null.
     */
    static QImage scaled(const QImage &src,
                         const QSize &s,
                         Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio,
                         FilterMode mode = ResizeBicubic);

signals:

public slots:
private:
    QZimgPrivate *d;
};

#endif // QZIMG_H
