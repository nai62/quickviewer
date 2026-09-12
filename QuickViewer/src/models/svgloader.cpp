#include "svgloader.h"

#include <cmath>
#include <limits>
#include <memory>

#include <QBuffer>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>

#include <resvg.h>

namespace {

QMutex &resvgMutex()
{
    static QMutex mutex;
    return mutex;
}

struct ResvgState
{
    ResvgState()
        : options(resvg_options_create())
    {
        resvg_options_load_system_fonts(options);
    }

    ~ResvgState()
    {
        resvg_options_destroy(options);
    }

    resvg_options *options;
};

ResvgState &resvgState()
{
    static ResvgState state;
    return state;
}

QString resvgErrorString(int error)
{
    switch (error) {
    case RESVG_ERROR_NOT_AN_UTF8_STR:
        return QStringLiteral("The SVG content is not UTF-8");
    case RESVG_ERROR_FILE_OPEN_FAILED:
        return QStringLiteral("Failed to open an SVG resource");
    case RESVG_ERROR_MALFORMED_GZIP:
        return QStringLiteral("The SVGZ data is malformed");
    case RESVG_ERROR_ELEMENTS_LIMIT_REACHED:
        return QStringLiteral("The SVG element limit was reached");
    case RESVG_ERROR_INVALID_SIZE:
        return QStringLiteral("The SVG has an invalid size");
    case RESVG_ERROR_PARSING_FAILED:
        return QStringLiteral("Failed to parse the SVG data");
    default:
        return QStringLiteral("Unknown resvg error (%1)").arg(error);
    }
}

int sourceDimension(qreal value)
{
    return static_cast<int>(qBound(
        1.0,
        std::ceil(value),
        static_cast<double>(std::numeric_limits<int>::max())));
}

SvgLoader::RenderResult renderWithResvg(
    const QByteArray &data, const QString &sourcePath, const QSize &maximumSize)
{
    SvgLoader::RenderResult result;
    result.backend = qvEnums::Resvg;

    QMutexLocker locker(&resvgMutex());
    ResvgState &state = resvgState();

    QByteArray resourcesDirectory;
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.exists() && sourceInfo.isFile()) {
        resourcesDirectory = sourceInfo.absolutePath().toUtf8();
    }
    resvg_options_set_resources_dir(
        state.options,
        resourcesDirectory.isEmpty() ? nullptr : resourcesDirectory.constData());

    resvg_render_tree *rawTree = nullptr;
    const int error = resvg_parse_tree_from_data(
        data.constData(), static_cast<uintptr_t>(data.size()), state.options, &rawTree);
    if (error != RESVG_OK) {
        result.resvgError = resvgErrorString(error);
        return result;
    }
    const std::unique_ptr<resvg_render_tree, decltype(&resvg_tree_destroy)> tree(
        rawTree, &resvg_tree_destroy);

    const resvg_size rawSourceSize = resvg_get_image_size(tree.get());
    const QSizeF sourceSize(rawSourceSize.width, rawSourceSize.height);
    const QSize rasterSize = SvgLoader::fittedRasterSize(sourceSize, maximumSize);
    if (!rasterSize.isValid()) {
        result.resvgError = QStringLiteral("resvg returned an invalid SVG size");
        return result;
    }

    result.sourceSize = QSize(
        sourceDimension(sourceSize.width()),
        sourceDimension(sourceSize.height()));
    result.image = QImage(rasterSize, QImage::Format_RGBA8888_Premultiplied);
    if (result.image.isNull()) {
        result.resvgError = QStringLiteral("resvg failed to allocate the output image");
        return result;
    }
    result.image.fill(Qt::transparent);

    resvg_transform transform = resvg_transform_identity();
    transform.a = static_cast<float>(rasterSize.width() / sourceSize.width());
    transform.d = static_cast<float>(rasterSize.height() / sourceSize.height());
    resvg_render(
        tree.get(),
        transform,
        static_cast<uint32_t>(rasterSize.width()),
        static_cast<uint32_t>(rasterSize.height()),
        reinterpret_cast<char *>(result.image.bits()));
    return result;
}

SvgLoader::RenderResult renderWithQtSvg(
    const QByteArray &data, const QSize &maximumSize, const QString &resvgError = QString())
{
    SvgLoader::RenderResult result;
    result.backend = qvEnums::QtSvg;
    result.resvgError = resvgError;

    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, QByteArrayLiteral("svg"));
    if (!reader.canRead()) {
        return result;
    }

    result.sourceSize = reader.size();
    const QSize rasterSize = SvgLoader::fittedRasterSize(result.sourceSize, maximumSize);
    if (!rasterSize.isValid()) {
        return result;
    }
    reader.setScaledSize(rasterSize);
    result.image = reader.read();
    return result;
}

} // namespace

namespace SvgLoader {

int validatedRasterDimension(int value, int defaultValue)
{
    if (value < MinimumRasterDimension || value > MaximumRasterDimension) {
        return defaultValue;
    }
    return value;
}

QString storageValue(qvEnums::SvgLoaderBackend backend)
{
    switch (backend) {
    case qvEnums::QtSvg:
        return QStringLiteral("qtsvg");
    case qvEnums::Resvg:
    default:
        return QStringLiteral("resvg");
    }
}

qvEnums::SvgLoaderBackend backendFromStorageValue(const QString &value)
{
    if (value == QLatin1String("qtsvg") || value == QLatin1String("qsvg")) {
        return qvEnums::QtSvg;
    }
    return qvEnums::Resvg;
}

QSize fittedRasterSize(const QSizeF &sourceSize, const QSize &maximumSize)
{
    if (sourceSize.width() <= 0.0 || sourceSize.height() <= 0.0 || maximumSize.width() <= 0 || maximumSize.height() <= 0) {
        return QSize();
    }

    const qreal scale = qMin(
        maximumSize.width() / sourceSize.width(),
        maximumSize.height() / sourceSize.height());
    return QSize(
        qBound(1, qRound(sourceSize.width() * scale), maximumSize.width()),
        qBound(1, qRound(sourceSize.height() * scale), maximumSize.height()));
}

RenderResult render(
    const QByteArray &data,
    const QString &sourcePath,
    const QSize &maximumSize,
    qvEnums::SvgLoaderBackend preferredBackend)
{
    if (preferredBackend == qvEnums::QtSvg) {
        return renderWithQtSvg(data, maximumSize);
    }

    RenderResult result = renderWithResvg(data, sourcePath, maximumSize);
    if (!result.image.isNull()) {
        return result;
    }

    qWarning() << "resvg failed; falling back to Qt SVG:" << result.resvgError;
    return renderWithQtSvg(data, maximumSize, result.resvgError);
}

} // namespace SvgLoader
