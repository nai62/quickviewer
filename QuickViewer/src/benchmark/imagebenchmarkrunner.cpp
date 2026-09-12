#include "imagebenchmarkrunner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>

#include <QCommandLineParser>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QScopedPointer>
#include <QTextStream>

#include "fileloader.h"
#include "imageloadmetrics.h"
#include "volume.h"
#include "volumeloader.h"

namespace {

enum class BenchmarkMode {
    SourceDecode,
    DecodeOnly,
    DecoderCompare,
};

enum class CompareFormat {
    Jpeg,
    Png,
};

struct BenchmarkOptions
{
    QStringList inputs;
    QString outputPath;
    BenchmarkMode mode = BenchmarkMode::SourceDecode;
    bool recursive = false;
    int runs = 5;
    int warmup = 2;
    ImageDecodePolicy decodePolicy;
    CompareFormat compareFormat = CompareFormat::Jpeg;
    QStringList jpegCompareDecoders{"qt", "turbojpeg"};
    QStringList pngCompareDecoders{"qt", "libspng"};
};

struct BenchmarkRecord
{
    QString input;
    QString entry;
    QString format;
    QString container;
    QString requestedDecoder;
    QString decoder;
    QString fallbackReason;
    QString mode;
    QString sizeBucket;
    int run = 0;
    qint64 inputBytes = 0;
    QSize sourceSize;
    QSize outputSize;
    qint64 sourceLoadNanoseconds = 0;
    qint64 decodeNanoseconds = 0;
    qint64 postprocessNanoseconds = 0;
    qint64 totalNanoseconds = 0;
    double megapixelsPerSecond = 0.0;
    bool success = false;
};

struct AggregateValues
{
    QVector<double> decodeMilliseconds;
    QVector<double> totalMilliseconds;
    QVector<double> megapixelsPerSecond;
};

using ByteLoader = std::function<QByteArray()>;

QString modeName(BenchmarkMode mode)
{
    switch (mode) {
    case BenchmarkMode::SourceDecode:
        return "source-decode";
    case BenchmarkMode::DecodeOnly:
        return "decode-only";
    case BenchmarkMode::DecoderCompare:
        return "decoder-compare";
    }
    return "unknown";
}

QString normalizeFormat(const QString &entry, const QString &measuredFormat)
{
    QString format = QFileInfo(entry).suffix().toLower();
    if (format == "jpeg" || format == "jpe" || format == "jif" || format == "jfif" || format == "jfi") {
        return "jpg";
    }
    if (format.isEmpty()) {
        format = measuredFormat;
    }
    return format;
}

QString archiveContainerName(const QString &path)
{
    const QFileInfo info(path);
    const QString completeSuffix = info.completeSuffix().toLower();
    QString format = info.suffix().toLower();
    if (completeSuffix.endsWith("cbz")) {
        format = "zip";
    } else if (completeSuffix.endsWith("cbr")) {
        format = "rar";
    } else if (completeSuffix.endsWith("cb7")) {
        format = "7z";
    } else if (completeSuffix.endsWith("tar.gz")) {
        format = "tgz";
    } else if (completeSuffix.endsWith("tar.bz2")) {
        format = "tbz2";
    } else if (completeSuffix.endsWith("tar.xz")) {
        format = "txz";
    }
    return format.isEmpty() ? "archive" : format;
}

QString sizeBucket(const QSize &size)
{
    if (!size.isValid() || size.isEmpty()) {
        return "unknown";
    }
    const qint64 pixels = static_cast<qint64>(size.width()) * size.height();
    if (pixels < 2000000) {
        return "<2MP";
    }
    if (pixels < 8000000) {
        return "2-8MP";
    }
    if (pixels < 24000000) {
        return "8-24MP";
    }
    return ">=24MP";
}

QString csvField(QString value)
{
    value.replace('"', "\"\"");
    return QString("\"%1\"").arg(value);
}

double nanosecondsToMilliseconds(qint64 nanoseconds)
{
    return static_cast<double>(nanoseconds) / 1000000.0;
}

double decodeThroughput(const QSize &sourceSize, qint64 decodeNanoseconds)
{
    if (!sourceSize.isValid() || sourceSize.isEmpty() || decodeNanoseconds <= 0) {
        return 0.0;
    }
    const double megapixels = static_cast<double>(sourceSize.width()) * sourceSize.height() / 1000000.0;
    return megapixels / (static_cast<double>(decodeNanoseconds) / 1000000000.0);
}

double median(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int middle = values.size() / 2;
    if ((values.size() & 1) != 0) {
        return values[middle];
    }
    return (values[middle - 1] + values[middle]) / 2.0;
}

double percentile95(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int count = static_cast<int>(values.size());
    const int index = qBound(0, static_cast<int>(std::ceil(count * 0.95)) - 1, count - 1);
    return values[index];
}

bool parsePositiveInt(const QString &value, int minimum, int &result)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed < minimum) {
        return false;
    }
    result = parsed;
    return true;
}

bool parseJpegDecoder(const QString &value, JpegDecoderPreference &preference)
{
    const QString normalized = value.toLower();
    if (normalized == "auto") {
        preference = JpegDecoderPreference::Auto;
        return true;
    }
    if (normalized == "qt") {
        preference = JpegDecoderPreference::Qt;
        return true;
    }
    if (normalized == "turbojpeg") {
        preference = JpegDecoderPreference::TurboJpeg;
        return true;
    }
    return false;
}

bool parsePngDecoder(const QString &value, PngDecoderPreference &preference)
{
    const QString normalized = value.toLower();
    if (normalized == "auto") {
        preference = PngDecoderPreference::Auto;
        return true;
    }
    if (normalized == "qt") {
        preference = PngDecoderPreference::Qt;
        return true;
    }
    if (normalized == "libspng") {
        preference = PngDecoderPreference::LibSpng;
        return true;
    }
    return false;
}

bool parseWebPDecoder(const QString &value, WebPDecoderPreference &preference)
{
    const QString normalized = value.toLower();
    if (normalized == "auto") {
        preference = WebPDecoderPreference::Auto;
        return true;
    }
    if (normalized == "qt") {
        preference = WebPDecoderPreference::Qt;
        return true;
    }
    if (normalized == "libwebp") {
        preference = WebPDecoderPreference::LibWebP;
        return true;
    }
    return false;
}

QString jpegDecoderName(JpegDecoderPreference preference)
{
    switch (preference) {
    case JpegDecoderPreference::Auto:
        return "auto";
    case JpegDecoderPreference::Qt:
        return "qt";
    case JpegDecoderPreference::TurboJpeg:
        return "turbojpeg";
    }
    return "unknown";
}

QString pngDecoderName(PngDecoderPreference preference)
{
    switch (preference) {
    case PngDecoderPreference::Auto:
        return "auto";
    case PngDecoderPreference::Qt:
        return "qt";
    case PngDecoderPreference::LibSpng:
        return "libspng";
    }
    return "unknown";
}

QString webpDecoderName(WebPDecoderPreference preference)
{
    switch (preference) {
    case WebPDecoderPreference::Auto:
        return "auto";
    case WebPDecoderPreference::Qt:
        return "qt";
    case WebPDecoderPreference::LibWebP:
        return "libwebp";
    }
    return "unknown";
}

bool isJpegEntry(const QString &entry)
{
    const QString suffix = QFileInfo(entry).suffix().toLower();
    return suffix == "jpg" || suffix == "jpeg" || suffix == "jpe" || suffix == "jif" || suffix == "jfif" || suffix == "jfi";
}

bool isPngEntry(const QString &entry)
{
    return QFileInfo(entry).suffix().compare("png", Qt::CaseInsensitive) == 0;
}

QString compareFormatName(CompareFormat format)
{
    return format == CompareFormat::Png ? "png" : "jpeg";
}

const QStringList &comparisonDecoders(const BenchmarkOptions &options)
{
    return options.compareFormat == CompareFormat::Png ? options.pngCompareDecoders : options.jpegCompareDecoders;
}

bool isComparisonEntry(const BenchmarkOptions &options, const QString &entry)
{
    return options.compareFormat == CompareFormat::Png ? isPngEntry(entry) : isJpegEntry(entry);
}

QString requestedDecoderForEntry(const BenchmarkOptions &options, const QString &entry)
{
    const QString suffix = QFileInfo(entry).suffix().toLower();
    if (isJpegEntry(entry)) {
        return jpegDecoderName(options.decodePolicy.jpeg);
    }
    if (isPngEntry(entry)) {
        return pngDecoderName(options.decodePolicy.png);
    }
    if (suffix == "webp") {
        return webpDecoderName(options.decodePolicy.webp);
    }
    return "default";
}

bool decoderMatchesRequest(const QString &requestedDecoder, const QString &actualDecoder)
{
    if (requestedDecoder == "qt") {
        return actualDecoder.startsWith("qimagereader:") || actualDecoder.startsWith("qmovie:");
    }
    return requestedDecoder == actualDecoder;
}

bool jpegHasIccProfileForBenchmark(const QByteArray &bytes)
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
        if (marker == 0xE2 && segmentLength >= 14 && std::memcmp(data + offset + 2, "ICC_PROFILE\0", 12) == 0) {
            return true;
        }
        offset += segmentLength;
    }
    return false;
}

int jpegComponentCountForBenchmark(const QByteArray &bytes)
{
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype size = bytes.size();
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return 0;
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
        const bool isStartOfFrame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (isStartOfFrame && segmentLength >= 8) {
            return data[offset + 7];
        }
        offset += segmentLength;
    }
    return 0;
}

QString jpegFallbackReason(const QByteArray &bytes)
{
    if (jpegHasIccProfileForBenchmark(bytes)) {
        return "icc-profile";
    }
    if (jpegComponentCountForBenchmark(bytes) == 4) {
        return "four-component-jpeg";
    }
    return "other";
}

bool pngHasChunkForBenchmark(const QByteArray &bytes, const char chunkType[5])
{
    static constexpr unsigned char PngSignature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype size = bytes.size();
    if (size < 8 || std::memcmp(data, PngSignature, sizeof(PngSignature)) != 0) {
        return false;
    }

    qsizetype offset = 8;
    while (offset + 12 <= size) {
        const quint32 chunkLength = (static_cast<quint32>(data[offset]) << 24) | (static_cast<quint32>(data[offset + 1]) << 16) | (static_cast<quint32>(data[offset + 2]) << 8) | static_cast<quint32>(data[offset + 3]);
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

QString pngFallbackReason(const QByteArray &bytes)
{
    if (pngHasChunkForBenchmark(bytes, "acTL")) {
        return "animated-png";
    }
    if (pngHasChunkForBenchmark(bytes, "iCCP")) {
        return "icc-profile";
    }
    if (pngHasChunkForBenchmark(bytes, "gAMA")) {
        return "gamma-chunk";
    }
    if (pngHasChunkForBenchmark(bytes, "cHRM")) {
        return "chromaticities";
    }
    if (bytes.size() >= 25 && static_cast<unsigned char>(bytes[24]) > 8) {
        return "high-bit-depth";
    }
    return "other";
}

bool parseJpegCompareDecoders(const QString &value, QStringList &decoders)
{
    QStringList parsed;
    for (const QString &part : value.split(',', Qt::SkipEmptyParts)) {
        const QString decoder = part.trimmed().toLower();
        if (decoder != "qt" && decoder != "turbojpeg") {
            return false;
        }
        if (parsed.contains(decoder)) {
            return false;
        }
        parsed.append(decoder);
    }
    if (parsed.size() != 2) {
        return false;
    }
    decoders = parsed;
    return true;
}

bool parsePngCompareDecoders(const QString &value, QStringList &decoders)
{
    QStringList parsed;
    for (const QString &part : value.split(',', Qt::SkipEmptyParts)) {
        const QString decoder = part.trimmed().toLower();
        if (decoder != "qt" && decoder != "libspng") {
            return false;
        }
        if (parsed.contains(decoder)) {
            return false;
        }
        parsed.append(decoder);
    }
    if (parsed.size() != 2) {
        return false;
    }
    decoders = parsed;
    return true;
}

bool parseOptions(const QStringList &arguments, BenchmarkOptions &options, QString &error, QString &helpText)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("QuickViewer image loading benchmark");
    const QCommandLineOption helpOption = parser.addHelpOption();
    parser.addOption(QCommandLineOption("benchmark", "Run the image loading benchmark instead of opening the viewer."));
    parser.addOption(QCommandLineOption("recursive", "Include images in subdirectories when a directory is specified."));
    parser.addOption(QCommandLineOption("runs", "Number of measured runs per image.", "count", "5"));
    parser.addOption(QCommandLineOption("warmup", "Number of unmeasured warmup runs per image.", "count", "2"));
    parser.addOption(QCommandLineOption("benchmark-mode", "Benchmark mode: source-decode, decode-only, or decoder-compare.", "mode", "source-decode"));
    parser.addOption(QCommandLineOption("output", "CSV output path. A timestamped file is used when omitted.", "path"));
    parser.addOption(QCommandLineOption("jpeg-decoder", "JPEG decoder: auto, qt, or turbojpeg.", "backend", "auto"));
    parser.addOption(QCommandLineOption("png-decoder", "PNG decoder: auto, qt, or libspng.", "backend", "auto"));
    parser.addOption(QCommandLineOption("webp-decoder", "WebP decoder: auto, qt, or libwebp.", "backend", "auto"));
    parser.addOption(QCommandLineOption("compare-format", "Image format for decoder-compare: jpeg or png.", "format", "jpeg"));
    parser.addOption(QCommandLineOption("jpeg-decoders", "Comma-separated JPEG decoder pair for decoder-compare.", "backends", "qt,turbojpeg"));
    parser.addOption(QCommandLineOption("png-decoders", "Comma-separated PNG decoder pair for decoder-compare.", "backends", "qt,libspng"));
    parser.addPositionalArgument("input", "Image, directory, or archive to benchmark. Multiple inputs are allowed.", "[input...]");

    if (!parser.parse(arguments)) {
        error = parser.errorText();
        return false;
    }
    helpText = parser.helpText();
    if (parser.isSet(helpOption)) {
        return true;
    }
    if (!parser.isSet("benchmark")) {
        error = "--benchmark is required.";
        return false;
    }

    options.inputs = parser.positionalArguments();
    if (options.inputs.isEmpty()) {
        error = "At least one image, directory, or archive is required.";
        return false;
    }
    if (!parsePositiveInt(parser.value("runs"), 1, options.runs)) {
        error = "--runs must be an integer greater than zero.";
        return false;
    }
    if (!parsePositiveInt(parser.value("warmup"), 0, options.warmup)) {
        error = "--warmup must be a non-negative integer.";
        return false;
    }

    const QString mode = parser.value("benchmark-mode").toLower();
    if (mode == "source-decode") {
        options.mode = BenchmarkMode::SourceDecode;
    } else if (mode == "decode-only") {
        options.mode = BenchmarkMode::DecodeOnly;
    } else if (mode == "decoder-compare") {
        options.mode = BenchmarkMode::DecoderCompare;
    } else {
        error = "--benchmark-mode must be source-decode, decode-only, or decoder-compare.";
        return false;
    }
    if (!parseJpegDecoder(parser.value("jpeg-decoder"), options.decodePolicy.jpeg)) {
        error = "--jpeg-decoder must be auto, qt, or turbojpeg.";
        return false;
    }
    if (!parsePngDecoder(parser.value("png-decoder"), options.decodePolicy.png)) {
        error = "--png-decoder must be auto, qt, or libspng.";
        return false;
    }
    if (!parseWebPDecoder(parser.value("webp-decoder"), options.decodePolicy.webp)) {
        error = "--webp-decoder must be auto, qt, or libwebp.";
        return false;
    }
    const QString compareFormat = parser.value("compare-format").toLower();
    if (compareFormat == "jpeg" || compareFormat == "jpg") {
        options.compareFormat = CompareFormat::Jpeg;
    } else if (compareFormat == "png") {
        options.compareFormat = CompareFormat::Png;
    } else {
        error = "--compare-format must be jpeg or png.";
        return false;
    }
    if (!parseJpegCompareDecoders(parser.value("jpeg-decoders"), options.jpegCompareDecoders)) {
        error = "--jpeg-decoders must contain qt and turbojpeg exactly once each.";
        return false;
    }
    if (!parsePngCompareDecoders(parser.value("png-decoders"), options.pngCompareDecoders)) {
        error = "--png-decoders must contain qt and libspng exactly once each.";
        return false;
    }

    options.recursive = parser.isSet("recursive");
    options.outputPath = parser.value("output");
    if (options.outputPath.isEmpty()) {
        options.outputPath = QDir::current().filePath(
            QString("quickviewer-benchmark-%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
    }
    return true;
}

BenchmarkRecord measureOnce(
    const BenchmarkOptions &options,
    const QString &input,
    const QString &entry,
    const QString &container,
    int run,
    const ByteLoader &loader,
    const QByteArray *preloadedBytes,
    const ImageDecodePolicy &decodePolicy,
    const QString &requestedDecoder)
{
    BenchmarkRecord record;
    record.input = input;
    record.entry = entry;
    record.container = container;
    record.requestedDecoder = requestedDecoder;
    record.mode = modeName(options.mode);
    record.run = run;

    QByteArray loadedBytes;
    const QByteArray *bytes = preloadedBytes;
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (!bytes) {
        QElapsedTimer sourceTimer;
        sourceTimer.start();
        loadedBytes = loader();
        record.sourceLoadNanoseconds = sourceTimer.nsecsElapsed();
        bytes = &loadedBytes;
    }
    record.inputBytes = bytes->size();

    ImageDecodeMetrics metrics;
    const ImageContent content = Volume::decodeImageBytes(
        entry,
        *bytes,
        QSize(),
        QSize(),
        true,
        decodePolicy,
        &metrics);
    record.totalNanoseconds = totalTimer.nsecsElapsed();
    record.sourceSize = metrics.sourceSize;
    record.outputSize = metrics.outputSize;
    record.decodeNanoseconds = metrics.decodeNanoseconds;
    record.postprocessNanoseconds = qMax<qint64>(0, metrics.pipelineNanoseconds - metrics.decodeNanoseconds);
    record.decoder = metrics.decoderBackend.isEmpty() ? "unknown" : metrics.decoderBackend;
    record.format = normalizeFormat(entry, metrics.format);
    record.sizeBucket = sizeBucket(record.sourceSize);
    record.megapixelsPerSecond = decodeThroughput(record.sourceSize, record.decodeNanoseconds);
    record.success = !content.loadedImage.isNull() || content.originalSize.isValid();
    if (options.mode == BenchmarkMode::DecoderCompare && !record.success) {
        record.fallbackReason = "decode-failure";
    } else if (options.mode == BenchmarkMode::DecoderCompare && !decoderMatchesRequest(requestedDecoder, record.decoder)) {
        if (requestedDecoder == "turbojpeg") {
            record.fallbackReason = jpegFallbackReason(*bytes);
        } else if (requestedDecoder == "libspng") {
            record.fallbackReason = pngFallbackReason(*bytes);
        } else {
            record.fallbackReason = "unexpected-backend";
        }
    }
    return record;
}

ImageDecodePolicy comparePolicy(const BenchmarkOptions &options, const QString &decoder)
{
    ImageDecodePolicy policy = options.decodePolicy;
    if (options.compareFormat == CompareFormat::Png) {
        policy.png = decoder == "qt" ? PngDecoderPreference::Qt : PngDecoderPreference::LibSpng;
    } else {
        policy.jpeg = decoder == "qt" ? JpegDecoderPreference::Qt : JpegDecoderPreference::TurboJpeg;
    }
    return policy;
}

void warmUp(
    const BenchmarkOptions &options,
    const QString &input,
    const QString &entry,
    const QString &container,
    const ByteLoader &loader,
    const QByteArray *preloadedBytes,
    const ImageDecodePolicy &decodePolicy,
    const QString &requestedDecoder)
{
    for (int run = 0; run < options.warmup; ++run) {
        measureOnce(options, input, entry, container, run, loader, preloadedBytes, decodePolicy, requestedDecoder);
    }
}

void benchmarkComparisonSample(
    const BenchmarkOptions &options,
    const QString &input,
    const QString &entry,
    const QString &container,
    const ByteLoader &loader,
    QVector<BenchmarkRecord> &records)
{
    if (!isComparisonEntry(options, entry)) {
        return;
    }

    const QByteArray bytes = loader();
    const QByteArray *preloaded = &bytes;
    auto runIteration = [&](int iteration, int recordedRun, bool appendRecords) {
        QStringList decoders = comparisonDecoders(options);
        if ((iteration & 1) != 0) {
            std::reverse(decoders.begin(), decoders.end());
        }
        for (const QString &decoder : decoders) {
            const ImageDecodePolicy policy = comparePolicy(options, decoder);
            BenchmarkRecord record = measureOnce(
                options,
                input,
                entry,
                container,
                recordedRun,
                loader,
                preloaded,
                policy,
                decoder);
            if (appendRecords) {
                records.append(std::move(record));
            }
        }
    };

    for (int run = 0; run < options.warmup; ++run) {
        runIteration(run, 0, false);
    }
    for (int run = 1; run <= options.runs; ++run) {
        runIteration(options.warmup + run - 1, run, true);
    }
}

void benchmarkSample(
    const BenchmarkOptions &options,
    const QString &input,
    const QString &entry,
    const QString &container,
    const ByteLoader &loader,
    QVector<BenchmarkRecord> &records)
{
    if (options.mode == BenchmarkMode::DecoderCompare) {
        benchmarkComparisonSample(options, input, entry, container, loader, records);
        return;
    }

    QByteArray preloadedBytes;
    const QByteArray *preloaded = nullptr;
    if (options.mode == BenchmarkMode::DecodeOnly) {
        preloadedBytes = loader();
        preloaded = &preloadedBytes;
    }

    const QString requestedDecoder = requestedDecoderForEntry(options, entry);
    warmUp(options, input, entry, container, loader, preloaded, options.decodePolicy, requestedDecoder);
    for (int run = 1; run <= options.runs; ++run) {
        records.append(measureOnce(
            options,
            input,
            entry,
            container,
            run,
            loader,
            preloaded,
            options.decodePolicy,
            requestedDecoder));
    }
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void benchmarkImageFile(
    const BenchmarkOptions &options,
    const QString &input,
    const QString &path,
    const QString &entry,
    const QString &container,
    QVector<BenchmarkRecord> &records)
{
    benchmarkSample(
        options,
        input,
        entry,
        container,
        [path] { return readFile(path); },
        records);
}

void benchmarkDirectory(
    const BenchmarkOptions &options,
    const QString &input,
    QVector<BenchmarkRecord> &records)
{
    QStringList paths;
    const QDirIterator::IteratorFlags flags = options.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator iterator(input, QDir::Files | QDir::NoSymLinks, flags);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (IFileLoader::isImageFile(path)) {
            paths.append(path);
        }
    }
    IFileLoader::sortFiles(paths);

    const QDir baseDirectory(input);
    const QString absoluteInput = QFileInfo(input).absoluteFilePath();
    for (const QString &path : paths) {
        benchmarkImageFile(
            options,
            absoluteInput,
            path,
            QDir::fromNativeSeparators(baseDirectory.relativeFilePath(path)),
            "directory",
            records);
    }
}

bool benchmarkArchive(
    const BenchmarkOptions &options,
    const QString &input,
    QVector<BenchmarkRecord> &records)
{
    QScopedPointer<Volume> volume(VolumeLoader::createVolume(nullptr, input));
    if (!volume || !volume->isArchive()) {
        return false;
    }
    volume->loadPageList();
    const QString absoluteInput = QFileInfo(input).absoluteFilePath();
    const QString container = archiveContainerName(input);
    for (int page = 0; page < volume->pageCount(); ++page) {
        const QString entry = volume->pageNameAt(page);
        benchmarkSample(
            options,
            absoluteInput,
            entry,
            container,
            [volume = volume.data(), entry] { return volume->loadByteArrayByName(entry); },
            records);
    }
    return true;
}

bool benchmarkInput(
    const BenchmarkOptions &options,
    const QString &input,
    QVector<BenchmarkRecord> &records,
    QTextStream &messages)
{
    const QFileInfo info(input);
    if (!info.exists()) {
        messages << "Input does not exist: " << input << '\n';
        return false;
    }
    if (info.isDir()) {
        const int before = records.size();
        benchmarkDirectory(options, info.absoluteFilePath(), records);
        if (records.size() == before) {
            messages << "No supported images found in directory: " << input << '\n';
            return false;
        }
        return true;
    }
    if (info.isFile() && IFileLoader::isImageFile(info.absoluteFilePath())) {
        benchmarkImageFile(
            options,
            info.absoluteFilePath(),
            info.absoluteFilePath(),
            info.fileName(),
            "file",
            records);
        return true;
    }
    if (info.isFile() && benchmarkArchive(options, info.absoluteFilePath(), records)) {
        return true;
    }

    messages << "Unsupported benchmark input: " << input << '\n';
    return false;
}

bool writeCsv(const QString &path, const QVector<BenchmarkRecord> &records, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        error = QString("Could not open CSV output: %1").arg(path);
        return false;
    }

    QTextStream out(&file);
    out << "input,entry,format,container,requested_decoder,decoder,fallback_reason,mode,size_bucket,run,input_bytes,source_width,source_height,output_width,output_height,source_load_us,decode_us,postprocess_us,total_us,megapixels_per_second,success\n";
    for (const BenchmarkRecord &record : records) {
        out << csvField(record.input) << ','
            << csvField(record.entry) << ','
            << csvField(record.format) << ','
            << csvField(record.container) << ','
            << csvField(record.requestedDecoder) << ','
            << csvField(record.decoder) << ','
            << csvField(record.fallbackReason) << ','
            << csvField(record.mode) << ','
            << csvField(record.sizeBucket) << ','
            << record.run << ','
            << record.inputBytes << ','
            << record.sourceSize.width() << ','
            << record.sourceSize.height() << ','
            << record.outputSize.width() << ','
            << record.outputSize.height() << ','
            << QString::number(record.sourceLoadNanoseconds / 1000.0, 'f', 3) << ','
            << QString::number(record.decodeNanoseconds / 1000.0, 'f', 3) << ','
            << QString::number(record.postprocessNanoseconds / 1000.0, 'f', 3) << ','
            << QString::number(record.totalNanoseconds / 1000.0, 'f', 3) << ','
            << QString::number(record.megapixelsPerSecond, 'f', 3) << ','
            << (record.success ? "true" : "false") << '\n';
    }
    return true;
}

double percentile(QVector<double> values, double fraction)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int count = static_cast<int>(values.size());
    const int index = qBound(0, static_cast<int>(std::ceil(count * fraction)) - 1, count - 1);
    return values[index];
}

QString formatTable(const QStringList &headers, const QVector<QStringList> &rows, const QVector<bool> &rightAligned)
{
    QVector<int> widths(headers.size());
    for (int column = 0; column < headers.size(); ++column) {
        widths[column] = headers[column].size();
    }
    for (const QStringList &row : rows) {
        for (int column = 0; column < qMin(row.size(), widths.size()); ++column) {
            widths[column] = qMax(widths[column], row[column].size());
        }
    }

    auto formattedRow = [&](const QStringList &row) {
        QString line;
        for (int column = 0; column < widths.size(); ++column) {
            if (column > 0) {
                line += "  ";
            }
            const QString value = row.value(column);
            const bool alignRight = column < rightAligned.size() && rightAligned[column];
            line += alignRight ? value.rightJustified(widths[column]) : value.leftJustified(widths[column]);
        }
        return line;
    };

    QString text = formattedRow(headers) + '\n';
    QStringList separators;
    for (int column = 0; column < widths.size(); ++column) {
        separators.append(QString(widths[column], '-'));
    }
    text += formattedRow(separators) + '\n';
    for (const QStringList &row : rows) {
        text += formattedRow(row) + '\n';
    }
    return text;
}

QString buildParsedOptionsSummary(const BenchmarkOptions &options)
{
    QVector<QStringList> rows{
        {"benchmark", "true"},
        {"benchmark-mode", modeName(options.mode)},
        {"recursive", options.recursive ? "true" : "false"},
        {"runs", QString::number(options.runs)},
        {"warmup", QString::number(options.warmup)},
        {"output", options.outputPath},
        {"jpeg-decoder", jpegDecoderName(options.decodePolicy.jpeg)},
        {"png-decoder", pngDecoderName(options.decodePolicy.png)},
        {"webp-decoder", webpDecoderName(options.decodePolicy.webp)},
        {"compare-format", compareFormatName(options.compareFormat)},
        {"jpeg-decoders", options.jpegCompareDecoders.join(',')},
        {"png-decoders", options.pngCompareDecoders.join(',')},
    };
    for (int i = 0; i < options.inputs.size(); ++i) {
        rows.append({QString("input[%1]").arg(i), options.inputs[i]});
    }
    return formatTable({"option", "value"}, rows, {false, false});
}

QString comparisonImageKey(const BenchmarkRecord &record)
{
    static const QChar separator(0x1F);
    return record.input + separator + record.entry;
}

QString buildComparisonSummary(const BenchmarkOptions &options, const QVector<BenchmarkRecord> &records)
{
    const QStringList &decoders = comparisonDecoders(options);
    if (options.mode != BenchmarkMode::DecoderCompare || decoders.size() != 2) {
        return {};
    }

    struct ImageValues
    {
        QMap<QString, QVector<double>> validDecodeMilliseconds;
        QMap<QString, QString> fallbackReasons;
    };

    QMap<QString, ImageValues> images;
    for (const BenchmarkRecord &record : records) {
        ImageValues &image = images[comparisonImageKey(record)];
        if (record.success && decoderMatchesRequest(record.requestedDecoder, record.decoder)) {
            image.validDecodeMilliseconds[record.requestedDecoder].append(nanosecondsToMilliseconds(record.decodeNanoseconds));
        } else if (!record.fallbackReason.isEmpty()) {
            image.fallbackReasons[record.requestedDecoder] = record.fallbackReason;
        } else if (!record.success) {
            image.fallbackReasons[record.requestedDecoder] = "decode-failure";
        }
    }

    const QString baseline = decoders[0];
    const QString candidate = decoders[1];
    QMap<QString, QVector<double>> perImageMedians;
    QVector<double> speedups;
    QMap<QString, int> exclusionReasons;
    int pairedImages = 0;

    for (auto it = images.cbegin(); it != images.cend(); ++it) {
        const ImageValues &image = it.value();
        const QVector<double> baselineRuns = image.validDecodeMilliseconds.value(baseline);
        const QVector<double> candidateRuns = image.validDecodeMilliseconds.value(candidate);
        if (baselineRuns.size() == options.runs && candidateRuns.size() == options.runs) {
            const double baselineMedian = median(baselineRuns);
            const double candidateMedian = median(candidateRuns);
            perImageMedians[baseline].append(baselineMedian);
            perImageMedians[candidate].append(candidateMedian);
            if (candidateMedian > 0.0) {
                speedups.append(baselineMedian / candidateMedian);
            }
            ++pairedImages;
            continue;
        }

        QString reason = image.fallbackReasons.value(candidate);
        if (reason.isEmpty()) {
            reason = image.fallbackReasons.value(baseline);
        }
        if (reason.isEmpty()) {
            reason = "incomplete-pair";
        }
        ++exclusionReasons[reason];
    }

    QString text;
    QTextStream out(&text);
    out << QString("Paired %1 decoder comparison\n").arg(options.compareFormat == CompareFormat::Png ? "PNG" : "JPEG");
    QVector<QStringList> overview{
        {"images considered", QString::number(images.size())},
        {"paired images", QString::number(pairedImages)},
        {"excluded images", QString::number(images.size() - pairedImages)},
        {QString("%1 median decode ms").arg(baseline), QString::number(median(perImageMedians.value(baseline)), 'f', 3)},
        {QString("%1 median decode ms").arg(candidate), QString::number(median(perImageMedians.value(candidate)), 'f', 3)},
        {QString("%1 median speedup vs %2").arg(candidate, baseline), QString("%1x").arg(QString::number(median(speedups), 'f', 3))},
        {"speedup p10", QString("%1x").arg(QString::number(percentile(speedups, 0.10), 'f', 3))},
        {"speedup p90", QString("%1x").arg(QString::number(percentile(speedups, 0.90), 'f', 3))},
    };
    out << formatTable({"metric", "value"}, overview, {false, true});

    if (!exclusionReasons.isEmpty()) {
        out << "\nExcluded/fallback images\n";
        QVector<QStringList> rows;
        for (auto it = exclusionReasons.cbegin(); it != exclusionReasons.cend(); ++it) {
            rows.append({it.key(), QString::number(it.value())});
        }
        out << formatTable({"reason", "images"}, rows, {false, true});
    }
    return text;
}

QString buildSummary(const BenchmarkOptions &options, const QVector<BenchmarkRecord> &records)
{
    static const QChar separator(0x1F);
    QMap<QString, AggregateValues> groups;
    int failures = 0;
    for (const BenchmarkRecord &record : records) {
        if (!record.success) {
            ++failures;
            continue;
        }
        const QString key = QStringList{
            record.format,
            record.container,
            record.requestedDecoder,
            record.decoder,
            record.sizeBucket,
        }
                                .join(separator);
        AggregateValues &values = groups[key];
        values.decodeMilliseconds.append(nanosecondsToMilliseconds(record.decodeNanoseconds));
        values.totalMilliseconds.append(nanosecondsToMilliseconds(record.totalNanoseconds));
        if (record.megapixelsPerSecond > 0.0) {
            values.megapixelsPerSecond.append(record.megapixelsPerSecond);
        }
    }

    QString summaryText;
    QTextStream out(&summaryText);
    out << "QuickViewer image benchmark\n\n"
        << "Parsed options\n"
        << buildParsedOptionsSummary(options) << '\n'
        << "Result\n";
    out << formatTable(
        {"metric", "value"},
        {
            {"records", QString::number(records.size())},
            {"failures", QString::number(failures)},
        },
        {false, true});

    out << "\nAggregate results\n";
    QVector<QStringList> aggregateRows;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        const QStringList parts = it.key().split(separator);
        const AggregateValues &values = it.value();
        aggregateRows.append({
            parts.value(0),
            parts.value(1),
            parts.value(2),
            parts.value(3),
            parts.value(4),
            QString::number(values.decodeMilliseconds.size()),
            QString::number(median(values.decodeMilliseconds), 'f', 3),
            QString::number(percentile95(values.decodeMilliseconds), 'f', 3),
            QString::number(median(values.totalMilliseconds), 'f', 3),
            QString::number(median(values.megapixelsPerSecond), 'f', 1),
        });
    }
    out << formatTable(
        {"format", "container", "requested", "actual", "size", "n", "median decode ms", "p95 decode ms", "median total ms", "median MP/s"},
        aggregateRows,
        {false, false, false, false, false, true, true, true, true, true});

    const QString comparisonSummary = buildComparisonSummary(options, records);
    if (!comparisonSummary.isEmpty()) {
        out << '\n'
            << comparisonSummary;
    }
    return summaryText;
}

bool writeSummary(const QString &csvPath, const QString &summary, QString &summaryPath, QString &error)
{
    summaryPath = csvPath + ".summary.txt";
    QFile file(summaryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        error = QString("Could not open summary output: %1").arg(summaryPath);
        return false;
    }
    QTextStream out(&file);
    out << summary;
    return true;
}

} // namespace

bool ImageBenchmarkRunner::isRequested(const QStringList &arguments)
{
    return arguments.contains("--benchmark");
}

int ImageBenchmarkRunner::run(const QStringList &arguments)
{
    BenchmarkOptions options;
    QString error;
    QString helpText;
    if (!parseOptions(arguments, options, error, helpText)) {
        QTextStream(stderr) << error << "\n\n"
                            << helpText;
        return 2;
    }
    if (arguments.contains("--help") || arguments.contains("-h")) {
        QTextStream(stdout) << helpText;
        return 0;
    }

    QVector<BenchmarkRecord> records;
    QString messagesText;
    QTextStream messages(&messagesText);
    bool allInputsAccepted = true;
    for (const QString &input : options.inputs) {
        if (!benchmarkInput(options, input, records, messages)) {
            allInputsAccepted = false;
        }
    }
    if (records.isEmpty()) {
        QTextStream(stderr) << messagesText << "No benchmark records were produced.\n";
        return 2;
    }

    if (!writeCsv(options.outputPath, records, error)) {
        QTextStream(stderr) << error << '\n';
        return 2;
    }

    const QString summary = buildSummary(options, records);
    QString summaryPath;
    if (!writeSummary(options.outputPath, summary, summaryPath, error)) {
        QTextStream(stderr) << error << '\n';
        return 2;
    }

    QTextStream(stdout) << messagesText
                        << summary << '\n'
                        << "CSV: " << QFileInfo(options.outputPath).absoluteFilePath() << '\n'
                        << "Summary: " << QFileInfo(summaryPath).absoluteFilePath() << '\n';

    const bool decodeFailure = std::any_of(records.cbegin(), records.cend(), [](const BenchmarkRecord &record) {
        return !record.success;
    });
    return allInputsAccepted && !decodeFailure ? 0 : 1;
}
