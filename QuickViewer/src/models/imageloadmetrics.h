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
    JpegDecoderPreference jpeg = JpegDecoderPreference::Auto;
    PngDecoderPreference png = PngDecoderPreference::Auto;
    WebPDecoderPreference webp = WebPDecoderPreference::Auto;
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
