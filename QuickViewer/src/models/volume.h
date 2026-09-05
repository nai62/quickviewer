#ifndef VOLUME_H
#define VOLUME_H

#include <QtCore>
#include <QtGui>
#include <QtConcurrent>

#include "fileloader.h"
#include "imageloadcontext.h"
#include "lrucache.h"
#include "pagecontent.h"
#include "qvimagemetadata.h"
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

    explicit Volume(QObject *parent, IFileLoader *loader);
    ~Volume();
    void loadPageList();
    bool isPageListLoaded() { return m_pageListLoaded; }
    ImageContent loadImageBeforePageList(QString subfileName);
    IFileLoader *fileLoader() { return m_loader; }

    static ImageContent futureLoadImageFromFileVolume(
        QSharedPointer<ImageLoadContext> context, QString path, QSize pageSize);
    static ImageContent loadImageFromFile(QString path, QSize pageSize);
    static ImageContent resizeImageForViewport(ImageContent content, QSize pageSize);
    static QString FullPathToVolumePath(QString path);
    static QString FullPathToSubFilePath(QString path);

    bool isArchive() const { return m_loader && m_loader->isArchive(); }
    bool hasSubDirectories() const { return m_loader && m_loader->hasSubDirectories(); }

    void sortPages(qvEnums::ImageSortBy sortBy);
    void applyPageSort(qvEnums::ImageSortBy sortBy);
    void startSlideShow();
    void stopSlideShow();

    QString pagePathForName(QString name)
    {
        if (!m_loader || name.isEmpty()) {
            return "";
        }
        if (m_loader->isArchive()) {
            return QString("%1::%2")
                .arg(QDir::fromNativeSeparators(m_loader->volumePath()))
                .arg(name);
        } else {
            return QDir(m_loader->realVolumePath()).absoluteFilePath(name);
        }
    }
    QString pageNameAt(int pageIndex);
    int pageIndexForName(const QString &name) const;
    QString pagePathAt(int pageIndex)
    {
        if (pageIndex < 0 || pageIndex >= m_pageNames.size()) {
            return "";
        }
        return QDir(m_loader->volumePath()).absoluteFilePath(m_pageNames[pageIndex]);
    }
    QString pagePathWithSeparatorAt(int pageIndex)
    {
        if (!m_loader || pageIndex < 0 || pageIndex >= m_pageNames.size()) {
            return "";
        }
        return QString("%1::%2")
            .arg(QDir::fromNativeSeparators(m_loader->volumePath()))
            .arg(m_pageNames[pageIndex]);
    }
    QString volumePath() { return m_loader ? m_loader->volumePath() : QString(); }
    QString realVolumePath() { return m_loader ? m_loader->realVolumePath() : QString(); }

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
    int pageCount() { return m_pageNames.size(); }
    void updatePrefetchCache(int anchorPageIndex, PrefetchMode mode, QSize viewportSize);
    void prefetchCoverImages(int anchorPageIndex = 0);
    ImageContent loadThumbnailSourceImage();

    ImageLoadFuture imageLoadAt(int pageIndex) const;
    bool openedWithSpecifiedImageFile() { return m_openedWithSpecifiedImageFile; }
    void setOpenedWithSpecifiedImageFile(bool openedWithSpecifiedImageFile) { m_openedWithSpecifiedImageFile = openedWithSpecifiedImageFile; }
    void moveToThread(QThread *targetThread);

signals:
    void pageListLoaded();

public slots:
    void handlePageListLoaded();

private:
    ImageLoadFuture scheduleImageLoad(const QString &path, const QSize &pageSize, bool requiredForDisplay);
    ImageLoadFuture scheduleResize(ImageContent content, const QSize &pageSize);

    QList<QString> m_pageNames;
    QList<QString> m_shuffledPageNames;
    QList<QvImageMetadata> m_imageMetadataList;
    ImageContent m_initialImage;
    LruCache<int, ImageLoadFuture> m_imageLoadCache;

    QSharedPointer<ImageLoadContext> m_loadContext;
    IFileLoader *m_loader;
    bool m_pageListLoaded;
    bool m_openedWithSpecifiedImageFile;
    QString m_volumePath;

    // fast image loading
    QString m_subfileName;
    QFutureWatcher<void> m_watcher;

    friend class VolumeLoader;
};

#endif // VOLUME_H
