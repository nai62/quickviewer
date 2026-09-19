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
    ImageContent loadImageBeforePageList(QString subfileName);
    IFileLoader *fileLoader() { return m_loadContext ? m_loadContext->loader() : nullptr; }
    const IFileLoader *fileLoader() const
    {
        return m_loadContext ? m_loadContext->loader() : nullptr;
    }

    static void shutdownImageLoading() { BoundedExecutor::shutdownAll(); }
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
        return QDir(loader->volumePath()).absoluteFilePath(m_pageNames[pageIndex]);
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
    ImageContent loadThumbnailSourceImage();

    ImageLoadFuture imageLoadAt(int pageIndex) const;
    bool openedWithSpecifiedImageFile() const { return m_openedWithSpecifiedImageFile; }
    void setOpenedWithSpecifiedImageFile(bool openedWithSpecifiedImageFile)
    {
        m_openedWithSpecifiedImageFile = openedWithSpecifiedImageFile;
    }
    void moveToThread(QThread *targetThread);

signals:
    void pageListLoaded();

public slots:
    void handlePageListLoaded();

private:
    /**
     * Loads the page list on demand and returns the loader when the volume has
     * pages to prefetch. Returns nullptr when there is nothing to prefetch.
     */
    IFileLoader *loaderForPrefetch();
    ImageLoadFuture scheduleImageLoad(const QString &path,
                                      const QSize &pageSize,
                                      bool requiredForDisplay,
                                      const QSize &decodeTargetSize = QSize(),
                                      bool loadDetailedMetadata = true,
                                      int pageIndex = -1,
                                      quint64 generation = 0);
    ImageLoadFuture scheduleResize(ImageContent content, const QSize &pageSize);
    ImageLoadFuture
    scheduleMetadataLoad(ImageContent content, const QString &path, quint64 generation);

    QList<QString> m_pageNames;
    QList<QString> m_shuffledPageNames;
    QList<ImageMetadata> m_imageMetadataList;
    /**
     * Sort the page list was built with. The metadata list only exists for the
     * metadata sorts, so pageNameAt() has to use this instead of the current
     * application setting.
     */
    qvEnums::ImageSortBy m_sortBy = qvEnums::ImageSortBy::SortByFileName;
    ImageContent m_initialImage;
    mutable LruCache<int, ImageLoadFuture> m_imageLoadCache;
    LruCache<int, ImageLoadFuture> m_previewLoadCache;

    QSharedPointer<ImageLoadContext> m_loadContext;
    bool m_pageListLoaded;
    bool m_openedWithSpecifiedImageFile;
    QString m_volumePath;
    quint64 m_prefetchOwnerId;
    quint64 m_prefetchGeneration;
    int m_lastPrefetchAnchor;
    PrefetchMode m_lastPrefetchMode;

    // fast image loading
    QString m_subfileName;
    QFutureWatcher<void> m_watcher;

    friend class VolumeLoader;
};

#endif // VOLUME_H
