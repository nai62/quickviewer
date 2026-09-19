#ifndef IMAGECONTENT_H
#define IMAGECONTENT_H

#include <utility>

#include <QtCore>
#include <QtGui>

#include "exif.h"
#include "movie.h"
#include "qvenums.h"

struct RetouchParameters
{
    float brightness;
    float contrast;
    float gamma;
    RetouchParameters(float brightness = 0.0f, float contrast = 1.0f, float gamma = 1.0f)
        : brightness(brightness),
          contrast(contrast),
          gamma(gamma)
    {
    }
    bool isDefault() const { return *this == RetouchParameters(); }
    bool operator==(const RetouchParameters &rhs) const
    {
        return brightness == rhs.brightness && contrast == rhs.contrast && gamma == rhs.gamma;
    }
};

/**
 * @brief Decoded image data, metadata, and derived rendering caches
 */
struct ImageContent
{
    QImage loadedImage;
    QImage retouchedImage;
    QImage resizedImage;
    Movie movie;
    QSize originalSize;
    QSize loadedImageSize;
    QString path;
    easyexif::EXIFInfo exifInfo;
    size_t fileSize = 0;
    bool isPreview = false;
    bool hasDetailedMetadata = false;
    RetouchParameters appliedRetouchParameters;
    qvEnums::ShaderEffect appliedResizeMode = qvEnums::ShaderEffect::Bilinear;

    ImageContent() = default;
    ImageContent(QString imagePath, size_t size)
        : path(std::move(imagePath)),
          fileSize(size)
    {
    }
    ImageContent(
        QImage image, QString imagePath, QSize sourceSize, easyexif::EXIFInfo metadata, size_t size)
        : loadedImage(std::move(image)),
          originalSize(sourceSize),
          loadedImageSize(loadedImage.size()),
          path(std::move(imagePath)),
          exifInfo(std::move(metadata)),
          fileSize(size)
    {
    }
    bool isRenderable() const
    {
        return !loadedImage.isNull() || !resizedImage.isNull() || !movie.isNull();
    }
    /**
     * True when the EXIF orientation rotates the stored pixels by 90 degrees, so
     * the displayed size has width and height exchanged.
     */
    bool isOrientationRotated() const
    {
        return exifInfo.Orientation == 6 || exifInfo.Orientation == 8;
    }
    QSize orientedSize(const QSize &size) const
    {
        return isOrientationRotated() ? QSize(size.height(), size.width()) : size;
    }
    bool isLandscape() const { return originalSize.width() > originalSize.height(); }
    void initializeAnimation();
};

#endif // IMAGECONTENT_H
