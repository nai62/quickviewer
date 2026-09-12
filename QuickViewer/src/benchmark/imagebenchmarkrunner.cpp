#include "imagebenchmarkrunner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

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
};

struct BenchmarkRecord
{
    QString input;
    QString entry;
    QString format;
    QString container;
    QString decoder;
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
    return mode == BenchmarkMode::DecodeOnly ? "decode-only" : "source-decode";
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

bool parseOptions(const QStringList &arguments, BenchmarkOptions &options, QString &error, QString &helpText)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("QuickViewer image loading benchmark");
    const QCommandLineOption helpOption = parser.addHelpOption();
    parser.addOption(QCommandLineOption("benchmark", "Run the image loading benchmark instead of opening the viewer."));
    parser.addOption(QCommandLineOption("recursive", "Include images in subdirectories when a directory is specified."));
    parser.addOption(QCommandLineOption("runs", "Number of measured runs per image.", "count", "5"));
    parser.addOption(QCommandLineOption("warmup", "Number of unmeasured warmup runs per image.", "count", "2"));
    parser.addOption(QCommandLineOption("benchmark-mode", "Benchmark mode: source-decode or decode-only.", "mode", "source-decode"));
    parser.addOption(QCommandLineOption("output", "CSV output path. A timestamped file is used when omitted.", "path"));
    parser.addOption(QCommandLineOption("jpeg-decoder", "JPEG decoder: auto, qt, or turbojpeg.", "backend", "auto"));
    parser.addOption(QCommandLineOption("webp-decoder", "WebP decoder: auto, qt, or libwebp.", "backend", "auto"));
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
    } else {
        error = "--benchmark-mode must be source-decode or decode-only.";
        return false;
    }
    if (!parseJpegDecoder(parser.value("jpeg-decoder"), options.decodePolicy.jpeg)) {
        error = "--jpeg-decoder must be auto, qt, or turbojpeg.";
        return false;
    }
    if (!parseWebPDecoder(parser.value("webp-decoder"), options.decodePolicy.webp)) {
        error = "--webp-decoder must be auto, qt, or libwebp.";
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
    const QByteArray *preloadedBytes)
{
    BenchmarkRecord record;
    record.input = input;
    record.entry = entry;
    record.container = container;
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
        options.decodePolicy,
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
    return record;
}

void warmUp(
    const BenchmarkOptions &options,
    const QString &input,
    const QString &entry,
    const QString &container,
    const ByteLoader &loader,
    const QByteArray *preloadedBytes)
{
    for (int run = 0; run < options.warmup; ++run) {
        measureOnce(options, input, entry, container, run, loader, preloadedBytes);
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
    QByteArray preloadedBytes;
    const QByteArray *preloaded = nullptr;
    if (options.mode == BenchmarkMode::DecodeOnly) {
        preloadedBytes = loader();
        preloaded = &preloadedBytes;
    }

    warmUp(options, input, entry, container, loader, preloaded);
    for (int run = 1; run <= options.runs; ++run) {
        records.append(measureOnce(options, input, entry, container, run, loader, preloaded));
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
    out << "input,entry,format,container,decoder,mode,size_bucket,run,input_bytes,source_width,source_height,output_width,output_height,source_load_us,decode_us,postprocess_us,total_us,megapixels_per_second,success\n";
    for (const BenchmarkRecord &record : records) {
        out << csvField(record.input) << ','
            << csvField(record.entry) << ','
            << csvField(record.format) << ','
            << csvField(record.container) << ','
            << csvField(record.decoder) << ','
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

QString buildSummary(const BenchmarkOptions &options, const QVector<BenchmarkRecord> &records)
{
    QMap<QString, AggregateValues> groups;
    int failures = 0;
    for (const BenchmarkRecord &record : records) {
        if (!record.success) {
            ++failures;
            continue;
        }
        const QString key = QString("%1\t%2\t%3\t%4")
                                .arg(record.format, record.container, record.decoder, record.sizeBucket);
        AggregateValues &values = groups[key];
        values.decodeMilliseconds.append(nanosecondsToMilliseconds(record.decodeNanoseconds));
        values.totalMilliseconds.append(nanosecondsToMilliseconds(record.totalNanoseconds));
        if (record.megapixelsPerSecond > 0.0) {
            values.megapixelsPerSecond.append(record.megapixelsPerSecond);
        }
    }

    QString summaryText;
    QTextStream out(&summaryText);
    out << "QuickViewer image benchmark\n"
        << "mode=" << modeName(options.mode)
        << " runs=" << options.runs
        << " warmup=" << options.warmup
        << " records=" << records.size()
        << " failures=" << failures << "\n\n";
    out << "format\tcontainer\tdecoder\tsize\tn\tmedian decode ms\tp95 decode ms\tmedian total ms\tmedian MP/s\n";
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        const QStringList parts = it.key().split('\t');
        const AggregateValues &values = it.value();
        out << parts.value(0) << '\t'
            << parts.value(1) << '\t'
            << parts.value(2) << '\t'
            << parts.value(3) << '\t'
            << values.decodeMilliseconds.size() << '\t'
            << QString::number(median(values.decodeMilliseconds), 'f', 3) << '\t'
            << QString::number(percentile95(values.decodeMilliseconds), 'f', 3) << '\t'
            << QString::number(median(values.totalMilliseconds), 'f', 3) << '\t'
            << QString::number(median(values.megapixelsPerSecond), 'f', 1) << '\n';
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
