#include "imagedecoder.h"

#include <QLibrary>
#include <climits>
#include <cstring>
#include <limits>
#include <memory>

#define SPNG_STATIC
#include <spng.h>

#include "svgloader.h"

namespace {

bool jpegHasIccProfile(const QByteArray &bytes)
{
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype size = bytes.size();
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;
    }

    qsizetype offset = 2;
    while (offset + 4 <= size) {
        if (data[offset] != 0xFF) {
            ++offset;
            continue;
        }
        while (offset < size && data[offset] == 0xFF) {
            ++offset;
        }
        if (offset >= size) {
            break;
        }
        const unsigned char marker = data[offset++];
        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }
        if (offset + 2 > size) {
            break;
        }
        const quint16 segmentLength = static_cast<quint16>((data[offset] << 8) | data[offset + 1]);
        if (segmentLength < 2 || offset + segmentLength > size) {
            break;
        }
        if (marker == 0xE2 && segmentLength >= 14 &&
            std::memcmp(data + offset + 2, "ICC_PROFILE\0", 12) == 0) {
            return true;
        }
        offset += segmentLength;
    }
    return false;
}

bool webpHasFeature(const QByteArray &bytes, unsigned char featureMask)
{
    if (bytes.size() < 21) {
        return false;
    }
    const char *data = bytes.constData();
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WEBP", 4) != 0 ||
        std::memcmp(data + 12, "VP8X", 4) != 0) {
        return false;
    }
    return (static_cast<unsigned char>(data[20]) & featureMask) != 0;
}

bool pngHasChunk(const QByteArray &bytes, const char chunkType[5])
{
    static constexpr unsigned char PngSignature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype size = bytes.size();
    if (size < 8 || std::memcmp(data, PngSignature, sizeof(PngSignature)) != 0) {
        return false;
    }

    qsizetype offset = 8;
    while (offset + 12 <= size) {
        const quint32 chunkLength = (static_cast<quint32>(data[offset]) << 24) |
                                    (static_cast<quint32>(data[offset + 1]) << 16) |
                                    (static_cast<quint32>(data[offset + 2]) << 8) |
                                    static_cast<quint32>(data[offset + 3]);
        if (static_cast<quint64>(chunkLength) > static_cast<quint64>(size - offset - 12)) {
            return false;
        }
        const char *type = reinterpret_cast<const char *>(data + offset + 4);
        if (std::memcmp(type, chunkType, 4) == 0) {
            return true;
        }
        if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        offset += static_cast<qsizetype>(chunkLength) + 12;
    }
    return false;
}

bool tryDecodeSpng(const QByteArray &bytes, QImage &decoded, QSize &sourceSize)
{
    if (bytes.isEmpty() || pngHasChunk(bytes, "acTL") || pngHasChunk(bytes, "iCCP") ||
        pngHasChunk(bytes, "gAMA") || pngHasChunk(bytes, "cHRM")) {
        return false;
    }

    using SpngContext = std::unique_ptr<spng_ctx, decltype(&spng_ctx_free)>;
    SpngContext context(spng_ctx_new(0), &spng_ctx_free);
    if (!context) {
        return false;
    }
    if (spng_set_png_buffer(context.get(), bytes.constData(), static_cast<size_t>(bytes.size())) !=
        0) {
        return false;
    }

    spng_ihdr ihdr{};
    if (spng_get_ihdr(context.get(), &ihdr) != 0 || ihdr.width == 0 || ihdr.height == 0 ||
        ihdr.width > INT_MAX || ihdr.height > INT_MAX || ihdr.bit_depth > 8) {
        return false;
    }
    sourceSize = QSize(static_cast<int>(ihdr.width), static_cast<int>(ihdr.height));

    size_t outputSize = 0;
    if (spng_decoded_image_size(context.get(), SPNG_FMT_RGBA8, &outputSize) != 0) {
        return false;
    }
    const quint64 expectedSize = static_cast<quint64>(ihdr.width) * ihdr.height * 4;
    if (outputSize != expectedSize ||
        expectedSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
        return false;
    }

    QImage image(sourceSize, QImage::Format_RGBA8888);
    if (image.isNull() || static_cast<quint64>(image.sizeInBytes()) < expectedSize) {
        return false;
    }
    if (spng_decode_image(
            context.get(), image.bits(), outputSize, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS) != 0) {
        return false;
    }
    if (pngHasChunk(bytes, "sRGB")) {
        image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    }

    decoded = std::move(image);
    return true;
}

class NativeTurboJpegApi
{
public:
    struct ScalingFactor
    {
        int num;
        int denom;
    };

    using Handle = void *;
    using InitDecompress = Handle (*)();
    using DecompressHeader3 =
        int (*)(Handle, const unsigned char *, unsigned long, int *, int *, int *, int *);
    using GetScalingFactors = ScalingFactor *(*)(int *);
    using Decompress2 = int (*)(
        Handle, const unsigned char *, unsigned long, unsigned char *, int, int, int, int, int);
    using Destroy = int (*)(Handle);

    NativeTurboJpegApi()
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates{
            QDir(appDir).filePath("turbojpeg"),
            QDir(appDir).filePath("libturbojpeg"),
            QDir(appDir).filePath("imageformats/turbojpeg"),
            QDir(appDir).filePath("imageformats/libturbojpeg"),
            "turbojpeg",
            "libturbojpeg",
        };
        for (const QString &candidate : candidates) {
            m_library.setFileName(candidate);
            if (!m_library.load()) {
                continue;
            }
            initDecompress =
                reinterpret_cast<InitDecompress>(m_library.resolve("tjInitDecompress"));
            decompressHeader3 =
                reinterpret_cast<DecompressHeader3>(m_library.resolve("tjDecompressHeader3"));
            getScalingFactors =
                reinterpret_cast<GetScalingFactors>(m_library.resolve("tjGetScalingFactors"));
            decompress2 = reinterpret_cast<Decompress2>(m_library.resolve("tjDecompress2"));
            destroy = reinterpret_cast<Destroy>(m_library.resolve("tjDestroy"));
            if (available()) {
                return;
            }
            m_library.unload();
        }
    }

    bool available() const
    {
        return initDecompress && decompressHeader3 && getScalingFactors && decompress2 && destroy;
    }

    InitDecompress initDecompress = nullptr;
    DecompressHeader3 decompressHeader3 = nullptr;
    GetScalingFactors getScalingFactors = nullptr;
    Decompress2 decompress2 = nullptr;
    Destroy destroy = nullptr;

private:
    QLibrary m_library;
};

NativeTurboJpegApi &nativeTurboJpegApi()
{
    static NativeTurboJpegApi *api = new NativeTurboJpegApi;
    return *api;
}

NativeTurboJpegApi::Handle nativeTurboJpegHandle()
{
    NativeTurboJpegApi &api = nativeTurboJpegApi();
    if (!api.available()) {
        return nullptr;
    }

    struct ThreadHandle
    {
        NativeTurboJpegApi::Handle handle = nullptr;
        NativeTurboJpegApi::Destroy destroy = nullptr;
        ~ThreadHandle()
        {
            if (handle && destroy) {
                destroy(handle);
            }
        }
    };
    thread_local ThreadHandle threadHandle;
    if (!threadHandle.handle) {
        threadHandle.handle = api.initDecompress();
        threadHandle.destroy = api.destroy;
    }
    return threadHandle.handle;
}

int turboScaledDimension(int dimension, const NativeTurboJpegApi::ScalingFactor &factor)
{
    return (dimension * factor.num + factor.denom - 1) / factor.denom;
}

bool tryDecodeTurboJpeg(const QByteArray &bytes,
                        const QSize &decodeTargetSize,
                        const ImageDecodeSettings &settings,
                        QImage &decoded,
                        QSize &sourceSize)
{
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
    Q_UNUSED(bytes);
    Q_UNUSED(decodeTargetSize);
    Q_UNUSED(settings);
    Q_UNUSED(decoded);
    Q_UNUSED(sourceSize);
    return false;
#else
    if (bytes.isEmpty() || static_cast<quint64>(bytes.size()) > ULONG_MAX ||
        jpegHasIccProfile(bytes)) {
        return false;
    }

    NativeTurboJpegApi &api = nativeTurboJpegApi();
    NativeTurboJpegApi::Handle handle = nativeTurboJpegHandle();
    if (!api.available() || !handle) {
        return false;
    }

    int width = 0;
    int height = 0;
    int subsampling = 0;
    int colorSpace = 0;
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const auto dataSize = static_cast<unsigned long>(bytes.size());
    if (api.decompressHeader3(handle, data, dataSize, &width, &height, &subsampling, &colorSpace) !=
            0 ||
        width <= 0 || height <= 0) {
        return false;
    }
    // TurboJPEG's direct BGRA conversion does not preserve CMYK/YCCK semantics.
    if (colorSpace == 3 || colorSpace == 4) {
        return false;
    }

    sourceSize = QSize(width, height);
    const QSize desiredSize =
        ImageDecoder::constrainedDecodeSize(sourceSize, decodeTargetSize, settings.maxTextureSize);
    int outputWidth = width;
    int outputHeight = height;
    if (desiredSize.isValid() && desiredSize != sourceSize) {
        int factorCount = 0;
        NativeTurboJpegApi::ScalingFactor *factors = api.getScalingFactors(&factorCount);
        if (!factors || factorCount <= 0) {
            return false;
        }

        bool found = false;
        qint64 bestArea = 0;
        for (int i = 0; i < factorCount; ++i) {
            if (factors[i].num <= 0 || factors[i].denom <= 0) {
                continue;
            }
            const int candidateWidth = turboScaledDimension(width, factors[i]);
            const int candidateHeight = turboScaledDimension(height, factors[i]);
            if (candidateWidth > desiredSize.width() || candidateHeight > desiredSize.height()) {
                continue;
            }
            const qint64 area = static_cast<qint64>(candidateWidth) * candidateHeight;
            if (!found || area > bestArea) {
                found = true;
                bestArea = area;
                outputWidth = candidateWidth;
                outputHeight = candidateHeight;
            }
        }
        if (!found) {
            qint64 smallestArea = std::numeric_limits<qint64>::max();
            for (int i = 0; i < factorCount; ++i) {
                if (factors[i].num <= 0 || factors[i].denom <= 0) {
                    continue;
                }
                const int candidateWidth = turboScaledDimension(width, factors[i]);
                const int candidateHeight = turboScaledDimension(height, factors[i]);
                const qint64 area = static_cast<qint64>(candidateWidth) * candidateHeight;
                if (area < smallestArea) {
                    smallestArea = area;
                    outputWidth = candidateWidth;
                    outputHeight = candidateHeight;
                }
            }
        }
    }

    QImage image(outputWidth, outputHeight, QImage::Format_ARGB32);
    if (image.isNull()) {
        return false;
    }
    static constexpr int TurboJpegPixelFormatBgra = 8;
    static constexpr int TurboJpegFlagFastDct = 2048;
    const int flags = settings.fastDctForJpeg ? TurboJpegFlagFastDct : 0;
    if (api.decompress2(handle,
                        data,
                        dataSize,
                        image.bits(),
                        outputWidth,
                        image.bytesPerLine(),
                        outputHeight,
                        TurboJpegPixelFormatBgra,
                        flags) != 0) {
        return false;
    }

    decoded = std::move(image);
    return true;
#endif
}

class NativeWebPApi
{
public:
    using GetInfo = int (*)(const unsigned char *, size_t, int *, int *);
    using DecodeBgraInto =
        unsigned char *(*)(const unsigned char *, size_t, unsigned char *, size_t, int);

    NativeWebPApi()
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates{
            QDir(appDir).filePath("webp"),
            QDir(appDir).filePath("libwebp"),
            QDir(appDir).filePath("imageformats/webp"),
            QDir(appDir).filePath("imageformats/libwebp"),
            "webp",
            "libwebp",
        };
        for (const QString &candidate : candidates) {
            m_library.setFileName(candidate);
            if (!m_library.load()) {
                continue;
            }
            getInfo = reinterpret_cast<GetInfo>(m_library.resolve("WebPGetInfo"));
            decodeBgraInto =
                reinterpret_cast<DecodeBgraInto>(m_library.resolve("WebPDecodeBGRAInto"));
            if (available()) {
                return;
            }
            m_library.unload();
        }
    }

    bool available() const { return getInfo && decodeBgraInto; }

    GetInfo getInfo = nullptr;
    DecodeBgraInto decodeBgraInto = nullptr;

private:
    QLibrary m_library;
};

NativeWebPApi &nativeWebPApi()
{
    static NativeWebPApi *api = new NativeWebPApi;
    return *api;
}

bool tryDecodeWebP(const QByteArray &bytes,
                   const QSize &decodeTargetSize,
                   const ImageDecodeSettings &settings,
                   QImage &decoded,
                   QSize &sourceSize)
{
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
    Q_UNUSED(bytes);
    Q_UNUSED(decodeTargetSize);
    Q_UNUSED(settings);
    Q_UNUSED(decoded);
    Q_UNUSED(sourceSize);
    return false;
#else
    static constexpr unsigned char WebPFeatureAnimation = 0x02;
    static constexpr unsigned char WebPFeatureIcc = 0x20;
    if (bytes.isEmpty() || webpHasFeature(bytes, WebPFeatureAnimation) ||
        webpHasFeature(bytes, WebPFeatureIcc)) {
        return false;
    }

    NativeWebPApi &api = nativeWebPApi();
    if (!api.available()) {
        return false;
    }

    int width = 0;
    int height = 0;
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const size_t dataSize = static_cast<size_t>(bytes.size());
    if (!api.getInfo(data, dataSize, &width, &height) || width <= 0 || height <= 0) {
        return false;
    }

    sourceSize = QSize(width, height);
    if (ImageDecoder::constrainedDecodeSize(
            sourceSize, decodeTargetSize, settings.maxTextureSize) != sourceSize) {
        // The convenience direct-to-buffer API has no scaling option. Let the
        // Qt/libwebp handler use its scaled decode path for large/preview images.
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    if (image.isNull()) {
        return false;
    }
    const size_t outputSize =
        static_cast<size_t>(image.bytesPerLine()) * static_cast<size_t>(image.height());
    if (!api.decodeBgraInto(data, dataSize, image.bits(), outputSize, image.bytesPerLine())) {
        return false;
    }

    decoded = std::move(image);
    return true;
#endif
}

} // namespace

ImageDecoder::ImageDecoder(ImageDecodeSettings settings)
    : m_settings(settings)
{
}

QSize ImageDecoder::constrainedDecodeSize(const QSize &sourceSize,
                                          const QSize &requestedSize,
                                          int maxTextureSize)
{
    if (!sourceSize.isValid()) {
        return QSize();
    }

    QSize limit(maxTextureSize, maxTextureSize);
    if (requestedSize.isValid() && !requestedSize.isEmpty()) {
        limit.setWidth(qMin(limit.width(), requestedSize.width()));
        limit.setHeight(qMin(limit.height(), requestedSize.height()));
    }
    if (sourceSize.width() <= limit.width() && sourceSize.height() <= limit.height()) {
        return sourceSize;
    }
    return sourceSize.scaled(limit, Qt::KeepAspectRatio);
}

bool ImageDecoder::decodeTurboJpeg(const QByteArray &bytes,
                                   const QSize &decodeTargetSize,
                                   ImageDecodeOutput &output) const
{
    return tryDecodeTurboJpeg(bytes, decodeTargetSize, m_settings, output.image, output.sourceSize);
}

bool ImageDecoder::decodeSpng(const QByteArray &bytes, ImageDecodeOutput &output) const
{
    return tryDecodeSpng(bytes, output.image, output.sourceSize);
}

bool ImageDecoder::decodeWebP(const QByteArray &bytes,
                              const QSize &decodeTargetSize,
                              ImageDecodeOutput &output) const
{
    return tryDecodeWebP(bytes, decodeTargetSize, m_settings, output.image, output.sourceSize);
}

ImageDecodeOutput ImageDecoder::decodeSvg(const QByteArray &bytes, const QString &path) const
{
    const SvgLoader::RenderResult rendered =
        SvgLoader::render(bytes, path, m_settings.svgRasterMaximum, m_settings.svgLoaderBackend);
    ImageDecodeOutput output;
    output.image = rendered.image;
    output.sourceSize = rendered.sourceSize;
    return output;
}

QImage ImageDecoder::readWithQt(QImageReader &reader, const QString &logPath, bool bailOutOnFailure)
{
    constexpr int MaximumAttempts = 100;
    const int maximumAttempts = bailOutOnFailure ? 1 : MaximumAttempts;
    QImage image;
    // QImage processing sometimes fails
    for (int count = 1;; count++) {
        image = reader.read();
        if (!image.isNull()) {
            return image;
        }
        qDebug() << "[0]" << logPath << image << count;
        if (count >= maximumAttempts) {
            return QImage();
        }
        QThread::currentThread()->usleep(40000);
    }
}
