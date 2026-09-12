#include <QtGui>
#include <atomic>
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
    static constexpr int MaximumPrefetchThreads = 4;
    static constexpr int MaximumPendingPrefetchJobs = 128;
    static BoundedExecutor executor(
        qBound(1, QThread::idealThreadCount(), MaximumPrefetchThreads),
        MaximumPendingPrefetchJobs);
    return executor;
}

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
    int pageIndex,
    quint64 generation)
{
    const QSharedPointer<ImageLoadContext> context = m_loadContext;
    auto submission = imagePrefetchExecutor().submit(
        [context, path, pageSize, decodeTargetSize] {
            return futureLoadImageFromFileVolume(context, path, pageSize, decodeTargetSize);
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
        futureLoadImageFromFileVolume(context, path, pageSize));
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
}

void Volume::stopSlideShow()
{
    if (!qApp->SlideShowRandomly()) {
        return;
    }
    m_shuffledPageNames.clear();
    m_imageLoadCache.clear();
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

void Volume::updatePrefetchCache(
    int anchorPageIndex, PrefetchMode mode, QSize viewportSize)
{
    if (!m_pageListLoaded) {
        loadPageList();
    }
    if (!m_loader || anchorPageIndex < 0 || anchorPageIndex >= m_pageNames.size() || m_loader->contents().isEmpty()) {
        return;
    }

    if (anchorPageIndex != m_lastPrefetchAnchor || mode != m_lastPrefetchMode) {
        m_lastPrefetchAnchor = anchorPageIndex;
        m_lastPrefetchMode = mode;
        ++m_prefetchGeneration;
        imagePrefetchExecutor().cancelPendingOlderThan(m_prefetchOwnerId, m_prefetchGeneration);
    }

    const QList<int> indexes = PrefetchPlanner::indexes(
        mode, anchorPageIndex, m_pageNames.size(), qApp->MaxImagesCache());
    for (int cnt : indexes) {
        ImageLoadFuture *cachedImageLoad = m_imageLoadCache.find(cnt);
        if (cachedImageLoad && cachedImageLoad->isCanceled()) {
            m_imageLoadCache.remove(cnt);
            cachedImageLoad = nullptr;
        }
        if (qApp->Effect() < qvEnums::UsingFixedShader && cachedImageLoad && cachedImageLoad->isFinished()) {
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
                        m_imageLoadCache.insert(cnt, future);
                    }
                }
            }
        }
        if (!m_imageLoadCache.touch(cnt)) {
            const QSize pageSize = qApp->Effect() < qvEnums::UsingFixedShader
                                       ? viewportSize
                                       : QSize();
            const ImageLoadFuture future = scheduleImageLoad(
                pageNameAt(cnt), pageSize, cnt == anchorPageIndex, QSize(), cnt, m_prefetchGeneration);
            if (future.isValid()) {
                m_imageLoadCache.insert(cnt, future);
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
    for (int pageIndex : PrefetchPlanner::indexes(
             PrefetchMode::Normal, anchorPageIndex, m_pageNames.size(), 2)) {
        const ImageLoadFuture future = scheduleImageLoad(
            m_pageNames[pageIndex], QSize(), pageIndex == anchorPageIndex, QSize(), pageIndex, m_prefetchGeneration);
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

static ImageContent loadWithSpecifiedFormat(QString path, QSize pageSize, QSize decodeTargetSize, QByteArray bytes, QString aformat, uint loopcount)
{
    for (;;) {
        int maxTextureSize = qApp->MaxTextureSize();
        easyexif::EXIFInfo info;
        QBuffer buffer(&bytes);

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
            return ic;
        }

        QImageReader reader(&buffer, aformat.toUtf8());

        if (!reader.canRead()) {
            aformat = "";
            break;
        }

        if (reader.supportsAnimation()) {
            QvMovie movie = QvMovie(bytes, aformat.toUtf8());
            ImageContent ic(path, bytes.length());
            ic.movie = movie;
            ic.originalSize = ic.loadedImageSize = reader.size();
            return ic;
        }
        if (aformat == "apng") {
            bool lodepng_exist = IFileLoader::supportsImageFormat("lodepng");
            aformat = lodepng_exist ? "lodepng" : "png";
            break;
        }
        QSize baseSize = reader.size();
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
        QImage src;
        ImageContent ic(path, bytes.length());
        {
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
            src = QZimg::toPackedImage(tmp);
            if (src.isNull()) {
                return ImageContent(path, bytes.length());
            }
        }

        // parsing JPEG EXIF
        if (src.width() > 0 && IFileLoader::isExifJpegImageFile(path)) {
            info.parseFrom(reinterpret_cast<const unsigned char *>(bytes.constData()), bytes.length());
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
            QSize srcSizeReal = src.size();
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
    return loadWithSpecifiedFormat(path, pageSize, decodeTargetSize, bytes, aformat, loopcount - 1);
}

static ImageContent loadImageFromBytes(
    QString path, QSize pageSize, QSize decodeTargetSize, const QByteArray &bytes)
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
    return loadWithSpecifiedFormat(path, pageSize, decodeTargetSize, bytes, aformat, 5);
}

static ImageContent futureLoadImageFromFileVolumeImpl(
    const QSharedPointer<ImageLoadContext> &context, QString path, QSize pageSize, QSize decodeTargetSize)
{
    StartupProfiler::mark("image-worker.extract.begin");
    const QByteArray bytes = context->load(path);
    StartupProfiler::mark("image-worker.extract.end");
    ImageContent content = loadImageFromBytes(path, pageSize, decodeTargetSize, bytes);
    StartupProfiler::mark("image-worker.decode-resize.end");
    return content;
}

ImageContent Volume::futureLoadImageFromFileVolume(
    QSharedPointer<ImageLoadContext> context, QString path, QSize pageSize, QSize decodeTargetSize)
{
    QElapsedTimer et_load;
    et_load.start();
    ImageContent ic = futureLoadImageFromFileVolumeImpl(context, path, pageSize, decodeTargetSize);
    qint64 t_load = et_load.elapsed();

    qDebug() << "futureLoadImageFromFileVolume" << path << t_load << "ms, resizedImage=" << !ic.resizedImage.isNull();
    return ic;
}

ImageContent Volume::loadImageFromFile(QString path, QSize pageSize, QSize decodeTargetSize)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return ImageContent();
    }

    return loadImageFromBytes(path, pageSize, decodeTargetSize, file.readAll());
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
