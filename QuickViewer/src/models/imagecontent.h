#ifndef IMAGECONTENT_H
#define IMAGECONTENT_H

#include <utility>

#include <QtCore>
#include <QtGui>

#include "exif.h"
#include "qvmovie.h"
#include "qv_init.h"

struct RetouchParameters
{
    float brightness;
    float contrast;
    float gamma;
    RetouchParameters(float brightness = 0.0f, float contrast = 1.0f, float gamma = 1.0f)
        : brightness(brightness),
          contrast(contrast),
          gamma(gamma)
    {}
    bool isDefault() const
    {
        return *this == RetouchParameters();
    }
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
    QvMovie movie;
    QSize originalSize;
    QSize loadedImageSize;
    QString path;
    easyexif::EXIFInfo exifInfo;
    size_t fileSize = 0;
    RetouchParameters appliedRetouchParameters;
    qvEnums::ShaderEffect appliedResizeMode = qvEnums::Bilinear;

    ImageContent() = default;
    ImageContent(QString imagePath, size_t size)
        : path(std::move(imagePath)),
          fileSize(size)
    {}
    ImageContent(QImage image, QString imagePath, QSize sourceSize, easyexif::EXIFInfo metadata, size_t size)
        : loadedImage(std::move(image)),
          originalSize(sourceSize),
          loadedImageSize(loadedImage.size()),
          path(std::move(imagePath)),
          exifInfo(std::move(metadata)),
          fileSize(size)
    {}
    bool isLandscape() const { return originalSize.width() > originalSize.height(); }
    void initializeAnimation();
};

#endif // IMAGECONTENT_H
