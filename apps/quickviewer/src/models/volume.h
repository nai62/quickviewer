#ifndef VOLUME_H
#define VOLUME_H

#include <QtCore>
#include <QtGui>
#include <QtConcurrent>

#include <memory>

#include "boundedexecutor.h"
#include "fileloader.h"
#include "imageloadcontext.h"
#include "lrucache.h"
#include "imagecontent.h"
#include "imageloadmetrics.h"
#include "imagemetadata.h"
#include "prefetchplanner.h"

class VolumeLoader;
/**
 * @brief The Volume class
 *
 * This class represents one volume (folder or archive).
 * Image pre-reading is performed by the selected PrefetchMode algorithm.
 * Images in Volume are listed in advance and can be opened with numbers or subpaths.
 * Loaded images are kept in a least-recently-used cache.
 */
class Volume : public QObject
{
    Q_OBJECT
public:
    using ImageLoadFuture = QFuture<ImageContent>;

    explicit Volume(QObject *parent, std::unique_ptr<IFileLoader> loader);
    ~Volume();
    void loadPageList();
    bool isPageListLoaded() const { return m_pageListLoaded; }
    IFileLoader *fileLoader() { return m_loadContext ? m_loadContext->loader() : nullptr; }
    const IFileLoader *fileLoader() const
    {
        return m_loadContext ? m_loadContext->loader() : nullptr;
    }

    /**
     * The loader's shared state. Page loads still running hold it, and with it
     * the open archive, after the volume itself has gone.
     */
    QWeakPointer<ImageLoadContext> loadContext() const { return m_loadContext; }

    static void shutdownImageLoading() { BoundedExecutor::shutdownAll(); }
    /**
     * True when no page load, resize or metadata read of any volume is running
     * or waiting for a thread. The page-turn benchmark waits for this between
     * turns, the way a reader looks at a page before turning it.
     */
    static bool isImageLoadingIdle();
    static ImageContent futureLoadImageFromFileVolume(QSharedPointer<ImageLoadContext> context,
                                                      QString path,
                                                      QSize pageSize,
                                                      QSize decodeTargetSize = QSize(),
                                                      bool loadDetailedMetadata = true);
    static ImageContent
    decodeImageBytes(const QString &path,
                     const QByteArray &bytes,
                     QSize pageSize = QSize(),
                     QSize decodeTargetSize = QSize(),
                     bool loadDetailedMetadata = true,
                     const ImageDecodePolicy &decodePolicy = ImageDecodePolicy(),
                     ImageDecodeMetrics *metrics = nullptr);
    static ImageContent loadImageFromFile(QString path,
                                          QSize pageSize,
                                          QSize decodeTargetSize = QSize(),
                                          bool loadDetailedMetadata = true);
    static ImageContent resizeImageForViewport(ImageContent content, QSize pageSize);

    bool isArchive() const
    {
        const IFileLoader *loader = fileLoader();
        return loader && loader->isArchive();
    }
    bool hasSubDirectories() const
    {
        const IFileLoader *loader = fileLoader();
        return loader && loader->hasSubDirectories();
    }

    void sortPages(qvEnums::ImageSortBy sortBy);
    void applyPageSort(qvEnums::ImageSortBy sortBy);
    void startSlideShow();
    void stopSlideShow();

    /**
     * Real filesystem path of a page inside a folder volume. Archive entries do
     * not have one, so this returns an empty string for archives.
     */
    QString pagePathForName(const QString &name) const
    {
        const IFileLoader *loader = fileLoader();
        if (!loader || loader->isArchive() || name.isEmpty()) {
            return "";
        }
        return QDir(loader->realVolumePath()).absoluteFilePath(name);
    }
    QString pageNameAt(int pageIndex) const;
    int pageIndexForName(const QString &name) const;
    QString pagePathAt(int pageIndex) const
    {
        const IFileLoader *loader = fileLoader();
        if (!loader || pageIndex < 0 || pageIndex >= m_pageNames.size()) {
            return "";
        }
        // The pages live where the loader found them, which is not the volume
        // path when it unwrapped a single subdirectory.
        return QDir(loader->realVolumePath()).absoluteFilePath(m_pageNames[pageIndex]);
    }
    QString volumePath() const
    {
        const IFileLoader *loader = fileLoader();
        return loader ? loader->volumePath() : QString();
    }
    QString realVolumePath() const
    {
        const IFileLoader *loader = fileLoader();
        return loader ? loader->realVolumePath() : QString();
    }

    /**
     * @brief loadImageByName Reads and returns the image corresponding to the file name specified in the file list without advancing the internal counter
     */
    QByteArray loadByteArrayByName(const QString &name)
    {
        return m_loadContext && !name.isEmpty() ? m_loadContext->load(name) : QByteArray();
    }
    /**
     * @brief Returns the number of pages the volume has
     */
    int pageCount() const { return m_pageNames.size(); }
    void updatePrefetchCache(int anchorPageIndex, PrefetchMode mode, QSize viewportSize);
    void prefetchCoverImages(int anchorPageIndex = 0);
    /**
     * Loads the page the first display shows and waits for it, so a caller can
     * publish a volume whose first paint needs no further extraction or
     * decoding.
     */
    void prefetchInitialDisplayPage();
    ImageContent loadThumbnailSourceImage();
    /**
     * \a content, one of this volume's pages, with its full EXIF read when the
     * load that produced it only read the orientation. Reads the page's bytes
     * on the calling thread.
     */
    ImageContent withDetailedMetadata(ImageContent content) const;

    ImageLoadFuture imageLoadAt(int pageIndex) const;
    bool openedWithSpecifiedImageFile() const { return m_openedWithSpecifiedImageFile; }
    void setOpenedWithSpecifiedImageFile(bool openedWithSpecifiedImageFile)
    {
        m_openedWithSpecifiedImageFile = openedWithSpecifiedImageFile;
    }
    void moveToThread(QThread *targetThread);

private:
    /**
     * Loads the page list on demand and returns the loader when the volume has
     * pages to prefetch. Returns nullptr when there is nothing to prefetch.
     */
    IFileLoader *loaderForPrefetch();
    using SourceLoadFuture = QFuture<QByteArray>;

    /**
     * Loads and decodes the page at \a path. \a prefetchedSource, when not
     * empty, is the page's file already read, which is decoded instead of
     * reading the page again.
     */
    ImageLoadFuture scheduleImageLoad(const QString &path,
                                      const QSize &pageSize,
                                      bool requiredForDisplay,
                                      bool loadDetailedMetadata = true,
                                      int pageIndex = -1,
                                      quint64 generation = 0,
                                      QByteArray prefetchedSource = QByteArray());
    /** Reads the page's file - for an archive, extracts it - without decoding it. */
    SourceLoadFuture scheduleSourceLoad(const QString &path, int pageIndex, quint64 generation);
    /**
     * The page's file the prefetch already read, removed from the cache. Empty
     * when the read has not finished: the decode then reads the page itself.
     */
    QByteArray takePrefetchedSource(int pageIndex);
    ImageLoadFuture
    scheduleResize(ImageContent content, const QSize &pageSize, int pageIndex, quint64 generation);
    ImageLoadFuture
    scheduleMetadataLoad(ImageContent content, const QString &path, quint64 generation);

    /**
     * Page names in display order: every index the viewers and the prefetcher
     * use means the same page, whatever sort produced that order.
     */
    QList<QString> m_pageNames;
    QList<QString> m_shuffledPageNames;
    mutable LruCache<int, ImageLoadFuture> m_imageLoadCache;
    /**
     * Files of the pages beyond the decoded window, read but not decoded:
     * their bytes cost far less memory than their pixels, and decoding them
     * once they come close skips the read or the archive extraction.
     */
    LruCache<int, SourceLoadFuture> m_sourceLoadCache;

    QSharedPointer<ImageLoadContext> m_loadContext;
    bool m_pageListLoaded;
    bool m_openedWithSpecifiedImageFile;
    quint64 m_prefetchOwnerId;
    quint64 m_prefetchGeneration;
    int m_lastPrefetchAnchor;
    PrefetchMode m_lastPrefetchMode;

    friend class VolumeLoader;
};

#endif // VOLUME_H
