#ifndef JPEGORIENTATION_H
#define JPEGORIENTATION_H

#include <QtCore>

/**
 * EXIF orientation of a JPEG, read from the markers without decoding the pixels.
 * Returns 1 when the bytes carry no usable orientation, which is the upright
 * value every caller wants, and never trusts a length or offset it finds.
 */
int jpegExifOrientation(const QByteArray &bytes);

#endif // JPEGORIENTATION_H
