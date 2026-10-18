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
    /**
     * True when the file carries EXIF the viewer could read: a JPEG whose full
     * EXIF parse succeeded, or a RAW image whose decoder reported its extents.
     * False until hasDetailedMetadata is set, since the fast path only reads
     * the orientation.
     */
    bool hasExif = false;
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
     * True when the EXIF orientation exchanges the axes of the stored pixels
     * (a quarter turn, with or without a mirror), so the displayed size has
     * width and height exchanged.
     */
    static bool orientationSwapsAxes(int orientation)
    {
        return orientation >= 5 && orientation <= 8;
    }
    bool isOrientationRotated() const { return orientationSwapsAxes(exifInfo.Orientation); }
    /**
     * Linear transform that turns the stored pixels upright for the EXIF
     * orientation. It carries no translation, so an image it flips or turns
     * lands on the negative side of that axis; callers move it back by the
     * mapped bounds. Values outside 1-8, including easyexif's 0 for a missing
     * tag, are upright.
     */
    QTransform orientationTransform() const
    {
        switch (exifInfo.Orientation) {
        case 2: // mirror horizontal
            return QTransform(-1, 0, 0, 1, 0, 0);
        case 3: // rotate 180
            return QTransform(-1, 0, 0, -1, 0, 0);
        case 4: // mirror vertical
            return QTransform(1, 0, 0, -1, 0, 0);
        case 5: // mirror horizontal and rotate 270 clockwise (transpose)
            return QTransform(0, 1, 1, 0, 0, 0);
        case 6: // rotate 90 clockwise
            return QTransform(0, 1, -1, 0, 0, 0);
        case 7: // mirror horizontal and rotate 90 clockwise (transverse)
            return QTransform(0, -1, -1, 0, 0, 0);
        case 8: // rotate 270 clockwise
            return QTransform(0, -1, 1, 0, 0, 0);
        default:
            return QTransform();
        }
    }
    /**
     * \a image, cut from the stored pixels, turned upright for the EXIF
     * orientation. The page view turns its item instead, so this is for the
     * places that hand the pixels on.
     */
    QImage orientedImage(const QImage &image) const
    {
        const QTransform transform = orientationTransform();
        return transform.isIdentity() ? image : image.transformed(transform);
    }
    QSize orientedSize(const QSize &size) const
    {
        return isOrientationRotated() ? QSize(size.height(), size.width()) : size;
    }
    /** Size of the source image as displayed, after the EXIF orientation. */
    QSize displayedSize() const { return orientedSize(originalSize); }
    /** True when the page is wider than tall as displayed. */
    bool isLandscape() const
    {
        const QSize size = displayedSize();
        return size.width() > size.height();
    }
    void initializeAnimation();
};

#endif // IMAGECONTENT_H
