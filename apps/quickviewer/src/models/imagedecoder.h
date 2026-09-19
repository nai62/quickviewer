#ifndef IMAGEDECODER_H
#define IMAGEDECODER_H

#include <QtCore>
#include <QtGui>

#include "qvenums.h"

/**
 * Inputs the decode backends need. Capturing them here keeps the decoder from
 * reading application settings on its own.
 */
struct ImageDecodeSettings
{
    int maxTextureSize = 0;
    bool fastDctForJpeg = false;
    QSize svgRasterMaximum;
    qvEnums::SvgLoaderBackend svgLoaderBackend = qvEnums::SvgLoaderBackend::Resvg;
};

/**
 * Pixels produced by one decode backend, together with the size of the encoded
 * image. A backend may return an image smaller than sourceSize when it decoded
 * closer to the requested target size.
 */
struct ImageDecodeOutput
{
    QImage image;
    QSize sourceSize;
};

/**
 * @brief Produces pixels for one backend that the caller already chose.
 *
 * Deciding which backends to try, in which order, and what to do when one of
 * them fails stays with the caller. Each method only reports whether the given
 * bytes could be decoded by that backend. The class holds no QObject and is
 * safe to use from the image worker threads.
 */
class ImageDecoder
{
public:
    explicit ImageDecoder(ImageDecodeSettings settings = ImageDecodeSettings());

    /**
     * Largest size an image may be decoded to, honoring both the maximum
     * texture size and the caller's target size.
     */
    static QSize
    constrainedDecodeSize(const QSize &sourceSize, const QSize &requestedSize, int maxTextureSize);

    bool decodeTurboJpeg(const QByteArray &bytes,
                         const QSize &decodeTargetSize,
                         ImageDecodeOutput &output) const;
    bool decodeSpng(const QByteArray &bytes, ImageDecodeOutput &output) const;
    bool decodeWebP(const QByteArray &bytes,
                    const QSize &decodeTargetSize,
                    ImageDecodeOutput &output) const;

    /**
     * SVG rasterization has no alternative backend, so this always reports the
     * rasterization result and never falls back.
     */
    ImageDecodeOutput decodeSvg(const QByteArray &bytes, const QString &path) const;

    /**
     * Reads an already configured Qt reader. Returns a null image when reading
     * failed. Data and format errors give up at once because no retry can fix
     * them; the failures that can come from outside the bytes are retried a few
     * times. bailOutOnFailure stops after the first failure either way.
     */
    static QImage readWithQt(QImageReader &reader, const QString &logPath, bool bailOutOnFailure);

private:
    ImageDecodeSettings m_settings;
};

#endif // IMAGEDECODER_H
