#ifndef SVGLOADER_H
#define SVGLOADER_H

#include <QtCore>
#include <QtGui>

#include "qv_init.h"

namespace SvgLoader {

constexpr int MinimumRasterDimension = 100;
constexpr int MaximumRasterDimension = 10000;
constexpr int DefaultMaximumWidth = 1920;
constexpr int DefaultMaximumHeight = 1080;

struct RenderResult
{
    QImage image;
    QSize sourceSize;
    qvEnums::SvgLoaderBackend backend = qvEnums::Resvg;
    QString resvgError;
};

int validatedRasterDimension(int value, int defaultValue);
QString storageValue(qvEnums::SvgLoaderBackend backend);
qvEnums::SvgLoaderBackend backendFromStorageValue(const QString &value);
QSize fittedRasterSize(const QSizeF &sourceSize, const QSize &maximumSize);
RenderResult render(
    const QByteArray &data,
    const QString &sourcePath,
    const QSize &maximumSize,
    qvEnums::SvgLoaderBackend preferredBackend);

} // namespace SvgLoader

#endif // SVGLOADER_H
