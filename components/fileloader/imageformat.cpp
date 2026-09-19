#include "imageformat.h"

namespace {

struct SuffixFormat
{
    const char *suffix;
    ImageFormat format;
};

// I think the excessive normalization of recent years is really ridiculous.
// Calling what we've traditionally called JPEG something else, like JFIF, is causing confusion for many people.
// And it hasn't helped solve any of the problems with the JPEG file format.
// The incompatibility with EXIF remains unresolved.
//
// The JPEG container names below are therefore one format, for display, for
// decoding and for EXIF alike.
const SuffixFormat SuffixFormats[] = {
    {"jpg", ImageFormat::Jpeg},
    {"jpeg", ImageFormat::Jpeg},
    {"jpe", ImageFormat::Jpeg},
    {"jif", ImageFormat::Jpeg},
    {"jfif", ImageFormat::Jpeg},
    {"jfi", ImageFormat::Jpeg},
    {"png", ImageFormat::Png},
    {"apng", ImageFormat::Apng},
    {"webp", ImageFormat::WebP},
    {"svg", ImageFormat::Svg},
    {"tif", ImageFormat::Tiff},
    {"tiff", ImageFormat::Tiff},
    {"gif", ImageFormat::Gif},
};

} // namespace

ImageFormat imageFormatFromSuffix(const QString &suffix)
{
    const QString normalized = suffix.toLower();
    for (const SuffixFormat &entry : SuffixFormats) {
        if (normalized == QLatin1String(entry.suffix)) {
            return entry.format;
        }
    }
    return normalized.isEmpty() ? ImageFormat::Unknown : ImageFormat::Other;
}

ImageFormat imageFormatFromPath(const QString &path)
{
    return imageFormatFromSuffix(QFileInfo(path).suffix());
}

QString imageFormatCanonicalName(ImageFormat format)
{
    switch (format) {
    case ImageFormat::Jpeg:
        return QStringLiteral("jpeg");
    case ImageFormat::Png:
    case ImageFormat::Apng:
        return QStringLiteral("png");
    case ImageFormat::WebP:
        return QStringLiteral("webp");
    case ImageFormat::Svg:
        return QStringLiteral("svg");
    case ImageFormat::Tiff:
        return QStringLiteral("tiff");
    case ImageFormat::Gif:
        return QStringLiteral("gif");
    case ImageFormat::Unknown:
    case ImageFormat::Other:
        break;
    }
    return QString();
}

QString imageFormatNameForPath(const QString &path, ImageFormat measured)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) {
        return imageFormatCanonicalName(measured);
    }
    const QString canonical = imageFormatCanonicalName(imageFormatFromSuffix(suffix));
    return canonical.isEmpty() ? suffix : canonical;
}
