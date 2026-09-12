#include <QtGui>
#include <QLibrary>
#include <atomic>
#include <climits>
#include <cstring>
#include <limits>
#include <random>

#include "volume.h"
#include "ResizeHalf.h"
#include "qvapplication.h"
#include "qzimg.h"
#include "fileloader.h"
#include "boundedexecutor.h"
#include "svgloader.h"
#include "startupprofiler.h"

static std::atomic<quint64> nextPrefetchOwnerId{1};

static BoundedExecutor &imagePrefetchExecutor()
{
    static constexpr int InitialPrefetchThreads = 4;
    static constexpr int MaximumPendingPrefetchJobs = 128;
    static BoundedExecutor executor(
        qBound(1, QThread::idealThreadCount(), InitialPrefetchThreads),
        MaximumPendingPrefetchJobs);
    return executor;
}

static ImageContent enrichDetailedMetadata(const QSharedPointer<ImageLoadContext> &context, ImageContent content, const QString &path);

static QFuture<ImageContent> readyImageFuture(ImageContent content)
{
    QPromise<ImageContent> promise;
    promise.start();
    QFuture<ImageContent> future = promise.future();
    promise.addResult(std::move(content));
    promise.finish();
    return future;
}

Volume::Volume(QObject *parent, IFileLoader *loader)
    : QObject(parent),
      m_imageLoadCache(qApp->MaxImagesCache()),
      m_previewLoadCache(qApp->MaxImagesCache()),
      m_loadContext(new ImageLoadContext(loader)),
      m_loader(loader),
      m_pageListLoaded(false),
      m_openedWithSpecifiedImageFile(false),
      m_prefetchOwnerId(nextPrefetchOwnerId.fetch_add(1, std::memory_order_relaxed)),
      m_prefetchGeneration(0),
      m_lastPrefetchAnchor(-1),
      m_lastPrefetchMode(PrefetchMode::Normal)
{
    m_volumePath = m_loader->volumePath();
    connect(&m_watcher, SIGNAL(finished()), this, SLOT(handlePageListLoaded()));
}

Volume::~Volume()
{
    m_imageLoadCache.clear();
    m_previewLoadCache.clear();
    m_loader = nullptr;
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

Volume::ImageLoadFuture Volume::scheduleImageLoad(
    const QString &path,
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
            ImageContent content = futureLoadImageFromFileVolume(context, path, pageSize, decodeTargetSize, loadDetailedMetadata);
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

    return readyImageFuture(
        futureLoadImageFromFileVolume(context, path, pageSize, decodeTargetSize, loadDetailedMetadata));
}

Volume::ImageLoadFuture Volume::scheduleResize(
    ImageContent content, const QSize &pageSize)
{
    auto submission = imagePrefetchExecutor().submit(
        [content, pageSize]() mutable {
            return resizeImageForViewport(std::move(content), pageSize);
        });
    if (submission.accepted) {
        return submission.future;
    }

    return QFuture<ImageContent>();
}

Volume::ImageLoadFuture Volume::scheduleMetadataLoad(
    ImageContent content, const QString &path, quint64 generation)
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
    if (!m_loader) {
        m_pageNames.clear();
        m_pageListLoaded = true;
        return;
    }
    m_pageNames = m_loader->contents();
    m_pageListLoaded = true;
    applyPageSort(qApp->ImageSortBy());
    StartupProfiler::mark("volume.page-list.end");
}

ImageContent Volume::loadImageBeforePageList(QString subfileName)
{
    m_subfileName = subfileName;
    m_initialImage = Volume::futureLoadImageFromFileVolume(m_loadContext, subfileName, QSize());
    loadPageList();
    return m_initialImage;
}

void Volume::handlePageListLoaded()
{
    const int index = m_pageNames.indexOf(m_subfileName);
    if (index >= 0) {
        m_imageLoadCache.insert(index, readyImageFuture(m_initialImage));
        updatePrefetchCache(index, PrefetchMode::Normal, QSize());
    }
    emit pageListLoaded();
}

static bool fileNameDescendingLessThan(const QString &m1, const QString &m2)
{
    QCollator col;
    col.setNumericMode(true);
    return col.compare(m1, m2) < 0;
}

static bool fileNameDescendingGreaterThan(const QString &m1, const QString &m2)
{
    QCollator col;
    col.setNumericMode(true);
    return col.compare(m1, m2) > 0;
}

static bool fileSizeLessThan(const QvImageMetadata &m1, const QvImageMetadata &m2)
{
    QvImageMetadata &mm1 = const_cast<QvImageMetadata &>(m1);
    QvImageMetadata &mm2 = const_cast<QvImageMetadata &>(m2);
    return mm1.getFileSize() < mm2.getFileSize();
}
static bool fileSizeDescendingLessThan(const QvImageMetadata &m1, const QvImageMetadata &m2)
{
    QvImageMetadata &mm1 = const_cast<QvImageMetadata &>(m1);
    QvImageMetadata &mm2 = const_cast<QvImageMetadata &>(m2);
    return mm1.getFileSize() > mm2.getFileSize();
}
static bool modifiedTimeLessThan(const QvImageMetadata &m1, const QvImageMetadata &m2)
{
    QvImageMetadata &mm1 = const_cast<QvImageMetadata &>(m1);
    QvImageMetadata &mm2 = const_cast<QvImageMetadata &>(m2);
    return mm1.getMTime() < mm2.getMTime();
}
static bool modifiedTimeDescendingLessThan(const QvImageMetadata &m1, const QvImageMetadata &m2)
{
    QvImageMetadata &mm1 = const_cast<QvImageMetadata &>(m1);
    QvImageMetadata &mm2 = const_cast<QvImageMetadata &>(m2);
    return mm1.getMTime() > mm2.getMTime();
}

void Volume::sortPages(qvEnums::ImageSortBy sortBy)
{
    applyPageSort(sortBy);
}

void Volume::applyPageSort(qvEnums::ImageSortBy sortBy)
{
    m_imageMetadataList.clear();
    foreach (const QString &fl, m_pageNames) {
        m_imageMetadataList << QvImageMetadata(this, fl);
    }
    switch (sortBy) {
    case qvEnums::SortByFileName:
        std::sort(m_pageNames.begin(), m_pageNames.end(), fileNameDescendingLessThan);
        break;
    case qvEnums::SortByFileNameDescending:
        std::sort(m_pageNames.begin(), m_pageNames.end(), fileNameDescendingGreaterThan);
        break;
    case qvEnums::SortByFileSize:
        std::stable_sort(m_imageMetadataList.begin(), m_imageMetadataList.end(), fileSizeLessThan);
        break;
    case qvEnums::SortByFileSizeDescending:
        std::stable_sort(m_imageMetadataList.begin(), m_imageMetadataList.end(), fileSizeDescendingLessThan);
        break;
    case qvEnums::SortByModifiedTime:
        std::stable_sort(m_imageMetadataList.begin(), m_imageMetadataList.end(), modifiedTimeLessThan);
        break;
    case qvEnums::SortByModifiedTimeDescending:
        std::stable_sort(m_imageMetadataList.begin(), m_imageMetadataList.end(), modifiedTimeDescendingLessThan);
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

QString Volume::pageNameAt(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_pageNames.size()) {
        return "";
    }
    if (!m_shuffledPageNames.isEmpty()) {
        return m_shuffledPageNames[pageIndex];
    }
    if (qApp->ImageSortBy() == qvEnums::SortByFileName || qApp->ImageSortBy() == qvEnums::SortByFileNameDescending) {
        return m_pageNames[pageIndex];
    } else if (pageIndex < m_imageMetadataList.size()) {
        return m_imageMetadataList[pageIndex].filename();
    }
    return "";
}

int Volume::pageIndexForName(const QString &name) const
{
    return m_pageNames.indexOf(QDir::toNativeSeparators(name));
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

    const QSize oversampled(
        qMin(maxTextureSize, qMax(1, viewportSize.width() * 3 / 2)),
        qMin(maxTextureSize, qMax(1, viewportSize.height() * 3 / 2)));
    return oversampled;
}

static bool shouldPrefetchFullResolution(int pageIndex, int anchorPageIndex)
{
    return qAbs(pageIndex - anchorPageIndex) <= 3;
}

void Volume::updatePrefetchCache(
    int anchorPageIndex, PrefetchMode mode, QSize viewportSize)
{
    if (!m_pageListLoaded) {
        loadPageList();
    }
    if (!m_loader || anchorPageIndex < 0 || anchorPageIndex >= m_pageNames.size() || m_loader->contents().isEmpty()) {
        return;
    }

    imagePrefetchExecutor().setMaximumConcurrency(recommendedPrefetchConcurrency(m_loader));

    if (anchorPageIndex != m_lastPrefetchAnchor || mode != m_lastPrefetchMode) {
        m_lastPrefetchAnchor = anchorPageIndex;
        m_lastPrefetchMode = mode;
        ++m_prefetchGeneration;
        imagePrefetchExecutor().cancelPendingOlderThan(m_prefetchOwnerId, m_prefetchGeneration);
    }

    const QList<int> indexes = PrefetchPlanner::indexes(
        mode, anchorPageIndex, m_pageNames.size(), qApp->MaxImagesCache());
    for (int cnt : indexes) {
        const bool fullResolution = shouldPrefetchFullResolution(cnt, anchorPageIndex);
        if (fullResolution) {
            m_previewLoadCache.remove(cnt);
        } else if (m_imageLoadCache.touch(cnt)) {
            continue;
        }
        LruCache<int, ImageLoadFuture> &cache = fullResolution ? m_imageLoadCache : m_previewLoadCache;
        ImageLoadFuture *cachedImageLoad = cache.find(cnt);
        if (cachedImageLoad && cachedImageLoad->isCanceled()) {
            cache.remove(cnt);
            cachedImageLoad = nullptr;
        }
        if (fullResolution && cnt == anchorPageIndex && cachedImageLoad && cachedImageLoad->isFinished()) {
            ImageContent cachedImage = cachedImageLoad->result();
            if (!cachedImage.hasDetailedMetadata && IFileLoader::isExifJpegImageFile(cachedImage.path)) {
                const QString metadataPath = cachedImage.path;
                const ImageLoadFuture metadataLoad = scheduleMetadataLoad(
                    std::move(cachedImage), metadataPath, m_prefetchGeneration);
                if (metadataLoad.isValid()) {
                    cache.insert(cnt, metadataLoad);
                    cachedImageLoad = cache.find(cnt);
                }
            }
        }
        if (fullResolution && qApp->Effect() < qvEnums::UsingFixedShader && cachedImageLoad && cachedImageLoad->isFinished()) {
            ImageContent cachedImage = cachedImageLoad->result();
            if (cachedImage.loadedImageSize.isValid()) {
                const QSize pageSize = viewportSize;
                QSize resized = cachedImage.exifInfo.Orientation == 6 || cachedImage.exifInfo.Orientation == 8 ? QSize(pageSize.height(), pageSize.width()) : pageSize;
                resized.setWidth(cachedImage.loadedImageSize.width() * resized.height() / cachedImage.loadedImageSize.height());

                if (cachedImage.resizedImage.size() != resized && !cachedImage.loadedImage.isNull()) {
                    qDebug() << cachedImage.resizedImage.size() << resized;
                    const ImageLoadFuture future = scheduleResize(
                        cachedImage, pageSize);
                    if (future.isValid()) {
                        cache.insert(cnt, future);
                    }
                }
            }
        }
        if (!cache.touch(cnt)) {
            const QSize pageSize = fullResolution && qApp->Effect() < qvEnums::UsingFixedShader
                                       ? viewportSize
                                       : QSize();
            const QSize decodeTargetSize = fullResolution ? QSize() : previewDecodeSize(viewportSize);
            const ImageLoadFuture future = scheduleImageLoad(
                pageNameAt(cnt),
                pageSize,
                cnt == anchorPageIndex,
                decodeTargetSize,
                cnt == anchorPageIndex || m_loader->isArchive(),
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
    if (!m_pageListLoaded) {
        loadPageList();
    }
    if (!m_loader || anchorPageIndex < 0 || anchorPageIndex >= m_pageNames.size() || m_loader->contents().isEmpty()) {
        return;
    }
    imagePrefetchExecutor().setMaximumConcurrency(recommendedPrefetchConcurrency(m_loader));
    for (int pageIndex : PrefetchPlanner::indexes(
             PrefetchMode::Normal, anchorPageIndex, m_pageNames.size(), 2)) {
        const ImageLoadFuture future = scheduleImageLoad(
            m_pageNames[pageIndex], QSize(), pageIndex == anchorPageIndex, QSize(), pageIndex == anchorPageIndex || m_loader->isArchive(), pageIndex, m_prefetchGeneration);
        if (future.isValid()) {
            m_imageLoadCache.insert(pageIndex, future);
        }
    }
}

ImageContent Volume::loadThumbnailSourceImage()
{
    if (!m_pageListLoaded) {
        loadPageList();
    }
    if (!m_loader || m_pageNames.isEmpty() || m_loader->contents().isEmpty()) {
        return ImageContent();
    }
    return futureLoadImageFromFileVolume(m_loadContext, m_pageNames[0], QSize());
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
    m_watcher.moveToThread(targetThread);
    if (m_loader) {
        m_loader->moveToThread(targetThread);
    }
}

QString Volume::FullPathToVolumePath(QString path)
{
    if (!path.contains("::")) {
        return path;
    }
    return path.left(path.indexOf("::"));
}

QString Volume::FullPathToSubFilePath(QString path)
{
    if (!path.contains("::")) {
        return "";
    }
    return path.mid(path.indexOf("::") + 2);
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
                return littleEndian
                           ? static_cast<quint16>(p[0] | (p[1] << 8))
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
            const quint32 ifdOffset = read32(tiff + 4);
            if (ifdOffset + 2 > static_cast<quint32>(tiffSize)) {
                return 1;
            }
            const unsigned char *ifd = tiff + ifdOffset;
            const quint16 entryCount = read16(ifd);
            for (quint16 i = 0; i < entryCount; ++i) {
                const qsizetype entryOffset = 2 + static_cast<qsizetype>(i) * 12;
                if (ifdOffset + entryOffset + 12 > static_cast<quint32>(tiffSize)) {
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

static QZimg::FilterMode filterModeForShaderEffect(qvEnums::ShaderEffect effect)
{
    switch (effect) {
    case qvEnums::CpuBicubic:
        return QZimg::ResizeBicubic;
    case qvEnums::CpuSpline16:
        return QZimg::ResizeSpline16;
    case qvEnums::CpuSpline36:
        return QZimg::ResizeSpline36;
    case qvEnums::CpuLanczos3:
        return QZimg::ResizeLanczos3;
    case qvEnums::BilinearAndCpuBicubic:
        return QZimg::ResizeBicubic;
    case qvEnums::BilinearAndCpuSpline16:
        return QZimg::ResizeSpline16;
    case qvEnums::BilinearAndCpuSpline36:
        return QZimg::ResizeSpline36;
    case qvEnums::BilinearAndCpuLanczos:
        return QZimg::ResizeLanczos3;
    default:
        return QZimg::ResizeBicubic;
    }
}
static QSize constrainedDecodeSize(const QSize &sourceSize, const QSize &requestedSize, int maxTextureSize)
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

static bool shouldUseDecoderScaling(const QString &format, const QImageReader &reader)
{
    const QString normalized = format.toLower();
    const bool hotRaster = normalized == "jpg" || normalized == "jpeg" || normalized == TURBO_JPEG_FMT || normalized == "webp";
    return hotRaster && reader.supportsOption(QImageIOHandler::ScaledSize);
}

static bool jpegHasIccProfile(const QByteArray &bytes)
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

static bool webpHasFeature(const QByteArray &bytes, unsigned char featureMask)
{
    if (bytes.size() < 21) {
        return false;
    }
    const char *data = bytes.constData();
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WEBP", 4) != 0 || std::memcmp(data + 12, "VP8X", 4) != 0) {
        return false;
    }
    return (static_cast<unsigned char>(data[20]) & featureMask) != 0;
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
    using DecompressHeader3 = int (*)(Handle, const unsigned char *, unsigned long, int *, int *, int *, int *);
    using GetScalingFactors = ScalingFactor *(*)(int *);
    using Decompress2 = int (*)(Handle, const unsigned char *, unsigned long, unsigned char *, int, int, int, int, int);
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
            initDecompress = reinterpret_cast<InitDecompress>(m_library.resolve("tjInitDecompress"));
            decompressHeader3 = reinterpret_cast<DecompressHeader3>(m_library.resolve("tjDecompressHeader3"));
            getScalingFactors = reinterpret_cast<GetScalingFactors>(m_library.resolve("tjGetScalingFactors"));
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

static NativeTurboJpegApi &nativeTurboJpegApi()
{
    static NativeTurboJpegApi *api = new NativeTurboJpegApi;
    return *api;
}

static NativeTurboJpegApi::Handle nativeTurboJpegHandle()
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

static int turboScaledDimension(int dimension, const NativeTurboJpegApi::ScalingFactor &factor)
{
    return (dimension * factor.num + factor.denom - 1) / factor.denom;
}

static bool tryDecodeTurboJpeg(
    const QByteArray &bytes,
    const QSize &decodeTargetSize,
    int maxTextureSize,
    QImage &decoded,
    QSize &sourceSize)
{
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
    Q_UNUSED(bytes);
    Q_UNUSED(decodeTargetSize);
    Q_UNUSED(maxTextureSize);
    Q_UNUSED(decoded);
    Q_UNUSED(sourceSize);
    return false;
#else
    if (bytes.isEmpty() || static_cast<quint64>(bytes.size()) > ULONG_MAX || jpegHasIccProfile(bytes)) {
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
    if (api.decompressHeader3(handle, data, dataSize, &width, &height, &subsampling, &colorSpace) != 0 || width <= 0 || height <= 0) {
        return false;
    }
    // TurboJPEG's direct BGRA conversion does not preserve CMYK/YCCK semantics.
    if (colorSpace == 3 || colorSpace == 4) {
        return false;
    }

    sourceSize = QSize(width, height);
    const QSize desiredSize = constrainedDecodeSize(sourceSize, decodeTargetSize, maxTextureSize);
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
    const int flags = qApp->UseFastDCTForJPEG() ? TurboJpegFlagFastDct : 0;
    if (api.decompress2(
            handle,
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
    using DecodeBgraInto = unsigned char *(*)(const unsigned char *, size_t, unsigned char *, size_t, int);

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
            decodeBgraInto = reinterpret_cast<DecodeBgraInto>(m_library.resolve("WebPDecodeBGRAInto"));
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

static NativeWebPApi &nativeWebPApi()
{
    static NativeWebPApi *api = new NativeWebPApi;
    return *api;
}

static bool tryDecodeWebP(
    const QByteArray &bytes,
    const QSize &decodeTargetSize,
    int maxTextureSize,
    QImage &decoded,
    QSize &sourceSize)
{
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
    Q_UNUSED(bytes);
    Q_UNUSED(decodeTargetSize);
    Q_UNUSED(maxTextureSize);
    Q_UNUSED(decoded);
    Q_UNUSED(sourceSize);
    return false;
#else
    static constexpr unsigned char WebPFeatureAnimation = 0x02;
    static constexpr unsigned char WebPFeatureIcc = 0x20;
    if (bytes.isEmpty() || webpHasFeature(bytes, WebPFeatureAnimation) || webpHasFeature(bytes, WebPFeatureIcc)) {
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
    if (constrainedDecodeSize(sourceSize, decodeTargetSize, maxTextureSize) != sourceSize) {
        // The convenience direct-to-buffer API has no scaling option. Let the
        // Qt/libwebp handler use its scaled decode path for large/preview images.
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    if (image.isNull()) {
        return false;
    }
    const size_t outputSize = static_cast<size_t>(image.bytesPerLine()) * static_cast<size_t>(image.height());
    if (!api.decodeBgraInto(data, dataSize, image.bits(), outputSize, image.bytesPerLine())) {
        return false;
    }

    decoded = std::move(image);
    return true;
#endif
}

static ImageContent loadWithSpecifiedFormat(QString path, QSize pageSize, QSize decodeTargetSize, bool loadDetailedMetadata, QByteArray bytes, QString aformat, uint loopcount)
{
    for (;;) {
        int maxTextureSize = qApp->MaxTextureSize();
        easyexif::EXIFInfo info;

        // I think the excessive normalization of recent years is really ridiculous.
        // Calling what we've traditionally called JPEG something else, like JFIF, is causing confusion for many people.
        // And it hasn't helped solve any of the problems with the JPEG file format.
        // The incompatibility with EXIF remains unresolved.
        if (aformat == "jif" || aformat == "jfif" || aformat == "jfi" || aformat == "jpe") {
            aformat = "jpg";
        }
        if (aformat == "svg") {
            const SvgLoader::RenderResult rendered = SvgLoader::render(
                bytes,
                path,
                QSize(qApp->SvgRasterMaximumWidth(), qApp->SvgRasterMaximumHeight()),
                qApp->SvgLoaderBackend());
            ImageContent ic(rendered.image, path, rendered.sourceSize, info, bytes.length());
            ic.hasDetailedMetadata = true;
            return ic;
        }

        QImage src;
        QSize baseSize;
        const QString normalizedFormat = aformat.toLower();
        bool nativeDecoded = false;
        if (normalizedFormat == "jpg" || normalizedFormat == "jpeg" || normalizedFormat == TURBO_JPEG_FMT) {
            nativeDecoded = tryDecodeTurboJpeg(bytes, decodeTargetSize, maxTextureSize, src, baseSize);
        } else if (normalizedFormat == "webp") {
            nativeDecoded = tryDecodeWebP(bytes, decodeTargetSize, maxTextureSize, src, baseSize);
        }

        ImageContent ic(path, bytes.length());
        if (!nativeDecoded) {
            QBuffer buffer(&bytes);
            QImageReader reader(&buffer, aformat.toUtf8());

            if (!reader.canRead()) {
                aformat = "";
                break;
            }

            if (reader.supportsAnimation()) {
                QvMovie movie = QvMovie(bytes, aformat.toUtf8());
                ic.movie = movie;
                ic.originalSize = ic.loadedImageSize = reader.size();
                ic.hasDetailedMetadata = true;
                return ic;
            }
            if (aformat == "apng") {
                bool lodepng_exist = IFileLoader::supportsImageFormat("lodepng");
                aformat = lodepng_exist ? "lodepng" : "png";
                break;
            }
            baseSize = reader.size();
            QSize loadingSize = baseSize;
            if (reader.format() == TURBO_JPEG_FMT && !qApp->UseFastDCTForJPEG()) {
                reader.setQuality(0);
            }
            if (shouldUseDecoderScaling(aformat, reader)) {
                const QSize targetSize = constrainedDecodeSize(baseSize, decodeTargetSize, maxTextureSize);
                if (targetSize.isValid() && targetSize != baseSize) {
                    loadingSize = targetSize;
                    reader.setScaledSize(loadingSize);
                }
            }

            QImage tmp;
            // QImage processing sometimes fails
            for (int count = 1;; count++) {
                tmp = reader.read();
                if (!tmp.isNull()) {
                    break;
                }
                qDebug() << "[0]" << path << tmp << count;
                if (count >= 100 || aformat.startsWith("tif")) {
                    return ic;
                }
                QThread::currentThread()->usleep(40000);
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
        }

        const bool isExifJpeg = src.width() > 0 && IFileLoader::isExifJpegImageFile(path);
        if (isExifJpeg) {
            if (loadDetailedMetadata) {
                info.parseFrom(reinterpret_cast<const unsigned char *>(bytes.constData()), bytes.length());
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
        if (qApp->DontShrinkForLargeImage() || (src.width() <= maxTextureSize && src.height() <= maxTextureSize)) {
            ic = ImageContent(src, path, baseSize, info, bytes.length());
        } else {
            // resample for too big images
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
                            return ImageContent(path, bytes.length());
                        }
                        QThread::currentThread()->usleep(40000);
                    }
                    src.swap(src2);
                }
                break;
            default:
                if (src.format() != QImage::Format::Format_Grayscale8 && src.format() != QImage::Format::Format_RGB888) {
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
                        return ImageContent(path, bytes.length());
                    } while (1);
                    src.swap(src2);
                }
                break;
            }

            QSize srcSize = src.size();
            QSize halfSize = QSize((srcSize.width()) / 2, (srcSize.height()) / 2);

            QImage half = QImage(halfSize.width(), halfSize.height(), src.format());
            ResizeHalf::FMT fmt = (ResizeHalf::FMT)(src.depth() >> 3);
            ResizeHalf resizer(fmt);
            resizer.resizeHV(half.bits(), src.bits(), src.width(), srcSize.height(), half.bytesPerLine(), src.bytesPerLine());

            ic.loadedImage = half;
            ic.loadedImageSize = half.size();
        }
        if (decodeTargetSize.isValid() && !decodeTargetSize.isEmpty() && !ic.loadedImage.isNull() && (ic.loadedImage.width() > decodeTargetSize.width() || ic.loadedImage.height() > decodeTargetSize.height())) {
            ic.loadedImage = ic.loadedImage.scaled(decodeTargetSize, Qt::KeepAspectRatio, Qt::FastTransformation);
            ic.loadedImageSize = ic.loadedImage.size();
        }
        ic.hasDetailedMetadata = loadDetailedMetadata || !isExifJpeg;

        // CPU resizing before Page Viewing
        if (!pageSize.isEmpty() && !ic.loadedImage.isNull()) {
            QSize newsize = ic.exifInfo.Orientation == 6 || ic.exifInfo.Orientation == 8 ? QSize(pageSize.height(), pageSize.width()) : pageSize;
            ic.appliedResizeMode = qApp->Effect();
            ic.resizedImage = QZimg::scaled(ic.loadedImage, newsize, Qt::KeepAspectRatio, filterModeForShaderEffect(qApp->Effect()));
        }
        return ic;
    }
    if (!loopcount) {
        return ImageContent(path, bytes.length());
    }
    return loadWithSpecifiedFormat(path, pageSize, decodeTargetSize, loadDetailedMetadata, bytes, aformat, loopcount - 1);
}

static ImageContent loadImageFromBytes(
    QString path, QSize pageSize, QSize decodeTargetSize, bool loadDetailedMetadata, const QByteArray &bytes)
{
    if (bytes.isNull() || bytes.isEmpty()) {
        return ImageContent();
    }
    QString aformat;
    if (IFileLoader::isExifJpegImageFile(path)) {
        if (IFileLoader::supportsImageFormat(TURBO_JPEG_FMT)) {
            aformat = TURBO_JPEG_FMT;
        } else {
            aformat = "jpg";
        }
    } else {
        aformat = QFileInfo(path.toLower()).suffix();
    }
    // Extension "png" might be an APNG.
    if (aformat == "png" && IFileLoader::supportsImageFormat("apng")) {
        aformat = "apng";
    }
    return loadWithSpecifiedFormat(path, pageSize, decodeTargetSize, loadDetailedMetadata, bytes, aformat, 5);
}

static ImageContent enrichDetailedMetadata(
    const QSharedPointer<ImageLoadContext> &context, ImageContent content, const QString &path)
{
    if (content.hasDetailedMetadata || !IFileLoader::isExifJpegImageFile(path)) {
        content.hasDetailedMetadata = true;
        return content;
    }

    const QByteArray bytes = context ? context->load(path) : QByteArray();
    if (!bytes.isEmpty()) {
        content.exifInfo.parseFrom(
            reinterpret_cast<const unsigned char *>(bytes.constData()), bytes.length());
    }
    content.hasDetailedMetadata = true;
    return content;
}

static ImageContent futureLoadImageFromFileVolumeImpl(
    const QSharedPointer<ImageLoadContext> &context, QString path, QSize pageSize, QSize decodeTargetSize, bool loadDetailedMetadata)
{
    StartupProfiler::mark("image-worker.extract.begin");
    const QByteArray bytes = context->load(path);
    StartupProfiler::mark("image-worker.extract.end");
    ImageContent content = loadImageFromBytes(path, pageSize, decodeTargetSize, loadDetailedMetadata, bytes);
    StartupProfiler::mark("image-worker.decode-resize.end");
    return content;
}

ImageContent Volume::futureLoadImageFromFileVolume(
    QSharedPointer<ImageLoadContext> context, QString path, QSize pageSize, QSize decodeTargetSize, bool loadDetailedMetadata)
{
    QElapsedTimer et_load;
    et_load.start();
    ImageContent ic = futureLoadImageFromFileVolumeImpl(context, path, pageSize, decodeTargetSize, loadDetailedMetadata);
    qint64 t_load = et_load.elapsed();

    qDebug() << "futureLoadImageFromFileVolume" << path << t_load << "ms, resizedImage=" << !ic.resizedImage.isNull();
    return ic;
}

ImageContent Volume::loadImageFromFile(QString path, QSize pageSize, QSize decodeTargetSize, bool loadDetailedMetadata)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return ImageContent();
    }

    return loadImageFromBytes(path, pageSize, decodeTargetSize, loadDetailedMetadata, file.readAll());
}

ImageContent Volume::resizeImageForViewport(ImageContent content, QSize pageSize)
{
    const QSize targetSize = content.exifInfo.Orientation == 6 || content.exifInfo.Orientation == 8
                                 ? QSize(pageSize.height(), pageSize.width())
                                 : pageSize;
    content.appliedResizeMode = qApp->Effect();
    content.resizedImage = QZimg::scaled(
        content.loadedImage, targetSize, Qt::KeepAspectRatio, filterModeForShaderEffect(qApp->Effect()));
    return content;
}
