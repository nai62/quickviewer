#ifndef IMAGEFORMAT_H
#define IMAGEFORMAT_H

#include <QtCore>

/**
 * Image formats QuickViewer makes decisions about. Formats that are only handed
 * to Qt's automatic detection classify as Other, and paths without a usable
 * suffix classify as Unknown.
 */
enum class ImageFormat {
    Unknown,
    Jpeg,
    Png,
    Apng,
    WebP,
    Svg,
    Tiff,
    Gif,
    Other,
};

/**
 * Classifies a file suffix. The JPEG container names (jif, jfif, jfi and jpe)
 * all classify as Jpeg, which is how QuickViewer has always displayed them.
 */
ImageFormat imageFormatFromSuffix(const QString &suffix);
ImageFormat imageFormatFromPath(const QString &path);

/**
 * Machine-facing name of a format, used by the benchmark CSV and its decoder
 * options. Returns an empty string for formats that have no name of their own.
 */
QString imageFormatCanonicalName(ImageFormat format);

/**
 * Machine-facing name for a path: the canonical name of its format, or the
 * lower-cased suffix for formats QuickViewer does not model. Empty when the
 * path carries no suffix.
 */
QString imageFormatNameForPath(const QString &path);

#endif // IMAGEFORMAT_H
