#include <QtTest>

#include <memory>
#include <type_traits>
#include <utility>

#include "imageview.h"
#include "models/cursorscrollmapping.h"
#include "models/imagedecoder.h"
#include "models/decodemetricsscope.h"
#include "models/imagestring.h"
#include "models/loupecontroller.h"
#include "models/pagedisplayformatter.h"
#include "models/pagenavigator.h"
#include "models/storedvolumelocation.h"
#include "models/visiblepagecomposer.h"
#include "models/viewersession.h"
#include "models/qvapplication.h"
#include "models/shadereffect.h"
#include "models/shadermanager.h"
#include "models/volumecache.h"
#include "models/volumehandle.h"
#include "models/volume.h"
#include "qzimg.h"

#define FILELOADER_DATAPATH VIEWERNAVIGATION_SRCDIR "../fileloader/data/"

class FakeDecodeTimer
{
public:
    inline static qint64 now = 0;
    inline static int starts = 0;
    inline static int reads = 0;

    static void reset() { now = starts = reads = 0; }
    void start()
    {
        ++starts;
        m_startedAt = now;
    }
    qint64 nsecsElapsed() const
    {
        ++reads;
        return now - m_startedAt;
    }

private:
    qint64 m_startedAt = 0;
};

using TestDecodeMetricsScope = ImageDecodeDetail::DecodeMetricsScope<FakeDecodeTimer>;
static_assert(!std::is_copy_constructible_v<TestDecodeMetricsScope>);
static_assert(!std::is_move_constructible_v<TestDecodeMetricsScope>);

static QByteArray encodedStillPng(const QSize &size, const QColor &color)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return QByteArray();
    }
    image.save(&buffer, "PNG");
    buffer.close();
    return bytes;
}

// Encodes through a Qt plug-in that a test build may not ship.
static QByteArray encodedStillJpeg(const QSize &size, const QColor &color)
{
    if (!QImageWriter::supportedImageFormats().contains("jpeg")) {
        return QByteArray();
    }
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return QByteArray();
    }
    const bool saved = image.save(&buffer, "JPEG");
    buffer.close();
    return saved ? bytes : QByteArray();
}

// A 9x4 magenta still image. Qt's own WebP writer emits a profile chunk that
// the still-image libwebp path rejects, so the bytes are embedded instead.
static QByteArray stillWebP()
{
    return QByteArray::fromBase64("UklGRhwAAABXRUJQVlA4TBAAAAAvCMAAAAcQ/e9//wMR0f8A");
}

// Inserts a chunk right after IHDR. The CRC is left at zero: the callers below
// and the PNG backend only look at the chunk type.
static QByteArray
withChunk(const QByteArray &png, const char *type, const QByteArray &payload = QByteArray())
{
    if (png.size() < 33) {
        return png;
    }
    const quint32 length = static_cast<quint32>(payload.size());
    QByteArray chunk;
    chunk.append(static_cast<char>((length >> 24) & 0xFF));
    chunk.append(static_cast<char>((length >> 16) & 0xFF));
    chunk.append(static_cast<char>((length >> 8) & 0xFF));
    chunk.append(static_cast<char>(length & 0xFF));
    chunk.append(type, 4);
    chunk.append(payload);
    chunk.append(4, '\0');
    QByteArray result = png;
    result.insert(8 + 12 + 13, chunk);
    return result;
}

// Inserts an acTL chunk right after IHDR, which is what marks a PNG as animated.
static QByteArray withAnimationChunk(const QByteArray &png)
{
    return withChunk(png, "acTL");
}

static void appendLittleEndian16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

static void appendLittleEndian32(QByteArray &out, quint32 value)
{
    appendLittleEndian16(out, static_cast<quint16>(value & 0xFFFF));
    appendLittleEndian16(out, static_cast<quint16>((value >> 16) & 0xFFFF));
}

// Builds a JPEG that carries IFD0 with nothing but an EXIF Orientation tag.
static QByteArray
jpegWithOrientation(int orientation, const QSize &size = QSize(8, 4), quint32 ifdOffset = 8)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::yellow);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return QByteArray();
    }
    image.save(&buffer, "JPEG");
    buffer.close();
    if (bytes.size() < 2 || !bytes.startsWith("\xFF\xD8")) {
        return QByteArray();
    }

    QByteArray payload;
    payload.append("Exif\0\0", 6);
    payload.append("II", 2);
    appendLittleEndian16(payload, 0x2A);
    appendLittleEndian32(payload, ifdOffset); // offset of IFD0
    appendLittleEndian16(payload, 1); // one IFD0 entry
    appendLittleEndian16(payload, 0x0112);
    appendLittleEndian16(payload, 3); // SHORT
    appendLittleEndian32(payload, 1);
    appendLittleEndian16(payload, static_cast<quint16>(orientation));
    appendLittleEndian16(payload, 0);
    appendLittleEndian32(payload, 0); // no next IFD

    QByteArray segment;
    segment.append(static_cast<char>(0xFF));
    segment.append(static_cast<char>(0xE1));
    // JPEG marker segment lengths are big-endian, unlike the TIFF fields above.
    const quint16 segmentLength = static_cast<quint16>(payload.size() + 2);
    segment.append(static_cast<char>((segmentLength >> 8) & 0xFF));
    segment.append(static_cast<char>(segmentLength & 0xFF));
    segment.append(payload);

    // JFIF keeps APP0 first, so the Exif segment goes right after it.
    qsizetype insertAt = 2;
    if (bytes.size() > 6 && static_cast<unsigned char>(bytes[2]) == 0xFF) {
        const int firstSegmentLength =
            (static_cast<unsigned char>(bytes[4]) << 8) | static_cast<unsigned char>(bytes[5]);
        if (firstSegmentLength >= 2) {
            insertAt = 4 + firstSegmentLength;
        }
    }
    QByteArray result = bytes;
    result.insert(insertAt, segment);
    return result;
}

class EmptyFileLoader final : public IFileLoader
{
public:
    QString volumePath() const override { return "empty"; }
    QString realVolumePath() const override { return "empty"; }
    bool isArchive() const override { return false; }
    bool isValid() const override { return true; }
    bool hasSubDirectories() const override { return false; }
    QStringList contents() override { return {}; }
    QStringList subArchives() const override { return {}; }
    QByteArray getFile(QString) override { return {}; }
    InflateCacheMode getCacheMode() const override { return InflateNoCached; }
};

class MemoryFileLoader : public IFileLoader
{
public:
    explicit MemoryFileLoader(int imageCount)
    {
        for (int imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
            const QString name = QString("page-%1.bmp").arg(imageIndex);
            QImage image(16 + imageIndex, 24 + imageIndex, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(imageIndex * 60, 255, 255));
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "BMP");
            m_names.append(name);
            m_images.insert(name, bytes);
        }
    }

    QString volumePath() const override { return "memory"; }
    QString realVolumePath() const override { return "memory"; }
    bool isArchive() const override { return false; }
    bool isValid() const override { return true; }
    bool hasSubDirectories() const override { return false; }
    QStringList contents() override { return m_names; }
    QStringList subArchives() const override { return {}; }
    QByteArray getFile(QString name) override
    {
        m_requestedNames.append(name);
        return m_images.value(name);
    }
    InflateCacheMode getCacheMode() const override { return InflateNoCached; }

    QStringList requestedNames() const { return m_requestedNames; }

private:
    QStringList m_names;
    QHash<QString, QByteArray> m_images;
    QStringList m_requestedNames;
};

// Reports the pages as archive entries whose sizes fall with the page number, so
// sorting by file size reverses the order the loader lists them in.
class SizeSortedArchiveFileLoader final : public MemoryFileLoader
{
public:
    using MemoryFileLoader::MemoryFileLoader;

    bool isArchive() const override { return true; }
    quint64 getFileSize(QString name) const override
    {
        const int pageIndex = name.section('-', 1, 1).section('.', 0, 0).toInt();
        return quint64(100 - pageIndex);
    }
};

class ViewerNavigationTest : public QObject
{
    Q_OBJECT

private slots:
    void decodeMetricsScopeAccountsForEarlyReturnsAndStopsOnce()
    {
        FakeDecodeTimer::reset();
        ImageDecodeMetrics metrics;
        metrics.decodeNanoseconds = 7;
        metrics.pipelineNanoseconds = 99;
        metrics.decoderBackend = QStringLiteral("previous");
        int backendCalls = 0;
        const auto backend = [&] {
            ++backendCalls;
            FakeDecodeTimer::now += 5; // Include name construction only while timing is active.
            return QStringLiteral("successful");
        };

        const auto failedAttempt = [&] {
            TestDecodeMetricsScope scope(&metrics);
            FakeDecodeTimer::now += 11;
            scope.recordBackendOnSuccess(false, backend);
            return false; // No finish(): RAII must still accumulate the failed attempt.
        };
        QVERIFY(!failedAttempt());
        QCOMPARE(metrics.decodeNanoseconds, qint64(18));
        QCOMPARE(metrics.decoderBackend, QStringLiteral("previous"));
        QCOMPARE(backendCalls, 0);
        {
            TestDecodeMetricsScope scope(&metrics);
            FakeDecodeTimer::now += 13;
            scope.finish();
            scope.recordBackendOnSuccess(true, backend);
            FakeDecodeTimer::now += 1000; // Postprocessing must not be included.
            scope.finish();
        }
        QCOMPARE(metrics.decodeNanoseconds, qint64(31));
        QCOMPARE(metrics.decoderBackend, QStringLiteral("successful"));
        QCOMPARE(backendCalls, 1);
        QCOMPARE(FakeDecodeTimer::starts, 2);
        QCOMPARE(FakeDecodeTimer::reads, 2);
        QCOMPARE(metrics.pipelineNanoseconds, qint64(99));
    }

    void decodeMetricsScopeRecordsUnconditionalBackendBeforeStopping()
    {
        FakeDecodeTimer::reset();
        ImageDecodeMetrics metrics;
        {
            TestDecodeMetricsScope scope(&metrics);
            FakeDecodeTimer::now += 17;
            scope.recordBackend([] {
                FakeDecodeTimer::now += 5;
                return QStringLiteral("svgloader");
            });
            scope.finish();
        }
        QCOMPARE(metrics.decodeNanoseconds, qint64(22));
        QCOMPARE(metrics.decoderBackend, QStringLiteral("svgloader"));
        QCOMPARE(FakeDecodeTimer::reads, 1);
    }

    void decodeMetricsScopeDoesNoInstrumentationWithoutSink()
    {
        FakeDecodeTimer::reset();
        int backendCalls = 0;
        const auto backend = [&] {
            ++backendCalls;
            return QStringLiteral("unused");
        };
        {
            TestDecodeMetricsScope scope(nullptr);
            scope.recordBackend(backend);
            scope.recordBackendOnSuccess(true, backend);
            scope.recordBackendOnSuccess(false, backend);
            scope.finish();
        }
        QCOMPARE(backendCalls, 0);
        QCOMPARE(FakeDecodeTimer::starts, 0);
        QCOMPARE(FakeDecodeTimer::reads, 0);
    }

    void decodeImageBytesRecordsBackendForFailedSvg()
    {
        ImageDecodeMetrics metrics;
        const ImageContent content = Volume::decodeImageBytes("broken.svg",
                                                              QByteArray("not an SVG"),
                                                              QSize(),
                                                              QSize(),
                                                              true,
                                                              ImageDecodePolicy(),
                                                              &metrics);
        QVERIFY(!content.isRenderable());
        QCOMPARE(metrics.decoderBackend, QStringLiteral("svgloader"));
    }

    void decodeImageBytesRecordsMovieBackendBeforeLoadingFrames()
    {
        const QByteArray bytes =
            QByteArray::fromBase64("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7");
        ImageDecodeMetrics metrics;
        ImageContent content = Volume::decodeImageBytes(
            "frame.gif", bytes, QSize(), QSize(), true, ImageDecodePolicy(), &metrics);
        QVERIFY(!content.movie.isNull());
        QVERIFY(!content.movie.data()); // Frame decoding is deferred.
        QCOMPARE(metrics.decoderBackend, QStringLiteral("qmovie:gif"));
    }

    void initializeAnimationLoadsTheFirstFrame()
    {
        const QByteArray bytes =
            QByteArray::fromBase64("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7");
        ImageContent content = Volume::decodeImageBytes(
            "frame.gif", bytes, QSize(), QSize(), true, ImageDecodePolicy());
        QVERIFY(!content.movie.isNull());
        QVERIFY(!content.movie.data());
        QCOMPARE(content.originalSize, QSize(1, 1));

        content.initializeAnimation();
        QVERIFY(content.movie.data());
        QVERIFY(!content.loadedImage.isNull());
        QCOMPARE(content.loadedImage.size(), QSize(1, 1));
        QCOMPARE(content.originalSize, QSize(1, 1));
        QCOMPARE(content.loadedImageSize, QSize(1, 1));
    }

    void init()
    {
        qApp->setSeparatePagesWhenWideImage(true);
        qApp->setDualView(false);
        qApp->setFitting(true);
    }

    void constrainedDecodeSizeHonorsTextureAndTargetLimits()
    {
        const QSize source(4000, 2000);
        QCOMPARE(ImageDecoder::constrainedDecodeSize(source, QSize(), 4096), source);
        QCOMPARE(ImageDecoder::constrainedDecodeSize(source, QSize(), 1024), QSize(1024, 512));
        QCOMPARE(ImageDecoder::constrainedDecodeSize(source, QSize(800, 800), 4096),
                 QSize(800, 400));
        // The default settings carry no limit, which still honours the request.
        const int unlimited = ImageDecodeSettings::UnlimitedTextureSize;
        QCOMPARE(ImageDecoder::constrainedDecodeSize(source, QSize(), unlimited), source);
        QCOMPARE(ImageDecoder::constrainedDecodeSize(source, QSize(800, 800), unlimited),
                 QSize(800, 400));
        QCOMPARE(ImageDecoder::constrainedDecodeSize(QSize(), QSize(100, 100), 4096), QSize());
    }

    void defaultDecodeSettingsImposeNoLimit()
    {
        ImageDecodeSettings settings;
        QCOMPARE(settings.maxTextureSize, ImageDecodeSettings::UnlimitedTextureSize);

        // libwebp refuses bytes it would have to scale, so a default without a
        // limit is what keeps the backend usable for callers that name none.
        const QByteArray bytes = stillWebP();
        ImageDecoder decoder(settings);
        ImageDecodeOutput output;
        if (!decoder.decodeWebP(bytes, QSize(), output)) {
            QSKIP("The libwebp backend cannot decode these bytes in this environment.");
        }
        QCOMPARE(output.sourceSize, QSize(9, 4));
        QCOMPARE(output.image.size(), QSize(9, 4));
    }

    void textureSizeSettingIsClampedToItsRange()
    {
        QCOMPARE(QVApplication::clampTextureSizeSetting(0), QVApplication::TextureSizeSettingMin);
        QCOMPARE(QVApplication::clampTextureSizeSetting(-4096),
                 QVApplication::TextureSizeSettingMin);
        QCOMPARE(QVApplication::clampTextureSizeSetting(4096), 4096);
        QCOMPARE(QVApplication::clampTextureSizeSetting(999999),
                 QVApplication::TextureSizeSettingMax);
        // The default for an 8K display still fits under the cap.
        QCOMPARE(QVApplication::clampTextureSizeSetting(int(7680 * 2.1)), 16128);
    }

    void imageDecoderDecodesStillPngItself()
    {
        const QByteArray bytes = encodedStillPng(QSize(7, 5), Qt::darkCyan);
        QVERIFY(!bytes.isEmpty());

        ImageDecoder decoder;
        ImageDecodeOutput output;
        QVERIFY(decoder.decodeSpng(bytes, output));
        QCOMPARE(output.sourceSize, QSize(7, 5));
        QCOMPARE(output.image.size(), QSize(7, 5));
        QCOMPARE(output.image.pixelColor(0, 0).rgb(), QColor(Qt::darkCyan).rgb());
    }

    void imageDecoderLeavesAnimatedPngToTheCaller()
    {
        const QByteArray animated = withAnimationChunk(encodedStillPng(QSize(7, 5), Qt::darkCyan));
        QVERIFY(!animated.isEmpty());

        ImageDecoder decoder;
        ImageDecodeOutput output;
        QVERIFY(!decoder.decodeSpng(animated, output));
    }

    void pngBackendLeavesColourManagedChunksToQt()
    {
        const QByteArray plain = encodedStillPng(QSize(9, 4), Qt::magenta);
        QVERIFY(!plain.isEmpty());
        // The sRGB chunk only names the colour space, so libspng can keep the
        // bytes; a gamma chunk asks for a transform it does not apply.
        const QByteArray srgb = withChunk(plain, "sRGB", QByteArray(1, '\0'));
        const QByteArray gamma = withChunk(plain, "gAMA", QByteArray::fromHex("0000B18F"));

        ImageDecodePolicy policy;
        policy.png = PngDecoderPreference::Auto;
        ImageDecodeMetrics srgbMetrics;
        const ImageContent srgbContent = Volume::decodeImageBytes(
            "srgb.png", srgb, QSize(), QSize(), true, policy, &srgbMetrics);
        QCOMPARE(srgbMetrics.decoderBackend, QStringLiteral("libspng"));
        QCOMPARE(srgbContent.loadedImage.colorSpace(), QColorSpace(QColorSpace::SRgb));

        ImageDecodeMetrics gammaMetrics;
        const ImageContent gammaContent = Volume::decodeImageBytes(
            "gamma.png", gamma, QSize(), QSize(), true, policy, &gammaMetrics);
        QVERIFY(gammaMetrics.decoderBackend.startsWith(QStringLiteral("qimagereader:")));
        QVERIFY(!gammaContent.loadedImage.isNull());
    }

    void imageDecoderReportsInputItCannotDecode()
    {
        ImageDecoder decoder;
        ImageDecodeOutput output;
        QVERIFY(!decoder.decodeTurboJpeg(QByteArray("not a jpeg"), QSize(), output));
        QVERIFY(!decoder.decodeSpng(QByteArray("not a png"), output));
        QVERIFY(!decoder.decodeWebP(QByteArray("not a webp"), QSize(), output));
    }

    void decodeImageBytesUsesNativePngDecoder()
    {
        const QByteArray bytes = encodedStillPng(QSize(9, 4), Qt::magenta);
        QVERIFY(!bytes.isEmpty());

        ImageDecodePolicy policy;
        policy.jpeg = JpegDecoderPreference::Auto;
        policy.png = PngDecoderPreference::Auto;
        policy.webp = WebPDecoderPreference::Auto;

        ImageDecodeMetrics metrics;
        const ImageContent content =
            Volume::decodeImageBytes("still.png", bytes, QSize(), QSize(), true, policy, &metrics);

        QCOMPARE(metrics.decoderBackend, QStringLiteral("libspng"));
        QCOMPARE(content.originalSize, QSize(9, 4));
        QCOMPARE(content.loadedImage.size(), QSize(9, 4));
    }

    void decodeImageBytesUsesNativeJpegDecoder()
    {
        const QByteArray bytes = encodedStillJpeg(QSize(9, 4), Qt::magenta);
        if (bytes.isEmpty()) {
            QSKIP("Qt has no JPEG writer in this environment.");
        }
        // TurboJPEG is loaded at run time, so a test build can be without it.
        ImageDecodeSettings settings;
        settings.maxTextureSize = qApp->MaxTextureSize();
        ImageDecoder decoder(settings);
        ImageDecodeOutput probe;
        if (!decoder.decodeTurboJpeg(bytes, QSize(), probe)) {
            QSKIP("The TurboJPEG backend cannot decode these bytes in this environment.");
        }

        ImageDecodePolicy policy;
        policy.jpeg = JpegDecoderPreference::TurboJpeg;
        ImageDecodeMetrics metrics;
        const ImageContent content =
            Volume::decodeImageBytes("still.jpg", bytes, QSize(), QSize(), true, policy, &metrics);

        QCOMPARE(metrics.decoderBackend, QStringLiteral("turbojpeg"));
        QCOMPARE(content.originalSize, QSize(9, 4));
        QCOMPARE(content.loadedImage.size(), QSize(9, 4));
    }

    void decodeImageBytesUsesNativeWebPDecoder()
    {
        const QByteArray bytes = stillWebP();
        // libwebp is loaded at run time, so a test build can be without it.
        ImageDecodeSettings settings;
        settings.maxTextureSize = qApp->MaxTextureSize();
        ImageDecoder decoder(settings);
        ImageDecodeOutput probe;
        if (!decoder.decodeWebP(bytes, QSize(), probe)) {
            QSKIP("The libwebp backend cannot decode these bytes in this environment.");
        }

        ImageDecodePolicy policy;
        policy.webp = WebPDecoderPreference::LibWebP;
        ImageDecodeMetrics metrics;
        const ImageContent content =
            Volume::decodeImageBytes("still.webp", bytes, QSize(), QSize(), true, policy, &metrics);

        QCOMPARE(metrics.decoderBackend, QStringLiteral("libwebp"));
        QCOMPARE(content.originalSize, QSize(9, 4));
        QCOMPARE(content.loadedImage.size(), QSize(9, 4));
        QCOMPARE(content.loadedImage.pixelColor(0, 0), QColor(Qt::magenta));
    }

    void decodeImageBytesFallsBackToQtReaderWhenNativeDecoderIsDisabled()
    {
        if (!QImageReader::supportedImageFormats().contains("png")) {
            QSKIP("Qt has no PNG image handler in this environment.");
        }
        const QByteArray bytes = encodedStillPng(QSize(9, 4), Qt::magenta);
        QVERIFY(!bytes.isEmpty());

        ImageDecodePolicy policy;
        policy.png = PngDecoderPreference::Qt;

        ImageDecodeMetrics metrics;
        const ImageContent content =
            Volume::decodeImageBytes("still.png", bytes, QSize(), QSize(), true, policy, &metrics);

        QVERIFY(metrics.decoderBackend.startsWith(QStringLiteral("qimagereader:")));
        QCOMPARE(content.loadedImage.size(), QSize(9, 4));
    }

    void decodeImageBytesFallsBackToQtWhenNativeDecoderRejectsTheBytes()
    {
        if (!QImageReader::supportedImageFormats().contains("png")) {
            QSKIP("Qt has no PNG image handler in this environment.");
        }
        // The still-image libspng path rejects an animated PNG, so the native
        // attempt fails and the Qt reader has to produce the image instead.
        const QString path = QStringLiteral("still.png");
        const QByteArray bytes = withAnimationChunk(encodedStillPng(QSize(9, 4), Qt::magenta));
        QVERIFY(!bytes.isEmpty());
        ImageDecodeOutput rejected;
        QVERIFY(!ImageDecoder().decodeSpng(bytes, rejected));

        ImageDecodePolicy policy;
        policy.png = PngDecoderPreference::Auto;
        ImageDecodeMetrics metrics;
        const ImageContent content =
            Volume::decodeImageBytes(path, bytes, QSize(), QSize(), true, policy, &metrics);

        QVERIFY(metrics.decoderBackend.startsWith(QStringLiteral("qimagereader:")));
        QCOMPARE(content.originalSize, QSize(9, 4));
        QCOMPARE(content.loadedImage.size(), QSize(9, 4));
        QCOMPARE(content.loadedImage.pixelColor(0, 0), QColor(Qt::magenta));
    }

    void decodeImageBytesReadsPngWithFallbackFormatNames_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("unknown-format-name") << QStringLiteral("still.qv_unknown_format");
        QTest::newRow("apng-format-name") << QStringLiteral("still.apng");
        QTest::newRow("no-suffix") << QStringLiteral("still");
    }

    void decodeImageBytesReadsPngWithFallbackFormatNames()
    {
        QFETCH(QString, path);
        const QSize size(9, 4);
        const QByteArray bytes = encodedStillPng(size, Qt::magenta);
        QVERIFY(!bytes.isEmpty());

        ImageDecodePolicy policy;
        policy.png = PngDecoderPreference::Qt;
        ImageDecodeMetrics metrics;
        const ImageContent content =
            Volume::decodeImageBytes(path, bytes, QSize(), QSize(), true, policy, &metrics);

        QVERIFY(content.movie.isNull());
        QCOMPARE(content.path, path);
        QCOMPARE(content.fileSize, size_t(bytes.size()));
        QCOMPARE(content.originalSize, size);
        QCOMPARE(content.loadedImageSize, size);
        QCOMPARE(content.loadedImage.pixelColor(0, 0), QColor(Qt::magenta));
        QVERIFY(content.hasDetailedMetadata);
        QVERIFY(metrics.decoderBackend.startsWith(QStringLiteral("qimagereader:")));
    }

    void decodeImageBytesRejectsCorruptInput_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("native-png") << QStringLiteral("broken.png");
        QTest::newRow("apng") << QStringLiteral("broken.apng");
        QTest::newRow("empty-format-name") << QStringLiteral("broken");
    }

    void decodeImageBytesRejectsCorruptInput()
    {
        QFETCH(QString, path);
        const QByteArray bytes("not an image");
        ImageDecodePolicy policy;
        policy.png = PngDecoderPreference::Auto;
        ImageDecodeMetrics metrics;
        metrics.decoderBackend = QStringLiteral("previous-image");
        const ImageContent content =
            Volume::decodeImageBytes(path, bytes, QSize(), QSize(), true, policy, &metrics);

        QVERIFY(!content.isRenderable());
        QCOMPARE(content.path, path);
        QCOMPARE(content.fileSize, size_t(bytes.size()));
        QVERIFY(metrics.decoderBackend.isEmpty());
    }

    void decodeImageBytesStopsRetryingInputThatCannotBeDecoded()
    {
        if (!QImageReader::supportedImageFormats().contains("png")) {
            QSKIP("Qt has no PNG image handler in this environment.");
        }
        // A truncated PNG keeps a parseable header, so the reader reports that it
        // can read the file and only the decode fails with a data error. Retrying
        // that cannot help; before the retry was bounded it cost about 4.6 s.
        const QByteArray bytes = encodedStillPng(QSize(9, 4), Qt::magenta).left(43);
        QByteArray probe = bytes;
        QBuffer buffer(&probe);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "png");
        QVERIFY(reader.canRead());

        QElapsedTimer timer;
        timer.start();
        ImageDecodeMetrics metrics;
        const ImageContent content = Volume::decodeImageBytes(
            "truncated.png", bytes, QSize(), QSize(), true, ImageDecodePolicy(), &metrics);
        const qint64 elapsedMilliseconds = timer.elapsed();

        QVERIFY(!content.isRenderable());
        QVERIFY(metrics.decoderBackend.isEmpty());
        QVERIFY2(elapsedMilliseconds < 1000,
                 qPrintable(QStringLiteral("read took %1 ms").arg(elapsedMilliseconds)));
    }

    void decodeImageBytesPreparesStaticImage_data()
    {
        QTest::addColumn<bool>("useQt");
        QTest::addColumn<bool>("collectMetrics");
        QTest::newRow("native") << false << false;
        QTest::newRow("native-metrics") << false << true;
        QTest::newRow("qt") << true << false;
        QTest::newRow("qt-metrics") << true << true;
    }

    void decodeImageBytesPreparesStaticImage()
    {
        QFETCH(bool, useQt);
        QFETCH(bool, collectMetrics);
        const QByteArray bytes = encodedStillPng(QSize(64, 32), Qt::cyan);
        QVERIFY(!bytes.isEmpty());
        ImageDecodePolicy policy;
        policy.png = useQt ? PngDecoderPreference::Qt : PngDecoderPreference::Auto;
        ImageDecodeMetrics metrics;
        const ImageContent content = Volume::decodeImageBytes("still.png",
                                                              bytes,
                                                              QSize(8, 8),
                                                              QSize(32, 32),
                                                              false,
                                                              policy,
                                                              collectMetrics ? &metrics : nullptr);

        QCOMPARE(content.originalSize, QSize(64, 32));
        QCOMPARE(content.loadedImageSize, QSize(32, 16));
        QCOMPARE(content.loadedImage.size(), content.loadedImageSize);
        // QZimg preserves aspect ratio using the requested height, not a bounding box.
        QCOMPARE(content.resizedImage.size(), QSize(16, 8));
        QCOMPARE(content.loadedImage.pixelColor(0, 0), QColor(Qt::cyan));
        QVERIFY(content.hasDetailedMetadata); // PNG needs no deferred JPEG EXIF load.
        QCOMPARE(content.path, QStringLiteral("still.png"));
        QCOMPARE(content.fileSize, size_t(bytes.size()));
        if (collectMetrics) {
            if (useQt) {
                QVERIFY(metrics.decoderBackend.startsWith(QStringLiteral("qimagereader:")));
            } else {
                QCOMPARE(metrics.decoderBackend, QStringLiteral("libspng"));
            }
        }
    }

    void decodeImageBytesPreservesOrientationDuringPreparation_data()
    {
        QTest::addColumn<bool>("detailedMetadata");
        QTest::newRow("orientation-only") << false;
        QTest::newRow("full-exif") << true;
    }

    void decodeImageBytesPreservesOrientationDuringPreparation()
    {
        QFETCH(bool, detailedMetadata);
        const QByteArray bytes = jpegWithOrientation(6, QSize(64, 32));
        QVERIFY(!bytes.isEmpty());
        ImageDecodePolicy policy;
        policy.jpeg = JpegDecoderPreference::Qt;
        const ImageContent content = Volume::decodeImageBytes(
            "rotated.jpg", bytes, QSize(8, 16), QSize(), detailedMetadata, policy);

        QCOMPARE(content.originalSize, QSize(64, 32));
        QCOMPARE(content.loadedImageSize, QSize(64, 32));
        QCOMPARE(int(content.exifInfo.Orientation), 6);
        QCOMPARE(content.hasDetailedMetadata, detailedMetadata);
        // Page dimensions are swapped for EXIF orientation 6 before CPU resizing.
        QCOMPARE(content.resizedImage.size(), QSize(16, 8));
    }

    void decodeImageBytesRejectsExifOffsetOutsideTheSegment()
    {
        // The IFD offset is a 32-bit field, so an offset near 4 GiB used to wrap
        // the bounds check in the orientation fast path and read past the segment.
        const quint32 offsets[] = {0xFFFFFFFFu, 0xFFFFFFFEu, 0x7FFFFFFFu};
        for (quint32 ifdOffset : offsets) {
            const QByteArray bytes = jpegWithOrientation(6, QSize(8, 4), ifdOffset);
            QVERIFY(!bytes.isEmpty());
            ImageDecodePolicy policy;
            policy.jpeg = JpegDecoderPreference::Qt;
            const ImageContent content = Volume::decodeImageBytes(
                "out-of-range-ifd.jpg", bytes, QSize(), QSize(), false, policy);

            QVERIFY(!content.loadedImage.isNull());
            QCOMPARE(content.originalSize, QSize(8, 4));
            QCOMPARE(int(content.exifInfo.Orientation), 1);
        }
    }

    void decodeImageBytesRasterizesSvgThroughDecoder()
    {
        // Keep this as an escaped literal: moc 6.11.2 produces an empty .moc
        // when a multi-line raw string containing "//" appears in this file.
        const QByteArray svg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"20\">"
            "<rect width=\"40\" height=\"20\" fill=\"#4080c0\"/>"
            "</svg>";

        ImageDecodeMetrics metrics;
        const ImageContent content = Volume::decodeImageBytes(
            "shape.svg", svg, QSize(), QSize(), true, ImageDecodePolicy(), &metrics);

        QCOMPARE(metrics.decoderBackend, QStringLiteral("svgloader"));
        QVERIFY(content.hasDetailedMetadata);
        QCOMPARE(content.originalSize, QSize(40, 20));
        QVERIFY(!content.loadedImage.isNull());
    }

    void decodeImageBytesReadsExifForJpegContainerNames()
    {
        const QByteArray jpeg = jpegWithOrientation(6);
        QVERIFY(!jpeg.isEmpty());

        // The JIF/JFIF/JFI container names are JPEG, so the EXIF gate has to
        // open for them exactly as it does for .jpg.
        const QStringList paths{"rotated.jpg", "rotated.jfif", "rotated.jif", "rotated.jfi"};
        for (const QString &path : paths) {
            ImageDecodePolicy policy;
            policy.jpeg = JpegDecoderPreference::Qt;

            ImageDecodeMetrics metrics;
            const ImageContent content =
                Volume::decodeImageBytes(path, jpeg, QSize(), QSize(), true, policy, &metrics);

            QVERIFY(!content.loadedImage.isNull());
            QCOMPARE(int(content.exifInfo.Orientation), 6);
        }
    }

    void emptyViewerSessionOperationsAreSafe()
    {
        ViewerSession session(nullptr);

        QCOMPARE(session.stateKind(), ViewerStateKind::Empty);
        QVERIFY(!session.advanceSpread());
        QVERIFY(!session.retreatSpread());
        QVERIFY(!session.fastForwardPage());
        QVERIFY(!session.fastBackwardPage());
        QVERIFY(!session.firstPage());
        QVERIFY(!session.lastPage());
        QVERIFY(!session.advanceOnePage());
        QVERIFY(!session.retreatOnePage());
        QVERIFY(!session.nextVolume());
        QVERIFY(!session.prevVolume());
        QVERIFY(!session.reloadVisiblePages());
        QCOMPARE(session.currentPagePath(), QString());
        QCOMPARE(session.currentPageName(), QString());
        QCOMPARE(session.currentPageNumberText(), QString());
        QCOMPARE(session.currentPageStatusText(), QString());
        QCOMPARE(session.pageSignage(0), QString());
        QCOMPARE(session.pageSignage(-1), QString());
    }

    void pageNavigatorOwnsValidatedPagePosition()
    {
        PageNavigator navigator;
        QCOMPARE(navigator.currentPageIndex(), 0);
        QVERIFY(!navigator.selectPage(-1, 3));
        QVERIFY(!navigator.selectPage(3, 3));
        QCOMPARE(navigator.currentPageIndex(), 0);
        QVERIFY(navigator.selectPage(2, 3));
        QCOMPARE(navigator.currentPageIndex(), 2);
        navigator.reset();
        QCOMPARE(navigator.currentPageIndex(), 0);
    }

    void visiblePageComposerKeepsSpreadRulesAndPrefetchAnchor()
    {
        struct Case
        {
            const char *description;
            VisiblePageCompositionRequest request;
            QVector<int> expectedPageIndexes;
            int expectedPrefetchAnchor;
            bool shouldLoadSecondCandidate;
        };
        const QVector<Case> cases{
            {"single view", {1, 4, false, false, {false, false, true, true}}, {1}, 1, false},
            {"portrait spread", {1, 4, false, false, {true, false, true, true}}, {1, 2}, 2, true},
            {"first page alone", {0, 4, false, false, {true, true, true, true}}, {0}, 0, false},
            {"first page landscape", {1, 4, true, false, {true, false, true, true}}, {1}, 1, false},
            {"second page landscape", {1, 4, false, true, {true, false, true, true}}, {1}, 1, true},
            {"wide pages allowed", {1, 4, true, true, {true, false, false, true}}, {1, 2}, 2, true},
            {"second page disallowed",
             {1, 4, false, false, {true, false, true, false}},
             {1},
             1,
             false},
            {"last page", {3, 4, false, false, {true, false, true, true}}, {3}, 3, false},
            {"invalid page", {4, 4, false, false, {true, false, true, true}}, {}, -1, false},
        };

        for (const Case &testCase : cases) {
            Q_UNUSED(testCase.description);
            const VisiblePageComposition composition =
                VisiblePageComposer::compose(testCase.request);
            QCOMPARE(VisiblePageComposer::shouldLoadSecondPageCandidate(testCase.request),
                     testCase.shouldLoadSecondCandidate);
            QCOMPARE(composition.pageIndexes, testCase.expectedPageIndexes);
            QCOMPARE(composition.prefetchAnchorIndex, testCase.expectedPrefetchAnchor);
        }
    }

    void pageDisplayFormatterKeepsExistingTextFormats()
    {
        QCOMPARE(PageDisplayFormatter::pageNumberText(0, 12, 1), QString("(1/12)"));
        QCOMPARE(PageDisplayFormatter::pageNumberText(4, 12, 2), QString("(5-6/12)"));
        QCOMPARE(PageDisplayFormatter::pageNumberText(-1, 12, 1), QString());

        const QVector<PageDisplayEntry> onePage{{"first.png", {640, 480}}};
        QCOMPARE(PageDisplayFormatter::statusText(0, 12, onePage),
                 QString("first.png (1/12)[640x480]"));
        const QVector<PageDisplayEntry> spread{{"first.png", {640, 480}},
                                               {"second.png", {800, 600}}};
        QCOMPARE(PageDisplayFormatter::statusText(4, 12, spread),
                 QString("first.png (5-6/12)[640x480] | second.png [800x600]"));
        QCOMPARE(PageDisplayFormatter::statusText(0, 12, {}), QString());

        QCOMPARE(PageDisplayFormatter::signageText("book/page.png", 4, 12),
                 QString("book/page.png (5/12)"));
        QCOMPARE(PageDisplayFormatter::signageText({}, 4, 12), QString());
    }

    void shaderEffectVocabularyIsBuildIndependent()
    {
        struct EffectCase
        {
            qvEnums::ShaderEffect effect;
            ShaderEffectKind kind;
        };
        const QList<EffectCase> cases{
            {qvEnums::ShaderEffect::UnPrepared, ShaderEffectKind::Unprepared},
            {qvEnums::ShaderEffect::CpuBicubic, ShaderEffectKind::CpuOnly},
            {qvEnums::ShaderEffect::CpuSpline16, ShaderEffectKind::CpuOnly},
            {qvEnums::ShaderEffect::CpuSpline36, ShaderEffectKind::CpuOnly},
            {qvEnums::ShaderEffect::CpuLanczos3, ShaderEffectKind::CpuOnly},
            {qvEnums::ShaderEffect::CpuLanczos4, ShaderEffectKind::CpuOnly},
            {qvEnums::ShaderEffect::NearestNeighbor, ShaderEffectKind::FixedShader},
            {qvEnums::ShaderEffect::Bilinear, ShaderEffectKind::FixedShader},
            {qvEnums::ShaderEffect::Bicubic, ShaderEffectKind::GlShader},
            {qvEnums::ShaderEffect::Lanczos, ShaderEffectKind::GlShader},
        };
        for (const EffectCase &testCase : cases) {
            QCOMPARE(shaderEffectKind(testCase.effect), testCase.kind);
            QCOMPARE(usesGpuRendering(testCase.effect),
                     testCase.kind == ShaderEffectKind::FixedShader ||
                         testCase.kind == ShaderEffectKind::GlShader);
            QCOMPARE(resizesOnCpu(testCase.effect),
                     testCase.kind == ShaderEffectKind::Unprepared ||
                         testCase.kind == ShaderEffectKind::CpuOnly);
            // Only the fragment shader effects depend on the build.
            QCOMPARE(shaderEffectAvailable(testCase.effect),
                     testCase.kind != ShaderEffectKind::GlShader || gpuShadersAvailable());
        }

        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::CpuBicubic), QZimg::ResizeBicubic);
        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::CpuSpline16), QZimg::ResizeSpline16);
        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::CpuSpline36), QZimg::ResizeSpline36);
        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::CpuLanczos3), QZimg::ResizeLanczos3);
        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::CpuLanczos4), QZimg::ResizeLanczos4);
        // Effects that do not resize on the CPU keep the default filter.
        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::UnPrepared), QZimg::ResizeBicubic);
        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::Bilinear), QZimg::ResizeBicubic);
        QCOMPARE(cpuFilterMode(qvEnums::ShaderEffect::Lanczos), QZimg::ResizeBicubic);
    }

    void shaderEffectStringsRoundTripThroughTheVocabulary()
    {
        const QList<qvEnums::ShaderEffect> effects{
            qvEnums::ShaderEffect::UnPrepared,
            qvEnums::ShaderEffect::CpuBicubic,
            qvEnums::ShaderEffect::CpuSpline16,
            qvEnums::ShaderEffect::CpuSpline36,
            qvEnums::ShaderEffect::CpuLanczos3,
            qvEnums::ShaderEffect::CpuLanczos4,
            qvEnums::ShaderEffect::NearestNeighbor,
            qvEnums::ShaderEffect::Bilinear,
            qvEnums::ShaderEffect::Bicubic,
            qvEnums::ShaderEffect::Lanczos,
        };
        for (const qvEnums::ShaderEffect effect : effects) {
            const QString name = ShaderManager::shaderEffectToString(effect);
            QVERIFY(!name.isEmpty());
            // An effect this build cannot render falls back to Bilinear.
            const qvEnums::ShaderEffect expected =
                shaderEffectAvailable(effect) ? effect : qvEnums::ShaderEffect::Bilinear;
            QCOMPARE(ShaderManager::stringToShaderEffect(name), expected);
        }
        QCOMPARE(ShaderManager::stringToShaderEffect(QStringLiteral("NoSuchEffect")),
                 qvEnums::ShaderEffect::Bilinear);
    }

    void directImageTransitionsThroughStandalonePreview()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath("preview.bmp");
        QImage image(640, 480, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));

        ViewerSession session(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&session);

        QVERIFY(session.openFileInContainer(imagePath));
        QCOMPARE(session.stateKind(), ViewerStateKind::Loading);
        QVERIFY(session.initialImagePaintPending());

        QTRY_COMPARE(session.stateKind(), ViewerStateKind::StandalonePreview);
        QCOMPARE(QFileInfo(session.currentPageName()).fileName(), QString("preview.bmp"));
        QVERIFY(session.initialImagePaintPending());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);

        // Navigation while the parent folder is not ready must remain a no-op.
        QVERIFY(!session.advanceSpread());
        QVERIFY(!session.retreatSpread());
        QVERIFY(!session.nextVolume());
        QVERIFY(!session.prevVolume());

        session.notifyInitialImagePainted();
        QTRY_COMPARE(session.stateKind(), ViewerStateKind::VolumeReady);
        QVERIFY(!session.initialImagePaintPending());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void selectedPageIsRestoredAsReadProgress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 4; ++page) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 60, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        qApp->setOpenVolumeWithProgress(false);
        {
            ViewerSession session(nullptr);
            QVERIFY(session.openContainer(directory.path()));
            QVERIFY(session.selectPage(2));
            QCOMPARE(session.currentPageIndex(), 2);
        }

        const QString volumePath = QDir::fromNativeSeparators(directory.path());
        QVERIFY(qApp->readProgressStore()->contains(volumePath));
        const ReadProgress progress = qApp->readProgressStore()->at(volumePath);
        QCOMPARE(progress.resumePageIndex, 2);
        QCOMPARE(progress.currentPageName, QString("page-2.bmp"));

        qApp->setOpenVolumeWithProgress(true);
        ViewerSession restoredSession(nullptr);
        QVERIFY(restoredSession.openContainer(directory.path()));
        QCOMPARE(restoredSession.currentPageIndex(), 2);
        QCOMPARE(restoredSession.currentPageName(), QString("page-2.bmp"));
    }

    void readProgressResumesTheStoredPageName()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 60, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        // The position was recorded against a listing that no longer exists, so
        // only the stored name still points at the page the reader stopped on.
        const QString volumePath = QDir::fromNativeSeparators(directory.path());
        qApp->readProgressStore()->insert(volumePath,
                                          ReadProgress{QFileInfo(directory.path()).fileName(),
                                                       volumePath,
                                                       QStringLiteral("page-2.bmp"),
                                                       3,
                                                       0,
                                                       false});
        qApp->setOpenVolumeWithProgress(true);
        qApp->setDualView(false);

        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));

        QCOMPARE(session.currentPageName(), QStringLiteral("page-2.bmp"));
        QCOMPARE(session.currentPageIndex(), 2);
    }

    void readProgressFallsBackToTheIndexWhenThePageIsGone()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 60, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        const QString volumePath = QDir::fromNativeSeparators(directory.path());
        qApp->readProgressStore()->insert(volumePath,
                                          ReadProgress{QFileInfo(directory.path()).fileName(),
                                                       volumePath,
                                                       QStringLiteral("renamed.bmp"),
                                                       3,
                                                       1,
                                                       false});
        qApp->setOpenVolumeWithProgress(true);
        qApp->setDualView(false);

        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));

        QCOMPARE(session.currentPageName(), QStringLiteral("page-1.bmp"));
        QCOMPARE(session.currentPageIndex(), 1);
    }

    void finishedVolumeStartsOverInsteadOfResumingItsLastPage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 60, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        const QString volumePath = QDir::fromNativeSeparators(directory.path());
        qApp->readProgressStore()->insert(volumePath,
                                          ReadProgress{QFileInfo(directory.path()).fileName(),
                                                       volumePath,
                                                       QStringLiteral("page-2.bmp"),
                                                       3,
                                                       0,
                                                       true});
        qApp->setOpenVolumeWithProgress(true);
        qApp->setDualView(false);

        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));

        QCOMPARE(session.currentPageName(), QStringLiteral("page-0.bmp"));
        QCOMPARE(session.currentPageIndex(), 0);
    }

    void readProgressResumesAnArchiveEntry()
    {
        const QString archivePath = QString(FILELOADER_DATAPATH "7z/image.7z");
        QVERIFY(QFileInfo::exists(archivePath));

        const QString volumePath = QDir::fromNativeSeparators(archivePath);
        qApp->readProgressStore()->insert(volumePath,
                                          ReadProgress{QFileInfo(archivePath).fileName(),
                                                       volumePath,
                                                       QStringLiteral("yellow.png"),
                                                       3,
                                                       0,
                                                       false});
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        qApp->setOpenVolumeWithProgress(true);
        qApp->setDualView(false);

        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(archivePath));

        // Entry names are restored too, even though the stored index points at
        // another entry.
        QCOMPARE(session.currentPageName(), QStringLiteral("yellow.png"));
    }

    void readProgressKeepsLegacyIniKeys()
    {
        const QString volumePath = "read-progress-key-compatibility";
        const ReadProgress progress = {
            "Compatibility title", volumePath, "page-7.bmp", 12, 7, false};
        qApp->readProgressStore()->insert(volumePath, progress);
        qApp->readProgressStore()->save();

        QSettings settings(
            qApp->getFilePathOfApplicationSetting(QVApplication::readProgressSubPath()),
            QSettings::IniFormat);
        bool found = false;
        for (const QString &group : settings.childGroups()) {
            settings.beginGroup(group);
            if (settings.value("Path").toString() == volumePath) {
                found = true;
                QCOMPARE(settings.value("Title").toString(), QString("Compatibility title"));
                QCOMPARE(settings.value("CurrenPage").toString(), QString("page-7.bmp"));
                QCOMPARE(settings.value("Pages").toInt(), 12);
                QCOMPARE(settings.value("Current").toInt(), 7);
                QCOMPARE(settings.value("Completed").toBool(), false);
                QVERIFY(!settings.contains("CurrentPage"));
            }
            settings.endGroup();
        }
        QVERIFY(found);
    }

    void pageAndSpreadNavigationKeepTheirExistingStepSizes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 6; ++page) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 40, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        qApp->setOpenVolumeWithProgress(false);
        qApp->setDualView(false);
        ViewerSession singlePageSession(nullptr);
        QVERIFY(singlePageSession.openContainer(directory.path()));
        QCOMPARE(singlePageSession.currentPageIndex(), 0);
        QCOMPARE(singlePageSession.visiblePageCount(), 1);
        QVERIFY(singlePageSession.advanceSpread());
        QCOMPARE(singlePageSession.currentPageIndex(), 1);
        QVERIFY(singlePageSession.advanceOnePage());
        QCOMPARE(singlePageSession.currentPageIndex(), 2);
        QVERIFY(singlePageSession.retreatOnePage());
        QCOMPARE(singlePageSession.currentPageIndex(), 1);
        QVERIFY(singlePageSession.lastPage());
        QCOMPARE(singlePageSession.currentPageIndex(), 5);
        QVERIFY(singlePageSession.firstPage());
        QCOMPARE(singlePageSession.currentPageIndex(), 0);

        qApp->setDualView(true);
        qApp->setFirstImageAsOnePageInDualView(false);
        qApp->setWideImageAsOnePageInDualView(true);
        ViewerSession spreadSession(nullptr);
        QVERIFY(spreadSession.openContainer(directory.path()));
        QCOMPARE(spreadSession.currentPageIndex(), 0);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
        QVERIFY(spreadSession.advanceSpread());
        QCOMPARE(spreadSession.currentPageIndex(), 2);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
        QVERIFY(spreadSession.advanceOnePage());
        QCOMPARE(spreadSession.currentPageIndex(), 3);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
        QVERIFY(spreadSession.retreatSpread());
        QCOMPARE(spreadSession.currentPageIndex(), 1);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
    }

    void sortingPagesKeepsTheDisplayedPage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage large(32, 48, QImage::Format_RGB32);
        large.fill(Qt::red);
        QVERIFY(large.save(directory.filePath(QStringLiteral("a.bmp"))));
        QImage small(16, 24, QImage::Format_RGB32);
        small.fill(Qt::blue);
        QVERIFY(small.save(directory.filePath(QStringLiteral("b.bmp"))));

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QCOMPARE(session.currentPageName(), QStringLiteral("a.bmp"));

        session.sortActiveVolumePages(qvEnums::ImageSortBy::SortByFileSize);

        QCOMPARE(session.pageCount(), 2);
        QCOMPARE(session.currentPageName(), QStringLiteral("a.bmp"));
        QCOMPARE(session.currentPageIndex(), 1);
    }

    void openingAPageByNameFollowsTheVolumeSort()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage large(32, 48, QImage::Format_RGB32);
        large.fill(Qt::red);
        QVERIFY(large.save(directory.filePath(QStringLiteral("a-large.bmp"))));
        QImage small(16, 24, QImage::Format_RGB32);
        small.fill(Qt::blue);
        QVERIFY(small.save(directory.filePath(QStringLiteral("b-small.bmp"))));

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileSize);
        qApp->setDualView(false);
        qApp->setOpenVolumeWithProgress(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QCOMPARE(session.currentPageName(), QStringLiteral("b-small.bmp"));

        // The size sort orders the pages differently from their file names, so
        // a lookup by name has to follow the order the volume displays.
        QVERIFY(session.openEntry(VolumeLocation{directory.path(), QStringLiteral("a-large.bmp")}));

        QCOMPARE(session.currentPageName(), QStringLiteral("a-large.bmp"));
        QCOMPARE(session.currentPageIndex(), 1);
    }

    void reloadingAContainerRereadsItsPages()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 2; ++page) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 40, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QCOMPARE(session.pageCount(), 2);
        QVERIFY(session.selectPage(1));

        // A file appears while the volume is open.
        QImage added(16, 24, QImage::Format_RGB32);
        added.fill(Qt::green);
        QVERIFY(added.save(directory.filePath(QStringLiteral("page-2.bmp"))));

        session.reloadContainer(directory.path());

        QCOMPARE(session.pageCount(), 3);
        QCOMPARE(session.currentPageName(), QStringLiteral("page-1.bmp"));
    }

    void openingFileAddedAfterTheListingScansAgain()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString firstPath = directory.filePath(QStringLiteral("page-0.bmp"));
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(firstPath));

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QCOMPARE(session.pageCount(), 1);

        // Add a file behind the volume's back and open it directly.
        const QString addedPath = directory.filePath(QStringLiteral("page-1.bmp"));
        image.fill(Qt::blue);
        QVERIFY(image.save(addedPath));
        QVERIFY(session.openFileInContainer(addedPath));

        QCOMPARE(session.stateKind(), ViewerStateKind::VolumeReady);
        QCOMPARE(session.pageCount(), 2);
        QCOMPARE(session.currentPageName(), QStringLiteral("page-1.bmp"));
    }

    void volumeKeepsItsOwnSortForPageNames()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 40, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));

        // Changing the setting does not re-sort this volume, so it must keep
        // reporting its own page names.
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByModifiedTime);
        session.updateReadProgress();
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);

        const QString volumePath = QDir::fromNativeSeparators(directory.path());
        QVERIFY(qApp->readProgressStore()->contains(volumePath));
        QCOMPARE(qApp->readProgressStore()->at(volumePath).currentPageName, QString("page-0.bmp"));
    }

    void cachedVolumesKeepIndependentPagePositions()
    {
        QTemporaryDir rootDirectory;
        QVERIFY(rootDirectory.isValid());
        const QString firstVolumePath = rootDirectory.filePath("first");
        const QString secondVolumePath = rootDirectory.filePath("second");
        QVERIFY(QDir().mkpath(firstVolumePath));
        QVERIFY(QDir().mkpath(secondVolumePath));
        for (const QString &volumePath : {firstVolumePath, secondVolumePath}) {
            for (int pageIndex = 0; pageIndex < 4; ++pageIndex) {
                QImage image(16, 24, QImage::Format_RGB32);
                image.fill(QColor::fromHsv(pageIndex * 40, 255, 255));
                QVERIFY(
                    image.save(QDir(volumePath).filePath(QString("page-%1.bmp").arg(pageIndex))));
            }
        }

        qApp->setOpenVolumeWithProgress(false);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(firstVolumePath));
        QVERIFY(session.selectPage(2));
        QVERIFY(session.openContainer(secondVolumePath));
        QVERIFY(session.selectPage(1));

        QVERIFY(session.openContainer(firstVolumePath));
        QCOMPARE(session.currentPageIndex(), 2);
        QCOMPARE(session.currentPageName(), QString("page-2.bmp"));
        QVERIFY(session.openContainer(secondVolumePath));
        QCOMPARE(session.currentPageIndex(), 1);
        QCOMPARE(session.currentPageName(), QString("page-1.bmp"));
    }

    void openTargetClassifiesPaths()
    {
        const QString folder = QDir::tempPath();
        const QString archivePath = QDir(folder).filePath(QStringLiteral("book.zip"));
        const QString imagePath = QDir(folder).filePath(QStringLiteral("page.jpg"));

        const OpenTarget archive = OpenTarget::forPath(archivePath);
        QCOMPARE(archive.intent, OpenIntent::Container);
        QCOMPARE(archive.location.containerPath, QDir::fromNativeSeparators(archivePath));
        QVERIFY(archive.location.isContainer());

        const OpenTarget image = OpenTarget::forPath(imagePath);
        QCOMPARE(image.intent, OpenIntent::FileInContainer);
        QCOMPARE(image.location.containerPath, QDir::fromNativeSeparators(folder));
        QCOMPARE(image.location.entryName, QStringLiteral("page.jpg"));

        QCOMPARE(volumeLocationDisplayText({archivePath, QStringLiteral("page.jpg")}),
                 QDir::toNativeSeparators(archivePath) + QStringLiteral(" (page.jpg)"));
        QCOMPARE(
            volumeLocationDisplayText({folder, QStringLiteral("page.jpg")}),
            QDir::toNativeSeparators(QDir(folder).absoluteFilePath(QStringLiteral("page.jpg"))));
    }

    void storedVolumeLocationKeepsLegacyArchiveForm()
    {
        const QString folder = QDir::tempPath();
        const QString archivePath = QDir(folder).filePath(QStringLiteral("book.zip"));

        const QString storedContainer = storeVolumeLocation(VolumeLocation{folder, QString()});
        QCOMPARE(storedContainer, QDir::fromNativeSeparators(folder));
        QVERIFY(loadStoredVolumeLocation(storedContainer).isContainer());

        // Releases before the typed location API wrote archive pages this way.
        const QString storedEntry = archivePath + QStringLiteral("::page.jpg");
        const VolumeLocation entry = loadStoredVolumeLocation(storedEntry);
        QCOMPARE(entry.containerPath, QDir::fromNativeSeparators(archivePath));
        QCOMPARE(entry.entryName, QStringLiteral("page.jpg"));
        QCOMPARE(storeVolumeLocation(entry), storedEntry);

        // Folder pages are stored as plain file paths.
        const QString imagePath = QDir(folder).filePath(QStringLiteral("page.jpg"));
        const QString storedImage = storeVolumeLocation(OpenTarget::forPath(imagePath).location);
        QCOMPARE(storedImage, QDir::fromNativeSeparators(imagePath));
        QVERIFY(loadStoredVolumeLocation(storedImage).isContainer());
    }

    void storedVolumeLocationHandlesNamesWithTheSeparator()
    {
        const QString folder = QDir::fromNativeSeparators(QDir::tempPath());
        const QString archivePath = QDir(folder).filePath(QStringLiteral("book.zip"));
        const QString entryName = QStringLiteral("pages/chapter::one.jpg");

        // The split happens at the first separator, so an archive entry may
        // contain the separator itself.
        const VolumeLocation entry{archivePath, entryName};
        const QString storedEntry = storeVolumeLocation(entry);
        QCOMPARE(storedEntry, archivePath + QStringLiteral("::") + entryName);
        const VolumeLocation loadedEntry = loadStoredVolumeLocation(storedEntry);
        QCOMPARE(loadedEntry.containerPath, archivePath);
        QCOMPARE(loadedEntry.entryName, entryName);

        // Known limitation: a folder page is stored as a plain path, so the
        // legacy parser cannot tell a name containing the separator from an
        // archive entry, and reads the one page back as two parts. The path is
        // spelled out instead of being built with QDir, because Windows reads
        // a colon inside a name as a drive letter.
        const QString plainPagePath = folder + QStringLiteral("/a::b.jpg");
        const VolumeLocation plainPage = loadStoredVolumeLocation(plainPagePath);
        QVERIFY(!plainPage.isContainer());
        QCOMPARE(plainPage.containerPath, folder + QStringLiteral("/a"));
        QCOMPARE(plainPage.entryName, QStringLiteral("b.jpg"));

#ifndef Q_OS_WIN
        // The path is classified by what it is, not by the separator inside
        // its name. Only the other platforms can name a file with a colon.
        const VolumeLocation imageLocation = OpenTarget::forPath(plainPagePath).location;
        QCOMPARE(imageLocation.containerPath, folder);
        QCOMPARE(imageLocation.entryName, QStringLiteral("a::b.jpg"));
        QCOMPARE(storeVolumeLocation(imageLocation), plainPagePath);
#endif
    }

    void invalidatingFolderCacheOpensRenamedFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 40, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        qApp->setOpenVolumeWithProgress(false);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&session);
        QVERIFY(session.openContainer(directory.path()));
        QVERIFY(session.selectPage(1));
        QCOMPARE(session.currentPageName(), QString("page-1.bmp"));

        // Renaming can move the file to another position in the page order.
        const QString renamedName = QStringLiteral("page-9.bmp");
        const QString renamedPath = directory.filePath(renamedName);
        QVERIFY(QFile::rename(directory.filePath(QString("page-1.bmp")), renamedPath));
        session.invalidateVolumeCache(directory.path());
        QVERIFY(session.openFileInContainer(renamedPath));

        QTRY_COMPARE(session.stateKind(), ViewerStateKind::StandalonePreview);
        session.notifyInitialImagePainted();
        QTRY_COMPARE(session.stateKind(), ViewerStateKind::VolumeReady);
        QCOMPARE(session.pageCount(), 3);
        QCOMPARE(session.currentPageName(), renamedName);
    }

    void removingDisplayedPageOpensItsNeighbour()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 40, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        qApp->setOpenVolumeWithProgress(false);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QVERIFY(session.selectPage(1));
        QCOMPARE(session.currentPageName(), QString("page-1.bmp"));

        QVERIFY(QFile::remove(directory.filePath(QString("page-1.bmp"))));
        session.reloadVolumeAfterImageRemoval();

        QCOMPARE(session.pageCount(), 2);
        QCOMPARE(session.stateKind(), ViewerStateKind::VolumeReady);
        QCOMPARE(session.currentPageName(), QString("page-2.bmp"));
    }

    void removingLastDisplayedPageOpensPreviousOne()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 40, 255, 255));
            QVERIFY(image.save(directory.filePath(QString("page-%1.bmp").arg(page))));
        }

        qApp->setOpenVolumeWithProgress(false);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QVERIFY(session.lastPage());
        QCOMPARE(session.currentPageName(), QString("page-2.bmp"));

        QVERIFY(QFile::remove(directory.filePath(QString("page-2.bmp"))));
        session.reloadVolumeAfterImageRemoval();

        QCOMPARE(session.pageCount(), 2);
        QCOMPARE(session.stateKind(), ViewerStateKind::VolumeReady);
        QCOMPARE(session.currentPageName(), QString("page-1.bmp"));
    }

    void removingOnlyPageLeavesEmptyViewer()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath(QStringLiteral("only.bmp"));
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));

        qApp->setOpenVolumeWithProgress(false);
        qApp->setDualView(false);
        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QCOMPARE(session.pageCount(), 1);

        QVERIFY(QFile::remove(imagePath));
        session.reloadVolumeAfterImageRemoval();

        QCOMPARE(session.stateKind(), ViewerStateKind::Empty);
        QCOMPARE(session.pageCount(), 0);
        QCOMPARE(session.currentPagePath(), QString());
    }

    void visiblePagesAreReadOnlySnapshots()
    {
        ViewerSession session(nullptr);
        int notificationCount = 0;
        VisiblePages latest;
        connect(&session, &ViewerSession::visiblePagesChanged, this, [&](VisiblePages pages) {
            ++notificationCount;
            latest = std::move(pages);
        });
        QVERIFY(session.appendVisiblePage(ImageContent("first.bmp", 0)));

        const VisiblePages pages = session.visiblePages();
        QCOMPARE(notificationCount, 1);
        QCOMPARE(latest.count(), 1);
        QCOMPARE(pages.count(), 1);
        QVERIFY(pages.at(-1) == nullptr);
        QVERIFY(pages.at(1) == nullptr);
        QVERIFY(pages.first() != nullptr);
        QCOMPARE(pages.first()->path, QString("first.bmp"));

        QVERIFY(session.appendVisiblePage(ImageContent("second.bmp", 0)));
        QCOMPARE(notificationCount, 2);
        QCOMPARE(latest.count(), 2);
        QVERIFY(!session.appendVisiblePage(ImageContent("third.bmp", 0)));
        QCOMPARE(notificationCount, 2);

        session.clearVisiblePages();
        QCOMPARE(notificationCount, 3);
        QVERIFY(latest.isEmpty());
        QVERIFY(session.visiblePages().isEmpty());
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first()->path, QString("first.bmp"));
    }

    void pagePresentationUpdatesUseAnExplicitOperation()
    {
        ViewerSession session(nullptr);
        QSignalSpy pageChangedSpy(&session, &ViewerSession::pageChanged);

        session.notifyPagePresentationChanged();

        QCOMPARE(pageChangedSpy.count(), 1);
    }

    void imageStringUsesValueSnapshots()
    {
        ImageString imageString;
        QCOMPARE(imageString.formatString("%p"), QString());

        ViewerSession session(nullptr);
        QImage image(100, 200, QImage::Format_RGB32);
        QVERIFY(
            session.appendVisiblePage(ImageContent(image, "sample.png", image.size(), {}, 1024)));
        imageString.initialize(&session, [] { return RenderedPageMetrics(QVector<qreal>{0.5}); });

        QCOMPARE(imageString.formatString("%p|%s|%m"), QString("sample.png|100x200|50%"));
    }

    void renderedPageUsesRenderSettingsSnapshot()
    {
        QGraphicsScene scene;
        QImage image(400, 400, QImage::Format_RGB32);
        image.fill(Qt::red);
        PageRenderSettings settings;
        settings.pixelRatio = 2.0;
        RenderedPage page(
            nullptr, &scene, ImageContent(image, "page.bmp", image.size(), {}, 0), settings);
        settings.pixelRatio = 3.0;

        page.setPageLayoutFitting(
            QRect(0, 0, 100, 100), RenderedPage::PageCenter, qvEnums::FitMode::FitToRect, 1.0);
        QCOMPARE(page.displayScale(), 0.5);

        page.setRenderSettings(settings);
        page.setPageLayoutFitting(
            QRect(0, 0, 100, 100), RenderedPage::PageCenter, qvEnums::FitMode::FitToRect, 1.0);
        QCOMPARE(page.displayScale(), 0.75);

        RenderedPage pageWithDefaults(
            nullptr, &scene, ImageContent(image, "preview.bmp", image.size(), {}, 0));
        pageWithDefaults.setPageLayoutFitting(
            QRect(0, 0, 100, 100), RenderedPage::PageCenter, qvEnums::FitMode::FitToRect, 1.0);
        QCOMPARE(pageWithDefaults.displayScale(), 0.25);
    }

    void cpuResizeGivesUpOnAnEmptyImage()
    {
        QElapsedTimer timer;
        timer.start();
        const QImage scaled =
            QZimg::scaled(QImage(), QSize(64, 64), Qt::IgnoreAspectRatio, QZimg::ResizeBicubic);
        const qint64 elapsedMilliseconds = timer.elapsed();

        QVERIFY(scaled.isNull());
        // Before the null check this slept through the allocation retries (~9 s).
        QVERIFY2(elapsedMilliseconds < 1000,
                 qPrintable(QStringLiteral("resize took %1 ms").arg(elapsedMilliseconds)));
    }

    void emptyVolumeOperationsAreSafe()
    {
        Volume volume(nullptr, std::make_unique<EmptyFileLoader>());

        QCOMPARE(volume.pageNameAt(0), QString());
        QCOMPARE(volume.pageIndexForName("missing.png"), -1);
        QCOMPARE(volume.pagePathAt(0), QString());
        QVERIFY(!volume.imageLoadAt(0).isValid());
        volume.updatePrefetchCache(0, PrefetchMode::Normal, QSize(100, 100));
        volume.moveToThread(nullptr);
    }

    void volumeSeparatesCoverAndThumbnailImageLoading()
    {
        auto coverLoader = std::make_unique<MemoryFileLoader>(3);
        MemoryFileLoader *coverLoaderPtr = coverLoader.get();
        Volume coverVolume(nullptr, std::move(coverLoader));
        coverVolume.prefetchCoverImages(0);
        // A repeated cover prefetch must not schedule the same pages twice.
        coverVolume.prefetchCoverImages(0);

        const Volume::ImageLoadFuture firstCoverLoad = coverVolume.imageLoadAt(0);
        const Volume::ImageLoadFuture secondCoverLoad = coverVolume.imageLoadAt(1);
        QVERIFY(firstCoverLoad.isValid());
        QVERIFY(secondCoverLoad.isValid());
        QVERIFY(!firstCoverLoad.result().loadedImage.isNull());
        QVERIFY(!secondCoverLoad.result().loadedImage.isNull());
        QVERIFY(!coverVolume.imageLoadAt(2).isValid());
        QStringList coverRequests = coverLoaderPtr->requestedNames();
        coverRequests.sort();
        QCOMPARE(coverRequests, QStringList({"page-0.bmp", "page-1.bmp"}));

        auto thumbnailLoader = std::make_unique<MemoryFileLoader>(3);
        MemoryFileLoader *thumbnailLoaderPtr = thumbnailLoader.get();
        Volume thumbnailVolume(nullptr, std::move(thumbnailLoader));
        const ImageContent thumbnailSource = thumbnailVolume.loadThumbnailSourceImage();

        QCOMPARE(thumbnailSource.path, QString("page-0.bmp"));
        QCOMPARE(thumbnailSource.loadedImage.size(), QSize(16, 24));
        QVERIFY(thumbnailSource.resizedImage.isNull());
        QCOMPARE(thumbnailLoaderPtr->requestedNames(), QStringList({"page-0.bmp"}));
    }

    void coverPrefetchFollowsTheSortedPageOrder()
    {
        const qvEnums::ImageSortBy previousSort = qApp->ImageSortBy();
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileSize);

        auto loader = std::make_unique<SizeSortedArchiveFileLoader>(3);
        MemoryFileLoader *loaderPtr = loader.get();
        Volume volume(nullptr, std::move(loader));
        volume.loadPageList();

        // The size sort reverses the order the loader lists the pages in, so the
        // page at index 0 is not the first entry of the loader's own list.
        QCOMPARE(volume.pageNameAt(0), QStringLiteral("page-2.bmp"));
        QCOMPARE(volume.pageNameAt(1), QStringLiteral("page-1.bmp"));
        QCOMPARE(volume.pageNameAt(2), QStringLiteral("page-0.bmp"));
        QCOMPARE(volume.pageIndexForName(QStringLiteral("page-2.bmp")), 0);
        QCOMPARE(volume.pageIndexForName(QStringLiteral("page-0.bmp")), 2);
        QCOMPARE(QFileInfo(volume.pagePathAt(0)).fileName(), QStringLiteral("page-2.bmp"));

        volume.prefetchCoverImages(0);
        QCOMPARE(volume.imageLoadAt(0).result().path, QStringLiteral("page-2.bmp"));
        QCOMPARE(volume.loadThumbnailSourceImage().path, QStringLiteral("page-2.bmp"));
        QVERIFY(loaderPtr->requestedNames().contains(QStringLiteral("page-2.bmp")));

        qApp->setImageSortBy(previousSort);
    }

    void volumeHandleDestroysOnOwnerThread()
    {
        auto *volume = new Volume(nullptr, std::make_unique<EmptyFileLoader>());
        QThread *destructionThread = nullptr;
        QObject::connect(
            volume,
            &QObject::destroyed,
            this,
            [&destructionThread] { destructionThread = QThread::currentThread(); },
            Qt::DirectConnection);

        VolumeHandle handle = makeVolumeHandle(volume);
        QFuture<void> release = QtConcurrent::run(
            [workerHandle = std::move(handle)]() mutable { workerHandle.reset(); });
        release.waitForFinished();

        QTRY_COMPARE(destructionThread, QThread::currentThread());
    }

    void activeVolumeSurvivesCacheEviction()
    {
        auto *volume = new Volume(nullptr, std::make_unique<EmptyFileLoader>());
        bool destroyed = false;
        QObject::connect(volume, &QObject::destroyed, this, [&destroyed] { destroyed = true; });

        VolumeHandle active = makeVolumeHandle(volume);
        VolumeCache cache(1);
        const VolumeCacheKey first{"first", false, false};
        const VolumeCacheKey second{"second", false, false};
        cache.insertReady(first, active);
        cache.insertReady(second, {});

        QVERIFY(!destroyed);
        active.reset();
        QTRY_VERIFY(destroyed);
    }

    void volumeCacheSharesLoadsAndDoesNotWaitWhenLookingUp()
    {
        VolumeCache cache(2);
        const VolumeCacheKey key{"shared", false, false};
        QPromise<CachedVolumeLoadResult> pendingLoad;
        pendingLoad.start();
        int startCount = 0;
        const auto startLoad = [&] {
            ++startCount;
            return pendingLoad.future();
        };

        const VolumeLoadFuture firstRequest = cache.request(key, startLoad);
        const VolumeLoadFuture secondRequest = cache.request(key, startLoad);

        QCOMPARE(startCount, 1);
        QVERIFY(firstRequest.isValid());
        QVERIFY(secondRequest.isValid());
        QVERIFY(!cache.findReady(key).volume);

        VolumeHandle loadedVolume =
            makeVolumeHandle(new Volume(nullptr, std::make_unique<EmptyFileLoader>()));
        pendingLoad.addResult({loadedVolume, ArchiveOpenError::None});
        pendingLoad.finish();

        QCOMPARE(cache.findReady(key).volume, loadedVolume);
        QVERIFY(cache.markUsed(key));
    }

    void volumeCacheRetriesFailedLoads()
    {
        VolumeCache cache(1);
        const VolumeCacheKey key{"retry", false, false};
        int startCount = 0;
        const auto failedLoad = [&] {
            ++startCount;
            QPromise<CachedVolumeLoadResult> promise;
            promise.start();
            VolumeLoadFuture future = promise.future();
            promise.addResult({});
            promise.finish();
            return future;
        };

        cache.request(key, failedLoad);
        QVERIFY(!cache.findReady(key).volume);
        QVERIFY(!cache.contains(key));
        cache.request(key, failedLoad);
        QCOMPARE(startCount, 2);
    }

    void emptyImageViewNavigationIsSafe()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        view.handleNextPageActionTriggered();
        view.handlePrevPageActionTriggered();
        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();
        view.handleFastForwardActionTriggered();
        view.handleFastBackwardActionTriggered();
        view.handleFirstPageActionTriggered();
        view.handleLastPageActionTriggered();
        view.handleNextOnePageActionTriggered();
        view.handlePrevOnePageActionTriggered();
        view.handleNextVolumeActionTriggered();
        view.handlePrevVolumeActionTriggered();
        view.handleRotateActionTriggered();
        view.handleSlideShowTimerTimeout();
        view.handleCopyPageActionTriggered();
        view.handleCopyFileActionTriggered();
    }

    void gestureStateIsIndependentBetweenViews()
    {
        ImageView firstView;
        ImageView secondView;

        firstView.updateGestureTransform(2.0, 0.0);
        secondView.updateGestureTransform(3.0, 0.0);
        firstView.commitGestureTransform();
        firstView.updateGestureTransform(1.0, 0.0);

        QCOMPARE(firstView.transform().m11(), 2.0);
        QCOMPARE(secondView.transform().m11(), 3.0);

        firstView.resetGestureTransform();
        QVERIFY(firstView.transform().isIdentity());
        QCOMPARE(secondView.transform().m11(), 3.0);
    }

    void cursorZoomMappingUsesBothScrollBarRanges()
    {
        const QSize viewportSize(400, 200);
        const QPoint minimum(10, 20);
        const QPoint maximum(110, 220);

        QCOMPARE(CursorScrollMapping::zoomScrollPosition(
                     QPoint(100, 50), viewportSize, minimum, maximum),
                 minimum);
        QCOMPARE(CursorScrollMapping::zoomScrollPosition(
                     QPoint(200, 100), viewportSize, minimum, maximum),
                 QPoint(60, 120));
        QCOMPARE(CursorScrollMapping::zoomScrollPosition(
                     QPoint(300, 150), viewportSize, minimum, maximum),
                 maximum);
    }

    void cursorLoupeMappingKeepsAnchorStable()
    {
        const std::optional<QPoint> position =
            CursorScrollMapping::loupeScrollPosition(QPoint(200, 150),
                                                     QPoint(200, 150),
                                                     QSize(400, 300),
                                                     QRect(0, 0, 400, 300),
                                                     QRectF(0, 0, 800, 600),
                                                     QPoint());

        QVERIFY(position);
        QCOMPARE(*position, QPoint(200, 150));
        QVERIFY(!CursorScrollMapping::loupeScrollPosition(QPoint(200, 150),
                                                          QPoint(0, 150),
                                                          QSize(400, 300),
                                                          QRect(0, 0, 400, 300),
                                                          QRectF(0, 0, 800, 600),
                                                          QPoint()));
    }

    void loupeControllerTracksActivationAndRestoration()
    {
        LoupeController loupe;
        const QRect contentRect(0, 0, 400, 300);
        const QPoint initialScrollPosition(25, 40);

        QVERIFY(!loupe.isActive());
        LoupeController::SceneUpdate update =
            loupe.prepareSceneUpdate(contentRect, initialScrollPosition);
        QVERIFY(!update.leavingLoupe);

        loupe.activate();
        QVERIFY(loupe.isActive());
        update = loupe.prepareSceneUpdate(QRect(0, 0, 800, 600), initialScrollPosition);
        QVERIFY(!update.leavingLoupe);
        QCOMPARE(update.scrollPositionToRestore, initialScrollPosition);

        loupe.setAnchorPosition(QPoint(200, 150));
        const std::optional<QPoint> mappedPosition = loupe.scrollPositionForCursor(
            QPoint(200, 150), QSize(400, 300), QRectF(0, 0, 800, 600));
        QVERIFY(mappedPosition);
        QCOMPARE(*mappedPosition, QPoint(250, 230));

        loupe.deactivate();
        QVERIFY(!loupe.isActive());
        update = loupe.prepareSceneUpdate(contentRect, QPoint(100, 120));
        QVERIFY(update.leavingLoupe);
        QCOMPARE(update.scrollPositionToRestore, initialScrollPosition);

        update = loupe.prepareSceneUpdate(contentRect, QPoint(100, 120));
        QVERIFY(!update.leavingLoupe);
    }

    void loupeControllerAdjustsScaleWithinLowerBound()
    {
        LoupeController loupe;

        QCOMPARE(loupe.scaleFactor(), 3.0);
        loupe.adjustScaleFromWheel(-120);
        loupe.adjustScaleFromWheel(-120);
        loupe.adjustScaleFromWheel(-120);
        loupe.adjustScaleFromWheel(-120);
        QCOMPARE(loupe.scaleFactor(), 1.5);

        loupe.adjustScaleFromWheel(120);
        QCOMPARE(loupe.scaleFactor(), 2.0);
        loupe.adjustScaleFromWheel(0);
        QCOMPARE(loupe.scaleFactor(), 2.0);
    }

    void loupeControllersKeepIndependentState()
    {
        LoupeController first;
        LoupeController second;

        first.activate();
        first.adjustScaleFromWheel(120);

        QVERIFY(first.isActive());
        QCOMPARE(first.scaleFactor(), 3.5);
        QVERIFY(!second.isActive());
        QCOMPARE(second.scaleFactor(), 3.0);
    }

    void standalonePreviewNavigationIsSafe()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        const QImage image(8, 8, QImage::Format_ARGB32);
        session.appendVisiblePage(ImageContent(image, "preview.png", image.size(), {}, 0));

        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();
        QCOMPARE(session.currentPageName(), QString("preview.png"));
    }

    void renderedPagesOwnItemsAndExposeSnapshots()
    {
        static_assert(!std::is_copy_constructible_v<RenderedPage>);
        static_assert(!std::is_copy_assignable_v<RenderedPage>);

        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        const QImage image(8, 4, QImage::Format_ARGB32);
        QCOMPARE(view.addRenderedPage(ImageContent(image, "first.png", image.size(), {}, 0), true),
                 ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 1);
        QCOMPARE(view.renderedPageContents().count(), 1);

        QCOMPARE(view.addRenderedPage(ImageContent(image, "second.png", image.size(), {}, 0), true),
                 ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 2);
        VisiblePages contents = view.renderedPageContents();
        QCOMPARE(contents.at(0)->path, QString("first.png"));
        QCOMPARE(contents.at(1)->path, QString("second.png"));
        QCOMPARE(view.addRenderedPage(ImageContent(image, "third.png", image.size(), {}, 0), true),
                 ImageView::AddRenderedPageResult::Rejected);

        view.clearRenderedPages();
        QCOMPARE(view.renderedPageCount(), 0);
        QVERIFY(view.renderedPageContents().isEmpty());

        QCOMPARE(view.addRenderedPage(ImageContent(image, "replacement.png", image.size(), {}, 0),
                                      false),
                 ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 1);
        QCOMPARE(
            view.addRenderedPage(ImageContent(image, "prepended.png", image.size(), {}, 0), false),
            ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 2);
        contents = view.renderedPageContents();
        QCOMPARE(contents.at(0)->path, QString("prepended.png"));
        QCOMPARE(contents.at(1)->path, QString("replacement.png"));
        QVERIFY(!contents.at(-1));
        QVERIFY(!contents.at(2));
    }

    void fittingModeRelayoutsRenderedPage()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&session);

        QImage image(640, 480, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(session.appendVisiblePage(ImageContent(image, "fitting.png", image.size(), {}, 0)));

        qApp->setFitting(false);
        view.refreshRenderedPages();
        QCOMPARE(view.renderedPageMetrics().notationalScaleAt(0), 1.0);

        view.handleFittingActionTriggered(true);
        QVERIFY(qApp->Fitting());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void fittingShortcutTriggersViewAction()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&session);

        QAction fittingAction;
        fittingAction.setCheckable(true);
        connect(
            &fittingAction, &QAction::triggered, &view, &ImageView::handleFittingActionTriggered);
        QAction *previousAction = qApp->keyActions().actions().value("actionFitting", nullptr);
        auto restoreAction = qScopeGuard(
            [previousAction]() { qApp->keyActions().actions()["actionFitting"] = previousAction; });
        QAction *fittingActionPtr = &fittingAction;
        qApp->keyActions().registerAction("actionFitting", fittingActionPtr, "Image");

        QImage image(640, 480, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(
            session.appendVisiblePage(ImageContent(image, "shortcut.png", image.size(), {}, 0)));

        qApp->setFitting(false);
        fittingAction.setChecked(false);
        view.refreshRenderedPages();
        QCOMPARE(view.renderedPageMetrics().notationalScaleAt(0), 1.0);

        QKeySequence fittingKey("M");
        QAction *mappedAction = qApp->keyActions().getActionByKey(fittingKey);
        QCOMPARE(mappedAction, &fittingAction);
        mappedAction->trigger();

        QVERIFY(fittingAction.isChecked());
        QVERIFY(qApp->Fitting());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void emptyDirectoryNavigationIsSafe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        QVERIFY(!session.openContainer(directory.path()));
        QCOMPARE(session.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(session.loadStatus().phase, ViewerLoadPhase::Failed);
        QCOMPARE(session.loadStatus().targetKind, LoadTargetKind::Folder);
        QCOMPARE(session.loadStatus().failureReason, LoadFailureReason::NoViewableImages);
        QCOMPARE(session.loadStatus().failurePath, QDir::toNativeSeparators(directory.path()));
        QCOMPARE(session.pageCount(), 0);
        QVERIFY(!session.firstPage());
        QVERIFY(!session.lastPage());
        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();

        session.reset();
        QCOMPARE(session.stateKind(), ViewerStateKind::Empty);
        QCOMPARE(session.loadStatus().phase, ViewerLoadPhase::Empty);
    }

    void emptyArchiveNavigationIsSafe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString archivePath = directory.filePath("empty.zip");
        QFile archive(archivePath);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        // Empty ZIP end-of-central-directory record.
        QCOMPARE(archive.write(QByteArray::fromHex("504b0506000000000000000000000000000000000000")),
                 qint64(22));
        archive.close();

        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        QVERIFY(!session.openContainer(archivePath));
        QCOMPARE(session.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(session.loadStatus().phase, ViewerLoadPhase::Failed);
        QCOMPARE(session.loadStatus().targetKind, LoadTargetKind::Archive);
        QCOMPARE(session.loadStatus().failureReason, LoadFailureReason::NoViewableImages);
        QCOMPARE(session.loadStatus().failurePath, QDir::toNativeSeparators(archivePath));
        QCOMPARE(session.pageCount(), 0);
        QVERIFY(!session.isArchive());
        QVERIFY(!session.firstPage());
        QVERIFY(!session.lastPage());
        QVERIFY(!session.reloadVisiblePages());
        QCOMPARE(session.currentPagePath(), QString());
        QCOMPARE(session.currentPageName(), QString());
        QCOMPARE(session.pageSignage(0), QString());
        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QVApplication application(argc, argv);
    ViewerNavigationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_viewernavigationtest.moc"
