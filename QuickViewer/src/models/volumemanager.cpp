#include <QtGui>
#include <random>
#include <QSvgRenderer>

#include "volumemanager.h"
#include "ResizeHalf.h"
#include "qvapplication.h"
#include "qzimg.h"
#include "fileloader.h"
#include "boundedexecutor.h"
#include "svgnative/SVGDocument.h"
#include "svgnative/ports/qt/QSVGRenderer.h"

using namespace SVGNative;

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

VolumeManager::VolumeManager(QObject *parent, IFileLoader *loader)
    : QObject(parent),
      m_cnt(0),
      m_imageCache(qApp->MaxImagesCache()),
      m_loadContext(new ImageLoadContext(loader)),
      m_loader(loader),
      m_cacheMode(CacheMode::Normal),
      m_viewportSize(),
      m_enumerated(false),
      m_openedWithSpecifiedImageFile(false)
{
    m_volumePath = m_loader->volumePath();
    connect(&m_watcher, SIGNAL(finished()), this, SLOT(handleEnumerationFinished()));
}

VolumeManager::~VolumeManager()
{
    m_imageCache.clear();
    m_loader = nullptr;
}

VolumeManager::future_image VolumeManager::scheduleImageLoad(
    const QString &path, const QSize &pageSize, bool requiredForDisplay)
{
    const QSharedPointer<ImageLoadContext> context = m_loadContext;
    auto submission = imagePrefetchExecutor().submit(
        [context, path, pageSize] {
            return futureLoadImageFromFileVolume(context, path, pageSize);
        });
    if (submission.accepted) {
        return submission.future;
    }

    if (!requiredForDisplay) {
        return QFuture<ImageContent>();
    }

    return readyImageFuture(
        futureLoadImageFromFileVolume(context, path, pageSize));
}

VolumeManager::future_image VolumeManager::scheduleResize(
    ImageContent content, const QSize &pageSize)
{
    auto submission = imagePrefetchExecutor().submit(
        [content, pageSize]() mutable {
            return futureReizeImage(std::move(content), pageSize);
        });
    if (submission.accepted) {
        return submission.future;
    }

    return QFuture<ImageContent>();
}

void VolumeManager::enumerate()
{
    if (!m_loader) {
        m_filelist.clear();
        m_enumerated = true;
        return;
    }
    m_filelist = m_loader->contents();
    m_enumerated = true;
    sortForReady(qApp->ImageSortBy());
}

ImageContent VolumeManager::getImageBeforeEnmumerate(QString subfilename)
{
    m_subfilename = subfilename;
    m_currentCacheSync = VolumeManager::futureLoadImageFromFileVolume(m_loadContext, subfilename, QSize());
    enumerate();
    return m_currentCacheSync;
}

void VolumeManager::handleEnumerationFinished()
{
    // foreach(const QString& fl, m_filelist) {
    //     m_imageMetadataList << QvImageMetadata(this, fl);
    // }
    const int index = m_filelist.indexOf(m_subfilename);
    if (index >= 0) {
        m_imageCache.insert(index, readyImageFuture(m_currentCacheSync));
        findImageByIndex(index);
    }
    setCacheMode(VolumeManager::Normal);
    handleReady();
    emit enumerationFinished();
}

// #ifdef Q_OS_WIN
// #include <Shlwapi.h>

// static bool fileNameDescendingLessThan(const QvImageMetadata& m1, const QvImageMetadata& m2)
// {
//     std::wstring ss1(m1.filename().toStdWString());
//     std::wstring ss2(m2.filename().toStdWString());
//     return ::StrCmpLogicalW(ss1.c_str(), ss2.c_str()) > 0;
// }
// #else

// static bool fileNameDescendingLessThan(const QvImageMetadata& m1, const QvImageMetadata& m2)
// {
//     return m1.filename() > m2.filename();
// }

// #endif

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

void VolumeManager::sort(qvEnums::ImageSortBy sortBy)
{
    sortForReady(sortBy);
    handleReady();
}

void VolumeManager::sortForReady(qvEnums::ImageSortBy sortBy)
{
    m_imageMetadataList.clear();
    foreach (const QString &fl, m_filelist) {
        m_imageMetadataList << QvImageMetadata(this, fl);
    }
    switch (sortBy) {
    case qvEnums::SortByFileName:
        std::sort(m_filelist.begin(), m_filelist.end(), fileNameDescendingLessThan);
        break;
    case qvEnums::SortByFileNameDescending:
        std::sort(m_filelist.begin(), m_filelist.end(), fileNameDescendingGreaterThan);
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
    m_cnt = 0;
    m_imageCache.clear();
}

void VolumeManager::startSlideShow()
{
    if (!qApp->SlideShowRandomly()) {
        return;
    }
    m_randomfilelist = m_filelist;
    m_randomfilelist.detach();
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_randomfilelist.begin(), m_randomfilelist.end(), g);
    m_cnt = 0;
    m_imageCache.clear();
    handleReady();
}

void VolumeManager::stopSlideShow()
{
    if (!qApp->SlideShowRandomly()) {
        return;
    }
    m_randomfilelist.clear();
    m_cnt = 0;
    m_imageCache.clear();
    handleReady();
}

QString VolumeManager::getIndexedFileName(int idx)
{
    if (idx < 0 || idx >= m_filelist.size()) {
        return "";
    }
    if (!m_randomfilelist.isEmpty()) {
        return m_randomfilelist[idx];
    }
    if (qApp->ImageSortBy() == qvEnums::SortByFileName || qApp->ImageSortBy() == qvEnums::SortByFileNameDescending) {
        return m_filelist[idx];
    } else if (idx < m_imageMetadataList.size()) {
        return m_imageMetadataList[idx].filename();
    }
    return "";
}

void VolumeManager::handleReady()
{
    if (!m_enumerated) {
        enumerate();
    }
    if (!m_loader || m_cnt < 0 || m_cnt >= m_filelist.size() || m_loader->contents().isEmpty()) {
        return;
    }

    //    qDebug() << "handleReady: m_cnt" << m_cnt;
    switch (m_cacheMode) {
    case CacheMode::CreateThumbnail:
        m_currentCacheSync = futureLoadImageFromFileVolume(m_loadContext, m_filelist[0], QSize());
        return;
    case CacheMode::CoverOnly:
        for (int cnt : PrefetchPlanner::indexes(
                 CacheMode::Normal, m_cnt, m_filelist.size(), 2)) {
            const future_image future = scheduleImageLoad(
                m_filelist[cnt], QSize(), cnt == m_cnt);
            if (future.isValid()) {
                m_imageCache.insert(cnt, future);
            }
        }
        return;
    default:
        break;
    }
    const QList<int> indexes = PrefetchPlanner::indexes(
        m_cacheMode, m_cnt, m_filelist.size(), qApp->MaxImagesCache());
    for (int cnt : indexes) {
        if (qApp->Effect() < qvEnums::UsingFixedShader && m_imageCache.contains(cnt) && m_imageCache.object(cnt).isFinished()) {
            ImageContent ic = m_imageCache.object(cnt).result();
            if (ic.ImportSize.isValid()) {
                const QSize pageSize = m_viewportSize;
                QSize resized = ic.Info.Orientation == 6 || ic.Info.Orientation == 8 ? QSize(pageSize.height(), pageSize.width()) : pageSize;
                resized.setWidth(ic.ImportSize.width() * resized.height() / ic.ImportSize.height());

                if (ic.ResizedImage.size() != resized && !ic.Image.isNull()) {
                    qDebug() << ic.ResizedImage.size() << resized;
                    const future_image future = scheduleResize(
                        ic, pageSize);
                    if (future.isValid()) {
                        m_imageCache.insert(cnt, future);
                    }
                }
            }
        }
        if (m_imageCache.checkShouldBeInserted(cnt)) {
            //            qDebug() << "handleReady()" << m_filelist[cnt];
            const QSize pageSize = qApp->Effect() < qvEnums::UsingFixedShader
                                       ? m_viewportSize
                                       : QSize();
            const future_image future = scheduleImageLoad(
                getIndexedFileName(cnt), pageSize, cnt == m_cnt);
            if (future.isValid()) {
                m_imageCache.insertNoChecked(cnt, future);
            } else {
                m_imageCache.remove(cnt);
            }
        }
    }
    m_currentCache = m_imageCache.object(m_cnt);
}

const ImageContent VolumeManager::getIndexedImageContent(int idx)
{
    if (idx < 0 || idx >= m_filelist.size() || !m_imageCache.contains(idx)) {
        return ImageContent();
    }
    //    future_image cache = m_imageCache[idx];
    future_image &cache = m_imageCache.object(idx);
    //    if(!cache.isFinished())
    //        cache.waitForFinished();
    return cache.isValid() ? cache.result() : ImageContent();
}

void VolumeManager::moveToThread(QThread *targetThread)
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

bool VolumeManager::nextPage()
{
    //    qDebug() << "nextPage: " << m_cnt << m_filelist.size() <<  "prevCache.size()" << m_prevCache.size() << "nextCache.size()" << m_nextCache.size();
    if (!m_loader || m_cnt < 0 || m_cnt >= m_filelist.size() - 1 || m_loader->contents().isEmpty()) {
        return false;
    }
    m_cnt++;
    handleReady();
    return true;
}

bool VolumeManager::prevPage()
{
    //    qDebug() << "prevPage: " << m_cnt << m_filelist.size() <<  "prevCache.size()" << m_prevCache.size() << "nextCache.size()" << m_nextCache.size();
    if (!m_loader || m_cnt <= 0 || m_cnt >= m_filelist.size() || m_loader->contents().isEmpty()) {
        return false;
    }
    m_cnt--;
    handleReady();
    return true;
}

bool VolumeManager::findPageByIndex(int idx)
{
    if (idx < 0 || idx >= m_filelist.size()) {
        return false;
    }
    if (m_cnt == idx) {
        return true;
    }
    m_cnt = idx;
    //    bool result = findImageByIndex(idx);
    handleReady();
    return true;
}

bool VolumeManager::findImageByIndex(int idx)
{
    if (idx < 0 || idx >= m_filelist.size()) {
        return false;
    }
    m_cnt = idx;
    handleReady();
    return true;
}

bool VolumeManager::findImageByName(QString name)
{
    int idx = m_filelist.indexOf(QDir::toNativeSeparators(name));
    return findImageByIndex(idx);
}

QString VolumeManager::FullPathToVolumePath(QString path)
{
    if (!path.contains("::")) {
        return path;
    }
    return path.left(path.indexOf("::"));
}

QString VolumeManager::FullPathToSubFilePath(QString path)
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

static QZimg::FilterMode ShaderEffect2FilterMode(qvEnums::ShaderEffect effect)
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
static ImageContent loadWithSpecifiedFormat(QString path, QSize pageSize, QByteArray bytes, QString aformat, uint loopcount)
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
        QImageReader reader(&buffer, aformat.toUtf8());

        //        QElapsedTimer et_canRead; et_canRead.start();
        if (!reader.canRead()) {
            aformat = "";
            break;
        }

        if (aformat == "svg") {
            QSize baseSize = reader.size();
            QSize maxSize(3840, 2160);
            QSize size = baseSize.scaled(maxSize, Qt::KeepAspectRatio);
            QImage image;
            if (qApp->HowToLoadSVG() == "imageformat") {
                reader.setScaledSize(size);
                image = reader.read();
            } else if (qApp->HowToLoadSVG() == "qsvg") {
                // SVG is drawn using QGraphicsSvgItem so QImage is not needed, but I want the resolution of the graphics.
                QSvgRenderer *renderer = new QSvgRenderer(bytes);
                image = QImage(size, QImage::Format_ARGB32);
                {
                    QPainter painter(&image);
                    renderer->render(&painter);
                }
            } else {
                // offline rendering into QImage
                auto renderer = std::shared_ptr<QSVGRenderer>(new QSVGRenderer);
                auto svgDocument = SVGDocument::CreateSVGDocument(bytes.constData(), renderer);

                // render twice size
                image = QImage(size, QImage::Format_ARGB32);
                {
                    QPainter painter(&image);
                    painter.setWindow(0, 0, baseSize.width(), baseSize.height());
                    renderer->SetPainter(&painter);
                    svgDocument->Render();
                }
            }

            ImageContent ic(image, path, size, info, bytes.length());
            return ic;
        }

        //        qint64 t_canRead = et_canRead.elapsed();
        //        // Emptying the format of QImageReader will get the format of the internal QImageIoHandler
        //        reader.setFormat("");

        //        QElapsedTimer et_supportsAnimation; et_supportsAnimation.start();
        if (reader.supportsAnimation()) {
            QvMovie movie = QvMovie(bytes, aformat.toUtf8());
            ImageContent ic(path, bytes.length());
            ic.Movie = movie;
            ic.BaseSize = ic.ImportSize = reader.size();
            return ic;
        }
        //        qint64 t_supportsAnimation = et_supportsAnimation.elapsed();
        //        qDebug() << path << t_canRead << t_supportsAnimation;

        if (aformat == "apng") {
            bool lodepng_exist = IFileLoader::isImageFile("lodepng");
            aformat = lodepng_exist ? "lodepng" : "png";
            break;
        }
        // turbjpeg can turbo rescaling when loading
        QSize baseSize = reader.size();
        QSize loadingSize = baseSize;
        // qrawspeed plugin can also load rescaled raw images(using built in thumbnail),
        // but usualy thumbnails are too small, so we don't use
        if (reader.format() == TURBO_JPEG_FMT) {
            if (!qApp->UseFastDCTForJPEG()) {
                reader.setQuality(0);
            }
            while (loadingSize.width() > maxTextureSize || loadingSize.height() > maxTextureSize) {
                loadingSize = QSize((loadingSize.width() + 1) >> 1, (loadingSize.height() + 1) >> 1);
            }
            reader.setScaledSize(loadingSize);
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
                //                if(count >= 100) return ImageContent(path);
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

        //    ImageContent ic(QPixmap::fromImage(src), path, baseSize, info);
        ic.BaseSize = baseSize;
        ic.Info = info;
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
                if ((src.width() | 0x3) > 0) {
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
                if ((src.width() | 0xF) > 0) {
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

            //qDebug() << path << "[3]width:" << srcSize;
            QImage half = QImage(halfSize.width(), halfSize.height(), src.format());
            //qDebug() << path << "[2]Dest:" <<  half;

            //        qDebug() << path << src;
            ResizeHalf::FMT fmt = (ResizeHalf::FMT)(src.depth() >> 3);
            ResizeHalf resizer(fmt);
            resizer.resizeHV(half.bits(), src.bits(), src.width(), srcSize.height(), half.bytesPerLine(), src.bytesPerLine());

            //        ImageContent ic(QPixmap::fromImage(half), path, srcSizeReal, info);
            ic.Image = half;
            ic.ImportSize = half.size();
        }
        // CPU resizing before Page Viewing
        if (!pageSize.isEmpty() && !ic.Image.isNull()) {
            QSize newsize = ic.Info.Orientation == 6 || ic.Info.Orientation == 8 ? QSize(pageSize.height(), pageSize.width()) : pageSize;
            ic.ResizeMode = qApp->Effect();
            ic.ResizedImage = QZimg::scaled(ic.Image, newsize, Qt::KeepAspectRatio, ShaderEffect2FilterMode(qApp->Effect()));
        }
        return ic;
    }
    if (!loopcount) {
        return ImageContent(path, bytes.length());
    }
    return loadWithSpecifiedFormat(path, pageSize, bytes, aformat, loopcount - 1);
}

static ImageContent loadImageFromBytes(
    QString path, QSize pageSize, const QByteArray &bytes)
{
    if (bytes.isNull() || bytes.isEmpty()) {
        return ImageContent();
    }
    QString aformat = QFileInfo(path.toLower()).suffix();
    if (IFileLoader::isExifJpegImageFile(path)) {
        aformat = "jpg";
        if (IFileLoader::isImageFile("turbojpeg")) {
            aformat = TURBO_JPEG_FMT;
        }
    }
    // Extension "png" might be an APNG.
    if (aformat == "png" && IFileLoader::isImageFile("apng")) {
        aformat = "apng";
    }
    //    if(aformat == "png") {
    //        bool lodepng_exist = IFileLoader::isImageFile("lodepng");
    //        aformat = lodepng_exist ? "lodepng" : "png";
    //    }
    return loadWithSpecifiedFormat(path, pageSize, bytes, aformat, 5);
}

static ImageContent futureLoadImageFromFileVolumeImpl(
    const QSharedPointer<ImageLoadContext> &context, QString path, QSize pageSize)
{
    //    qDebug() << "futureLoadImageFromFileVolume" << path << QThread::currentThread();

    return loadImageFromBytes(path, pageSize, context->load(path));
}

ImageContent VolumeManager::futureLoadImageFromFileVolume(
    QSharedPointer<ImageLoadContext> context, QString path, QSize pageSize)
{
    QElapsedTimer et_load;
    et_load.start();
    ImageContent ic = futureLoadImageFromFileVolumeImpl(context, path, pageSize);
    qint64 t_load = et_load.elapsed();

    qDebug() << "futureLoadImageFromFileVolume" << path << t_load << "ms, ResizedImage=" << !ic.ResizedImage.isNull();
    return ic;
}

ImageContent VolumeManager::loadImageFromFile(QString path, QSize pageSize)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return ImageContent();
    }

    return loadImageFromBytes(path, pageSize, file.readAll());
}

ImageContent VolumeManager::futureReizeImage(ImageContent ic, QSize pageSize)
{
    //    qDebug() << "futureReizeImage:" << ic.Path;
    QSize newsize = ic.Info.Orientation == 6 || ic.Info.Orientation == 8 ? QSize(pageSize.height(), pageSize.width()) : pageSize;
    ic.ResizeMode = qApp->Effect();
    ic.ResizedImage = QZimg::scaled(ic.Image, newsize, Qt::KeepAspectRatio, ShaderEffect2FilterMode(qApp->Effect()));
    return ic;
}
