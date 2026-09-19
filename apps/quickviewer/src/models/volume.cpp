#include <QtGui>
#include <atomic>
#include <cstring>
#include <memory>
#include <random>

#include "volume.h"
#include "ResizeHalf.h"
#include "qvapplication.h"
#include "qzimg.h"
#include "shadereffect.h"
#include "fileloader.h"
#include "boundedexecutor.h"
#include "imagedecoder.h"
#include "decodemetricsscope.h"
#include "imageformat.h"
#include "startupprofiler.h"

static std::atomic<quint64> nextPrefetchOwnerId{1};

static BoundedExecutor &imagePrefetchExecutor()
{
    static constexpr int InitialPrefetchThreads = 4;
    static constexpr int MaximumPendingPrefetchJobs = 128;
    static BoundedExecutor executor(qBound(1, QThread::idealThreadCount(), InitialPrefetchThreads),
                                    MaximumPendingPrefetchJobs);
    return executor;
}

static ImageContent enrichDetailedMetadata(const QSharedPointer<ImageLoadContext> &context,
                                           ImageContent content,
                                           const QString &path);

static QFuture<ImageContent> readyImageFuture(ImageContent content)
{
    QPromise<ImageContent> promise;
    promise.start();
    QFuture<ImageContent> future = promise.future();
    promise.addResult(std::move(content));
    promise.finish();
    return future;
}

Volume::Volume(QObject *parent, std::unique_ptr<IFileLoader> loader)
    : QObject(parent),
      m_imageLoadCache(qApp->MaxImagesCache()),
      m_previewLoadCache(qApp->MaxImagesCache()),
      m_loadContext(new ImageLoadContext(std::move(loader))),
      m_pageListLoaded(false),
      m_openedWithSpecifiedImageFile(false),
      m_prefetchOwnerId(nextPrefetchOwnerId.fetch_add(1, std::memory_order_relaxed)),
      m_prefetchGeneration(0),
      m_lastPrefetchAnchor(-1),
      m_lastPrefetchMode(PrefetchMode::Normal)
{
}

Volume::~Volume()
{
    m_imageLoadCache.clear();
    m_previewLoadCache.clear();
}

static BoundedExecutor::Priority prefetchPriorityForPage(int pageIndex, int anchorPageIndex)
{
    if (pageIndex < 0 || pageIndex == anchorPageIndex) {
        return BoundedExecutor::Priority::Critical;
    }
    const int distance = qAbs(pageIndex - anchorPageIndex);
    if (distance == 1) {
        return BoundedExecutor::Priority::High;
    }
    if (distance <= 3) {
        return BoundedExecutor::Priority::Normal;
    }
    return BoundedExecutor::Priority::Low;
}

Volume::ImageLoadFuture Volume::scheduleImageLoad(const QString &path,
                                                  const QSize &pageSize,
                                                  bool requiredForDisplay,
                                                  const QSize &decodeTargetSize,
                                                  bool loadDetailedMetadata,
                                                  int pageIndex,
                                                  quint64 generation)
{
    const QSharedPointer<ImageLoadContext> context = m_loadContext;
    auto submission = imagePrefetchExecutor().submit(
        [context, path, pageSize, decodeTargetSize, loadDetailedMetadata] {
            ImageContent content = futureLoadImageFromFileVolume(
                context, path, pageSize, decodeTargetSize, loadDetailedMetadata);
            content.isPreview = decodeTargetSize.isValid() && !decodeTargetSize.isEmpty();
            return content;
        },
        prefetchPriorityForPage(pageIndex, m_lastPrefetchAnchor),
        m_prefetchOwnerId,
        generation);
    if (submission.accepted) {
        return submission.future;
    }

    if (!requiredForDisplay) {
        return QFuture<ImageContent>();
    }

    return readyImageFuture(futureLoadImageFromFileVolume(
        context, path, pageSize, decodeTargetSize, loadDetailedMetadata));
}

Volume::ImageLoadFuture Volume::scheduleResize(ImageContent content, const QSize &pageSize)
{
    auto submission = imagePrefetchExecutor().submit([content, pageSize]() mutable {
        return resizeImageForViewport(std::move(content), pageSize);
    });
    if (submission.accepted) {
        return submission.future;
    }

    return QFuture<ImageContent>();
}

Volume::ImageLoadFuture
Volume::scheduleMetadataLoad(ImageContent content, const QString &path, quint64 generation)
{
    const QSharedPointer<ImageLoadContext> context = m_loadContext;
    auto submission = imagePrefetchExecutor().submit(
        [context, content = std::move(content), path]() mutable {
            return enrichDetailedMetadata(context, std::move(content), path);
        },
        BoundedExecutor::Priority::Critical,
        m_prefetchOwnerId,
        generation);
    return submission.accepted ? submission.future : QFuture<ImageContent>();
}

void Volume::loadPageList()
{
    StartupProfiler::mark("volume.page-list.begin");
    IFileLoader *loader = m_loadContext ? m_loadContext->loader() : nullptr;
    if (!loader) {
        m_pageNames.clear();
        m_pageListLoaded = true;
        return;
    }
    m_pageNames = loader->contents();
    m_pageListLoaded = true;
    applyPageSort(qApp->ImageSortBy());
    StartupProfiler::mark("volume.page-list.end");
}

static bool fileSizeLessThan(const ImageMetadata &m1, const ImageMetadata &m2)
{
    return m1.getFileSize() < m2.getFileSize();
}
static bool fileSizeDescendingLessThan(const ImageMetadata &m1, const ImageMetadata &m2)
{
    return m1.getFileSize() > m2.getFileSize();
}
static bool modifiedTimeLessThan(const ImageMetadata &m1, const ImageMetadata &m2)
{
    return m1.getMTime() < m2.getMTime();
}
static bool modifiedTimeDescendingLessThan(const ImageMetadata &m1, const ImageMetadata &m2)
{
    return m1.getMTime() > m2.getMTime();
}

void Volume::sortPages(qvEnums::ImageSortBy sortBy)
{
    applyPageSort(sortBy);
}

void Volume::applyPageSort(qvEnums::ImageSortBy sortBy)
{
    m_sortBy = sortBy;
    m_imageMetadataList.clear();
    if (sortBy != qvEnums::ImageSortBy::SortByFileName &&
        sortBy != qvEnums::ImageSortBy::SortByFileNameDescending) {
        foreach (const QString &fl, m_pageNames) {
            m_imageMetadataList << ImageMetadata(this, fl);
        }
    }
    switch (sortBy) {
    case qvEnums::ImageSortBy::SortByFileName: {
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(
            m_pageNames.begin(),
            m_pageNames.end(),
            [&collator](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
        break;
    }
    case qvEnums::ImageSortBy::SortByFileNameDescending: {
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(
            m_pageNames.begin(),
            m_pageNames.end(),
            [&collator](const QString &a, const QString &b) { return collator.compare(a, b) > 0; });
        break;
    }
    case qvEnums::ImageSortBy::SortByFileSize:
        std::stable_sort(m_imageMetadataList.begin(), m_imageMetadataList.end(), fileSizeLessThan);
        break;
    case qvEnums::ImageSortBy::SortByFileSizeDescending:
        std::stable_sort(
            m_imageMetadataList.begin(), m_imageMetadataList.end(), fileSizeDescendingLessThan);
        break;
    case qvEnums::ImageSortBy::SortByModifiedTime:
        std::stable_sort(
            m_imageMetadataList.begin(), m_imageMetadataList.end(), modifiedTimeLessThan);
        break;
    case qvEnums::ImageSortBy::SortByModifiedTimeDescending:
        std::stable_sort(
            m_imageMetadataList.begin(), m_imageMetadataList.end(), modifiedTimeDescendingLessThan);
        break;
    }
    m_imageLoadCache.clear();
    m_previewLoadCache.clear();
}

void Volume::startSlideShow()
{
    if (!qApp->SlideShowRandomly()) {
        return;
    }
    m_shuffledPageNames = m_pageNames;
    m_shuffledPageNames.detach();
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_shuffledPageNames.begin(), m_shuffledPageNames.end(), g);
    m_imageLoadCache.clear();
    m_previewLoadCache.clear();
}

void Volume::stopSlideShow()
{
    if (!qApp->SlideShowRandomly()) {
        return;
    }
    m_shuffledPageNames.clear();
    m_imageLoadCache.clear();
    m_previewLoadCache.clear();
}

QString Volume::pageNameAt(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= m_pageNames.size()) {
        return "";
    }
    if (!m_shuffledPageNames.isEmpty()) {
        return m_shuffledPageNames[pageIndex];
    }
    if (m_sortBy == qvEnums::ImageSortBy::SortByFileName ||
        m_sortBy == qvEnums::ImageSortBy::SortByFileNameDescending) {
        return m_pageNames[pageIndex];
    } else if (pageIndex < m_imageMetadataList.size()) {
        return m_imageMetadataList[pageIndex].filename();
    }
    return "";
}

int Volume::pageIndexForName(const QString &name) const
{
    const QString nativeName = QDir::toNativeSeparators(name);
    if (!m_shuffledPageNames.isEmpty()) {
        return m_shuffledPageNames.indexOf(nativeName);
    }
    // The metadata sorts reorder the metadata list and leave the name list in
    // the order the loader reported, so the lookup has to use the list that
    // pageNameAt() reads from.
    if (m_sortBy == qvEnums::ImageSortBy::SortByFileName ||
        m_sortBy == qvEnums::ImageSortBy::SortByFileNameDescending) {
        return m_pageNames.indexOf(nativeName);
    }
    for (int pageIndex = 0; pageIndex < m_imageMetadataList.size(); ++pageIndex) {
        if (m_imageMetadataList[pageIndex].filename() == nativeName) {
            return pageIndex;
        }
    }
    return -1;
}

static int recommendedPrefetchConcurrency(const IFileLoader *loader)
{
    const int idealThreads = qMax(1, QThread::idealThreadCount());
    if (loader && loader->isArchive()) {
        return qBound(1, (idealThreads + 3) / 4, 4);
    }
    return qBound(2, (idealThreads + 1) / 2, 8);
}

static QSize previewDecodeSize(const QSize &viewportSize)
{
    const int maxTextureSize = qApp->MaxTextureSize();
    if (!viewportSize.isValid() || viewportSize.isEmpty()) {
        const int fallback = qMin(maxTextureSize, 2048);
        return QSize(fallback, fallback);
    }

    const QSize oversampled(qMin(maxTextureSize, qMax(1, viewportSize.width() * 3 / 2)),
                            qMin(maxTextureSize, qMax(1, viewportSize.height() * 3 / 2)));
    return oversampled;
}

static bool shouldPrefetchFullResolution(int pageIndex, int anchorPageIndex)
{
    return qAbs(pageIndex - anchorPageIndex) <= 3;
}

void Volume::updatePrefetchCache(int anchorPageIndex, PrefetchMode mode, QSize viewportSize)
{
    IFileLoader *loader = loaderForPrefetch();
    if (!loader || anchorPageIndex < 0 || anchorPageIndex >= m_pageNames.size()) {
        return;
    }

    imagePrefetchExecutor().setMaximumConcurrency(recommendedPrefetchConcurrency(loader));

    if (anchorPageIndex != m_lastPrefetchAnchor || mode != m_lastPrefetchMode) {
        m_lastPrefetchAnchor = anchorPageIndex;
        m_lastPrefetchMode = mode;
        ++m_prefetchGeneration;
        imagePrefetchExecutor().cancelPendingOlderThan(m_prefetchOwnerId, m_prefetchGeneration);
    }

    const QList<int> indexes =
        PrefetchPlanner::indexes(mode, anchorPageIndex, m_pageNames.size(), qApp->MaxImagesCache());
    for (int cnt : indexes) {
        const bool fullResolution = shouldPrefetchFullResolution(cnt, anchorPageIndex);
        if (fullResolution) {
            m_previewLoadCache.remove(cnt);
        } else if (m_imageLoadCache.touch(cnt)) {
            continue;
        }
        LruCache<int, ImageLoadFuture> &cache =
            fullResolution ? m_imageLoadCache : m_previewLoadCache;
        ImageLoadFuture *cachedImageLoad = cache.find(cnt);
        if (cachedImageLoad && cachedImageLoad->isCanceled()) {
            cache.remove(cnt);
            cachedImageLoad = nullptr;
        }
        if (fullResolution && cnt == anchorPageIndex && cachedImageLoad &&
            cachedImageLoad->isFinished()) {
            ImageContent cachedImage = cachedImageLoad->result();
            if (!cachedImage.hasDetailedMetadata &&
                IFileLoader::isExifJpegImageFile(cachedImage.path)) {
                const QString metadataPath = cachedImage.path;
                const ImageLoadFuture metadataLoad = scheduleMetadataLoad(
                    std::move(cachedImage), metadataPath, m_prefetchGeneration);
                if (metadataLoad.isValid()) {
                    cache.insert(cnt, metadataLoad);
                    cachedImageLoad = cache.find(cnt);
                }
            }
        }
        if (fullResolution && resizesOnCpu(qApp->Effect()) && cachedImageLoad &&
            cachedImageLoad->isFinished()) {
            ImageContent cachedImage = cachedImageLoad->result();
            if (cachedImage.loadedImageSize.isValid()) {
                const QSize pageSize = viewportSize;
                QSize resized = cachedImage.orientedSize(pageSize);
                resized.setWidth(cachedImage.loadedImageSize.width() * resized.height() /
                                 cachedImage.loadedImageSize.height());

                if (cachedImage.resizedImage.size() != resized &&
                    !cachedImage.loadedImage.isNull()) {
                    const ImageLoadFuture future = scheduleResize(cachedImage, pageSize);
                    if (future.isValid()) {
                        cache.insert(cnt, future);
                    }
                }
            }
        }
        if (!cache.touch(cnt)) {
            const QSize pageSize =
                fullResolution && resizesOnCpu(qApp->Effect()) ? viewportSize : QSize();
            const QSize decodeTargetSize =
                fullResolution ? QSize() : previewDecodeSize(viewportSize);
            const ImageLoadFuture future =
                scheduleImageLoad(pageNameAt(cnt),
                                  pageSize,
                                  cnt == anchorPageIndex,
                                  decodeTargetSize,
                                  cnt == anchorPageIndex || loader->isArchive(),
                                  cnt,
                                  m_prefetchGeneration);
            if (future.isValid()) {
                cache.insert(cnt, future);
            }
        }
    }
}

void Volume::prefetchCoverImages(int anchorPageIndex)
{
    IFileLoader *loader = loaderForPrefetch();
    if (!loader || anchorPageIndex < 0 || anchorPageIndex >= m_pageNames.size()) {
        return;
    }
    imagePrefetchExecutor().setMaximumConcurrency(recommendedPrefetchConcurrency(loader));
    for (int pageIndex :
         PrefetchPlanner::indexes(PrefetchMode::Normal, anchorPageIndex, m_pageNames.size(), 2)) {
        // A page the viewer already scheduled keeps its load; replacing it would
        // start a second decode, because eviction does not cancel the future.
        const ImageLoadFuture *cached = m_imageLoadCache.find(pageIndex);
        if (cached && !cached->isCanceled()) {
            m_imageLoadCache.touch(pageIndex);
            continue;
        }
        const ImageLoadFuture future =
            scheduleImageLoad(pageNameAt(pageIndex),
                              QSize(),
                              pageIndex == anchorPageIndex,
                              QSize(),
                              pageIndex == anchorPageIndex || loader->isArchive(),
                              pageIndex,
                              m_prefetchGeneration);
        if (future.isValid()) {
            m_imageLoadCache.insert(pageIndex, future);
        }
    }
}

ImageContent Volume::loadThumbnailSourceImage()
{
    if (!loaderForPrefetch()) {
        return ImageContent();
    }
    return futureLoadImageFromFileVolume(m_loadContext, pageNameAt(0), QSize());
}

IFileLoader *Volume::loaderForPrefetch()
{
    if (!m_pageListLoaded) {
        loadPageList();
    }
    IFileLoader *loader = m_loadContext ? m_loadContext->loader() : nullptr;
    if (!loader || m_pageNames.isEmpty() || loader->contents().isEmpty()) {
        return nullptr;
    }
    return loader;
}

Volume::ImageLoadFuture Volume::imageLoadAt(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= m_pageNames.size()) {
        return {};
    }
    const ImageLoadFuture *imageLoad = m_imageLoadCache.find(pageIndex);
    return imageLoad ? *imageLoad : ImageLoadFuture();
}

void Volume::moveToThread(QThread *targetThread)
{
    if (!targetThread) {
        return;
    }
    QObject::moveToThread(targetThread);
}

static int parseJpegOrientation(const QByteArray &bytes)
{
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype size = bytes.size();
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return 1;
    }

    auto readBigEndian16 = [](const unsigned char *p) -> quint16 {
        return static_cast<quint16>((p[0] << 8) | p[1]);
    };
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
        const quint16 segmentLength = readBigEndian16(data + offset);
        if (segmentLength < 2 || offset + segmentLength > size) {
            break;
        }
        const unsigned char *payload = data + offset + 2;
        const qsizetype payloadSize = segmentLength - 2;
        if (marker == 0xE1 && payloadSize >= 14 && std::memcmp(payload, "Exif\0\0", 6) == 0) {
            const unsigned char *tiff = payload + 6;
            const qsizetype tiffSize = payloadSize - 6;
            const bool littleEndian = tiffSize >= 8 && tiff[0] == 'I' && tiff[1] == 'I';
            const bool bigEndian = tiffSize >= 8 && tiff[0] == 'M' && tiff[1] == 'M';
            if (!littleEndian && !bigEndian) {
                return 1;
            }
            auto read16 = [littleEndian](const unsigned char *p) -> quint16 {
                return littleEndian ? static_cast<quint16>(p[0] | (p[1] << 8))
                                    : static_cast<quint16>((p[0] << 8) | p[1]);
            };
            auto read32 = [littleEndian](const unsigned char *p) -> quint32 {
                return littleEndian
                           ? static_cast<quint32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24))
                           : static_cast<quint32>((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
            };
            if (read16(tiff + 2) != 42) {
                return 1;
            }
            // The offset is a 32-bit field, so the bounds check has to widen it
            // before adding: near 4 GiB it used to wrap and read past the segment.
            const qint64 ifdOffset = read32(tiff + 4);
            if (ifdOffset + 2 > tiffSize) {
                return 1;
            }
            const unsigned char *ifd = tiff + ifdOffset;
            const quint16 entryCount = read16(ifd);
            for (quint16 i = 0; i < entryCount; ++i) {
                const qsizetype entryOffset = 2 + static_cast<qsizetype>(i) * 12;
                if (ifdOffset + entryOffset + 12 > tiffSize) {
                    break;
                }
                const unsigned char *entry = ifd + entryOffset;
                if (read16(entry) == 0x0112 && read16(entry + 2) == 3 && read32(entry + 4) == 1) {
                    const int orientation = read16(entry + 8);
                    return orientation >= 1 && orientation <= 8 ? orientation : 1;
                }
            }
            return 1;
        }
        offset += segmentLength;
    }
    return 1;
}

static void parseExifTextExtents(QImage &img, easyexif::EXIFInfo &info)
{
    info.Make = img.text("Make").toStdString();
    info.Model = img.text("Model").toStdString();
    info.Orientation = img.text("Software").toInt();
    info.BitsPerSample = img.text("BitsPerSample").toInt();
    info.Software = img.text("Software").toStdString();
    info.DateTime = img.text("DateTime").toStdString();
    info.ExposureTime = img.text("ExposureTime").toDouble();
    info.FNumber = img.text("FNumber").toDouble();
    info.ISOSpeedRatings = img.text("ISOSpeedRatings").toInt();
    info.Flash = img.text("Flash").toInt();
    info.ImageWidth = img.text("ImageWidth").toInt();
    info.ImageHeight = img.text("ImageHeight").toInt();
}

static bool shouldUseDecoderScaling(ImageFormat format, const QImageReader &reader)
{
    const bool hotRaster = format == ImageFormat::Jpeg || format == ImageFormat::WebP;
    return hotRaster && reader.supportsOption(QImageIOHandler::ScaledSize);
}

/**
 * Qt format name to read `path` with. Only the formats whose registered plugin
 * depends on the user's decoder preference need a name of their own; everything
 * else is handed to Qt under the suffix it was found with. An empty name asks Qt
 * to detect the format instead, which is what the decoder fallback relies on.
 */
static QByteArray
qtFormatNameFor(const QString &path, ImageFormat format, const ImageDecodePolicy &policy)
{
    switch (format) {
    case ImageFormat::Jpeg:
        if (policy.jpeg == JpegDecoderPreference::Auto) {
            return IFileLoader::jpegQtFormatName();
        }
        return QByteArrayLiteral("jpg");
    case ImageFormat::Png:
        return IFileLoader::supportsImageFormat("apng") ? QByteArrayLiteral("apng")
                                                        : QByteArrayLiteral("png");
    case ImageFormat::Apng:
        return QByteArrayLiteral("apng");
    case ImageFormat::WebP:
        return QByteArrayLiteral("webp");
    default:
        return QFileInfo(path.toLower()).suffix().toUtf8();
    }
}

/**
 * Qt format names to try, in the order they are tried, for bytes whose
 * preferred name is `preferredQtFormatName`. The empty name, which lets Qt
 * detect the format from the bytes, is always last: it is the only candidate
 * that can read a file whose suffix matches no registered handler.
 */
static QList<QByteArray> qtFormatNameCandidates(const QByteArray &preferredQtFormatName)
{
    QList<QByteArray> candidates{preferredQtFormatName};
    // An APNG-capable handler that will not animate these bytes cannot serve
    // them, so the still-image handler gets a turn before Qt decides for itself.
    if (preferredQtFormatName == QByteArrayLiteral("apng")) {
        candidates.append(IFileLoader::supportsImageFormat("lodepng") ? QByteArrayLiteral("lodepng")
                                                                      : QByteArrayLiteral("png"));
    }
    if (!candidates.contains(QByteArray())) {
        candidates.append(QByteArray());
    }
    return candidates;
}

static ImageDecodeSettings currentImageDecodeSettings(int maxTextureSize)
{
    ImageDecodeSettings settings;
    settings.maxTextureSize = maxTextureSize;
    settings.fastDctForJpeg = qApp->UseFastDCTForJPEG();
    settings.svgRasterMaximum =
        QSize(qApp->SvgRasterMaximumWidth(), qApp->SvgRasterMaximumHeight());
    settings.svgLoaderBackend = qApp->SvgLoaderBackend();
    return settings;
}

// Pixels from the native backend chosen for the format, or `decoded == false`
// when the policy asks for the Qt reader or the backend rejected the bytes.
struct NativeDecodeOutcome
{
    QImage image;
    QSize sourceSize;
    bool decoded = false;
};

// One native decoder call, timed the way the branches it replaces were. The
// backend name is built only when metrics are collected and the call succeeded.
template <typename DecodeFn>
static bool measuredNativeDecode(ImageDecodeMetrics *metrics,
                                 const char *backendName,
                                 ImageDecodeOutput &output,
                                 DecodeFn &&decode)
{
    ImageDecodeDetail::DecodeMetricsScope<> decodeMetrics(metrics);
    const bool decoded = decode(output);
    decodeMetrics.finish();
    decodeMetrics.recordBackendOnSuccess(
        decoded, [backendName] { return QString::fromLatin1(backendName); });
    return decoded;
}

/**
 * Runs the native backend the policy selects for `format`, if that format has
 * one. Which backends exist, and when they are tried, stays with the caller of
 * ImageDecoder, which only knows how to run one backend at a time.
 */
static NativeDecodeOutcome tryNativeDecode(const ImageDecoder &decoder,
                                           const QByteArray &bytes,
                                           ImageFormat format,
                                           const ImageDecodePolicy &decodePolicy,
                                           QSize decodeTargetSize,
                                           ImageDecodeMetrics *metrics)
{
    ImageDecodeOutput output;
    if (format == ImageFormat::Jpeg && decodePolicy.jpeg != JpegDecoderPreference::Qt) {
        if (!measuredNativeDecode(metrics, "turbojpeg", output, [&](ImageDecodeOutput &target) {
                return decoder.decodeTurboJpeg(bytes, decodeTargetSize, target);
            })) {
            return {};
        }
    } else if ((format == ImageFormat::Png || format == ImageFormat::Apng) &&
               decodePolicy.png != PngDecoderPreference::Qt) {
        if (!measuredNativeDecode(metrics, "libspng", output, [&](ImageDecodeOutput &target) {
                return decoder.decodeSpng(bytes, target);
            })) {
            return {};
        }
    } else if (format == ImageFormat::WebP && decodePolicy.webp != WebPDecoderPreference::Qt) {
        if (!measuredNativeDecode(metrics, "libwebp", output, [&](ImageDecodeOutput &target) {
                return decoder.decodeWebP(bytes, decodeTargetSize, target);
            })) {
            return {};
        }
    } else {
        return {};
    }

    return {std::move(output.image), output.sourceSize, true};
}

/**
 * Trims `src` to the alignment the half-size resize needs. QImage copies can
 * fail while other decodes hold memory, so they are retried here; the caller
 * gives up on the image when they keep failing. Returns false in that case.
 */
static bool cropForHalfResize(QImage &src, const QString &path)
{
    QImage src2;
    switch (src.depth()) {
    case 32:
        if ((src.width() & 0x3) != 0 || (src.height() & 0x1) != 0) {
            // QImage processing sometimes fails
            for (int count = 1;; count++) {
                src2 = src.copy(QRect(0, 0, src.width() >> 2 << 2, src.height() >> 1 << 1));
                if (!src2.isNull()) {
                    break;
                }
                qDebug() << "[2]" << path << src2 << count;
                if (count >= 100) {
                    return false;
                }
                QThread::currentThread()->usleep(40000);
            }
            src.swap(src2);
        }
        break;
    default:
        if (src.format() != QImage::Format::Format_Grayscale8 &&
            src.format() != QImage::Format::Format_RGB888) {
            src = src.convertToFormat(QImage::Format::Format_RGB888);
        }
        if ((src.width() & 0xF) != 0 || (src.height() & 0x1) != 0) {
            // QImage processing sometimes fails
            int count = 0;
            do {
                src2 = src.copy(QRect(0, 0, src.width() >> 4 << 4, src.height() >> 1 << 1));
                qDebug() << "[2]" << path << src2 << count;
                if (!src2.isNull()) {
                    break;
                }
                if (src2.isNull() && count++ < 1000) {
                    QThread::currentThread()->usleep(1000);
                    continue;
                }
                return false;
            } while (1);
            src.swap(src2);
        }
        break;
    }
    return true;
}

// Metadata and display preparation are shared by native and Qt static-image decoders.
// Keep this outside decode timing and Qt format-name negotiation.
static ImageContent finishStaticImage(QImage src,
                                      QSize baseSize,
                                      const QString &path,
                                      const QByteArray &bytes,
                                      QSize pageSize,
                                      QSize decodeTargetSize,
                                      bool loadDetailedMetadata,
                                      int maxTextureSize)
{
    easyexif::EXIFInfo info;
    ImageContent ic(path, bytes.length());
    const bool isExifJpeg = src.width() > 0 && IFileLoader::isExifJpegImageFile(path);
    if (isExifJpeg) {
        if (loadDetailedMetadata) {
            info.parseFrom(reinterpret_cast<const unsigned char *>(bytes.constData()),
                           bytes.length());
        } else {
            info.Orientation = parseJpegOrientation(bytes);
        }
    }

    if (src.width() > 0 && IFileLoader::isExifRawImageFile(path)) {
        parseExifTextExtents(src, info);
    }

    ic.originalSize = baseSize;
    ic.exifInfo = info;
    if (src.isNull()) {
        return ic;
    }
    if (qApp->DontShrinkForLargeImage() ||
        (src.width() <= maxTextureSize && src.height() <= maxTextureSize)) {
        ic = ImageContent(src, path, baseSize, info, bytes.length());
    } else {
        // resample for too big images
        if (!cropForHalfResize(src, path)) {
            return ImageContent(path, bytes.length());
        }

        QSize srcSize = src.size();
        QSize halfSize = QSize((srcSize.width()) / 2, (srcSize.height()) / 2);

        QImage half = QImage(halfSize.width(), halfSize.height(), src.format());
        ResizeHalf::FMT fmt = (ResizeHalf::FMT)(src.depth() >> 3);
        ResizeHalf resizer(fmt);
        resizer.resizeHV(half.bits(),
                         src.bits(),
                         src.width(),
                         srcSize.height(),
                         half.bytesPerLine(),
                         src.bytesPerLine());

        ic.loadedImage = half;
        ic.loadedImageSize = half.size();
    }
    if (decodeTargetSize.isValid() && !decodeTargetSize.isEmpty() && !ic.loadedImage.isNull() &&
        (ic.loadedImage.width() > decodeTargetSize.width() ||
         ic.loadedImage.height() > decodeTargetSize.height())) {
        ic.loadedImage =
            ic.loadedImage.scaled(decodeTargetSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        ic.loadedImageSize = ic.loadedImage.size();
    }
    ic.hasDetailedMetadata = loadDetailedMetadata || !isExifJpeg;

    // CPU resizing before Page Viewing
    if (!pageSize.isEmpty() && !ic.loadedImage.isNull()) {
        const QSize newsize = ic.orientedSize(pageSize);
        ic.appliedResizeMode = qApp->Effect();
        ic.resizedImage = QZimg::scaled(
            ic.loadedImage, newsize, Qt::KeepAspectRatio, cpuFilterMode(qApp->Effect()));
    }
    return ic;
}

static ImageContent loadWithSpecifiedFormat(QString path,
                                            QSize pageSize,
                                            QSize decodeTargetSize,
                                            bool loadDetailedMetadata,
                                            QByteArray bytes,
                                            ImageFormat format,
                                            QByteArray preferredQtFormatName,
                                            const ImageDecodePolicy &decodePolicy,
                                            ImageDecodeMetrics *metrics)
{
    const int maxTextureSize = qApp->MaxTextureSize();
    const ImageDecoder decoder(currentImageDecodeSettings(maxTextureSize));
    if (format == ImageFormat::Svg) {
        ImageDecodeDetail::DecodeMetricsScope<> decodeMetrics(metrics);
        const ImageDecodeOutput output = decoder.decodeSvg(bytes, path);
        // SVG records its backend even when rasterization fails.
        decodeMetrics.recordBackend([] { return QStringLiteral("svgloader"); });
        decodeMetrics.finish();
        ImageContent ic(
            output.image, path, output.sourceSize, easyexif::EXIFInfo(), bytes.length());
        ic.hasDetailedMetadata = true;
        return ic;
    }

    NativeDecodeOutcome native =
        tryNativeDecode(decoder, bytes, format, decodePolicy, decodeTargetSize, metrics);
    if (native.decoded) {
        return finishStaticImage(std::move(native.image),
                                 native.sourceSize,
                                 path,
                                 bytes,
                                 pageSize,
                                 decodeTargetSize,
                                 loadDetailedMetadata,
                                 maxTextureSize);
    }

    // No native backend produced pixels, so read the bytes with Qt instead. Each
    // candidate format name gets one attempt; a name the reader cannot use hands
    // over to the next candidate, and everything else decides the result.
    for (const QByteArray &qtFormatName : qtFormatNameCandidates(preferredQtFormatName)) {
        QImage src;
        QSize baseSize;
        ImageContent ic(path, bytes.length());
        QBuffer buffer(&bytes);
        QImageReader reader(&buffer, qtFormatName);

        if (!reader.canRead()) {
            continue;
        }

        if (reader.supportsAnimation()) {
            ImageDecodeDetail::DecodeMetricsScope<> decodeMetrics(metrics);
            Movie movie = Movie(bytes, QString::fromUtf8(qtFormatName));
            // Movie construction records the backend before lazy frame decoding.
            decodeMetrics.recordBackend(
                [&] { return QString("qmovie:%1").arg(QString::fromLatin1(reader.format())); });
            decodeMetrics.finish();
            ic.movie = movie;
            ic.originalSize = ic.loadedImageSize = reader.size();
            ic.hasDetailedMetadata = true;
            return ic;
        }
        if (qtFormatName == QByteArrayLiteral("apng")) {
            // Readable, but this handler will not animate these bytes, so let
            // the still-image candidate read them instead.
            continue;
        }
        baseSize = reader.size();
        QSize loadingSize = baseSize;
        if (reader.format() == IFileLoader::turboJpegFormatName() && !qApp->UseFastDCTForJPEG()) {
            reader.setQuality(0);
        }
        if (shouldUseDecoderScaling(format, reader)) {
            const QSize targetSize =
                ImageDecoder::constrainedDecodeSize(baseSize, decodeTargetSize, maxTextureSize);
            if (targetSize.isValid() && targetSize != baseSize) {
                loadingSize = targetSize;
                reader.setScaledSize(loadingSize);
            }
        }

        ImageDecodeDetail::DecodeMetricsScope<> decodeMetrics(metrics);
        QImage tmp = ImageDecoder::readWithQt(reader, path, format == ImageFormat::Tiff);
        decodeMetrics.recordBackendOnSuccess(!tmp.isNull(), [&] {
            return QString("qimagereader:%1").arg(QString::fromLatin1(reader.format()));
        });
        decodeMetrics.finish();
        if (tmp.isNull()) {
            return ic;
        }
        if (baseSize.isEmpty()) {
            baseSize = loadingSize = tmp.size();
        }
        if (tmp.format() == QImage::Format_ARGB32 || tmp.format() == QImage::Format_RGB32) {
            src = std::move(tmp);
        } else {
            src = QZimg::toPackedImage(tmp);
        }
        if (src.isNull()) {
            return ImageContent(path, bytes.length());
        }

        return finishStaticImage(std::move(src),
                                 baseSize,
                                 path,
                                 bytes,
                                 pageSize,
                                 decodeTargetSize,
                                 loadDetailedMetadata,
                                 maxTextureSize);
    }
    return ImageContent(path, bytes.length());
}

ImageContent Volume::decodeImageBytes(const QString &path,
                                      const QByteArray &bytes,
                                      QSize pageSize,
                                      QSize decodeTargetSize,
                                      bool loadDetailedMetadata,
                                      const ImageDecodePolicy &decodePolicy,
                                      ImageDecodeMetrics *metrics)
{
    if (metrics) {
        *metrics = ImageDecodeMetrics();
    }
    if (bytes.isNull() || bytes.isEmpty()) {
        return ImageContent();
    }

    QElapsedTimer pipelineTimer;
    if (metrics) {
        pipelineTimer.start();
    }

    const ImageFormat format =
        IFileLoader::isExifJpegImageFile(path) ? ImageFormat::Jpeg : imageFormatFromPath(path);
    const QByteArray preferredQtFormatName = qtFormatNameFor(path, format, decodePolicy);

    ImageContent content = loadWithSpecifiedFormat(path,
                                                   pageSize,
                                                   decodeTargetSize,
                                                   loadDetailedMetadata,
                                                   bytes,
                                                   format,
                                                   preferredQtFormatName,
                                                   decodePolicy,
                                                   metrics);
    if (metrics) {
        metrics->pipelineNanoseconds = pipelineTimer.nsecsElapsed();
    }
    return content;
}

static ImageContent enrichDetailedMetadata(const QSharedPointer<ImageLoadContext> &context,
                                           ImageContent content,
                                           const QString &path)
{
    if (content.hasDetailedMetadata || !IFileLoader::isExifJpegImageFile(path)) {
        content.hasDetailedMetadata = true;
        return content;
    }

    const QByteArray bytes = context ? context->load(path) : QByteArray();
    if (!bytes.isEmpty()) {
        content.exifInfo.parseFrom(reinterpret_cast<const unsigned char *>(bytes.constData()),
                                   bytes.length());
    }
    content.hasDetailedMetadata = true;
    return content;
}

static ImageContent
futureLoadImageFromFileVolumeImpl(const QSharedPointer<ImageLoadContext> &context,
                                  QString path,
                                  QSize pageSize,
                                  QSize decodeTargetSize,
                                  bool loadDetailedMetadata)
{
    StartupProfiler::mark("image-worker.extract.begin");
    const QByteArray bytes = context->load(path);
    StartupProfiler::mark("image-worker.extract.end");
    ImageContent content =
        Volume::decodeImageBytes(path, bytes, pageSize, decodeTargetSize, loadDetailedMetadata);
    StartupProfiler::mark("image-worker.decode-resize.end");
    return content;
}

ImageContent Volume::futureLoadImageFromFileVolume(QSharedPointer<ImageLoadContext> context,
                                                   QString path,
                                                   QSize pageSize,
                                                   QSize decodeTargetSize,
                                                   bool loadDetailedMetadata)
{
    return futureLoadImageFromFileVolumeImpl(
        context, path, pageSize, decodeTargetSize, loadDetailedMetadata);
}

ImageContent Volume::loadImageFromFile(QString path,
                                       QSize pageSize,
                                       QSize decodeTargetSize,
                                       bool loadDetailedMetadata)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return ImageContent();
    }

    return decodeImageBytes(path, file.readAll(), pageSize, decodeTargetSize, loadDetailedMetadata);
}

ImageContent Volume::resizeImageForViewport(ImageContent content, QSize pageSize)
{
    const QSize targetSize = content.orientedSize(pageSize);
    content.appliedResizeMode = qApp->Effect();
    content.resizedImage = QZimg::scaled(
        content.loadedImage, targetSize, Qt::KeepAspectRatio, cpuFilterMode(qApp->Effect()));
    return content;
}
