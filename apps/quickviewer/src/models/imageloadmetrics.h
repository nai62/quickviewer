#ifndef IMAGELOADMETRICS_H
#define IMAGELOADMETRICS_H

#include <QtCore>

enum class JpegDecoderPreference {
    Auto,
    Qt,
    TurboJpeg,
};

enum class PngDecoderPreference {
    Auto,
    Qt,
    LibSpng,
};

enum class WebPDecoderPreference {
    Auto,
    Qt,
    LibWebP,
};

struct ImageDecodePolicy
{
    static JpegDecoderPreference defaultJpegPreference()
    {
        static const JpegDecoderPreference preference = [] {
            const QByteArray value = qgetenv("QV_BENCHMARK_JPEG_DECODER").toLower();
            if (value == "qt") {
                return JpegDecoderPreference::Qt;
            }
            if (value == "turbojpeg") {
                return JpegDecoderPreference::TurboJpeg;
            }
            return JpegDecoderPreference::Auto;
        }();
        return preference;
    }

    static PngDecoderPreference defaultPngPreference()
    {
        static const PngDecoderPreference preference = [] {
            const QByteArray value = qgetenv("QV_BENCHMARK_PNG_DECODER").toLower();
            if (value == "qt") {
                return PngDecoderPreference::Qt;
            }
            if (value == "libspng") {
                return PngDecoderPreference::LibSpng;
            }
            return PngDecoderPreference::Auto;
        }();
        return preference;
    }

    static WebPDecoderPreference defaultWebPPreference()
    {
        static const WebPDecoderPreference preference = [] {
            const QByteArray value = qgetenv("QV_BENCHMARK_WEBP_DECODER").toLower();
            if (value == "qt") {
                return WebPDecoderPreference::Qt;
            }
            if (value == "libwebp") {
                return WebPDecoderPreference::LibWebP;
            }
            return WebPDecoderPreference::Auto;
        }();
        return preference;
    }

    JpegDecoderPreference jpeg = defaultJpegPreference();
    PngDecoderPreference png = defaultPngPreference();
    WebPDecoderPreference webp = defaultWebPPreference();
};

struct ImageDecodeMetrics
{
    QString format;
    QString decoderBackend;
    QSize sourceSize;
    QSize outputSize;
    qint64 decodeNanoseconds = 0;
    qint64 pipelineNanoseconds = 0;
};

#endif // IMAGELOADMETRICS_H
