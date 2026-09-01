#ifndef VOLUMEMANAGER_H
#define VOLUMEMANAGER_H

#include <QtCore>
#include <QtGui>
#include <QtConcurrent>

#include "fileloader.h"
#include "timeorderdcache.h"
#include "futurecache.h"
#include "imageloadcontext.h"
#include "pagecontent.h"
#include "qvimagemetadata.h"
#include "prefetchplanner.h"

class VolumeManagerBuilder;
/**
 * @brief The VolumeManager class
 *
 * This class manages a Volume (Folder or Archive).
 * Image pre-reading is performed by the prefetch algorithm specified by CacheMode.
 * Images in Volume are listed in advance and can be opened with numbers or subpaths.
 * The loaded image is cached by the FIFO method(TimeOrderdCache).
 */
class VolumeManager : public QObject
{
    Q_OBJECT
//    Q_DISABLE_COPY(IFileVolume)
public:
    using CacheMode = PrefetchMode;
    static constexpr CacheMode Normal = CacheMode::Normal;
    static constexpr CacheMode NormalForward = CacheMode::NormalForward;
    static constexpr CacheMode NormalBackward = CacheMode::NormalBackward;
    static constexpr CacheMode FastForward = CacheMode::FastForward;
    static constexpr CacheMode FastBackward = CacheMode::FastBackward;
    static constexpr CacheMode CoverOnly = CacheMode::CoverOnly;
    static constexpr CacheMode CreateThumbnail = CacheMode::CreateThumbnail;

    typedef QFuture<ImageContent> future_image;

    explicit VolumeManager(QObject *parent, IFileLoader *loader);
    ~VolumeManager();
    void enumerate();
    bool enumerated() { return m_enumerated; }
    ImageContent getImageBeforeEnmumerate(QString subfilename);
    IFileLoader *FileLoader() { return m_loader; }

    static ImageContent futureLoadImageFromFileVolume(
        QSharedPointer<ImageLoadContext> context, QString path, QSize pageSize);
    static ImageContent loadImageFromFile(QString path, QSize pageSize);
    static ImageContent futureReizeImage(ImageContent ic, QSize pageSize);
    static QString FullPathToVolumePath(QString path);
    static QString FullPathToSubFilePath(QString path);

    bool isArchive() const { return m_loader && m_loader->isArchive(); }
    bool hasSubDirectories() const { return m_loader && m_loader->hasSubDirectories(); }

    void sort(qvEnums::ImageSortBy sortBy);
    void sortForReady(qvEnums::ImageSortBy sortBy);
    void startSlideShow();
    void stopSlideShow();

    QString currentPath()
    {
        if (!m_loader || m_cnt < 0 || m_cnt >= m_filelist.size()) {
            return "";
        }
        if (m_loader->isArchive()) {
            return QString("%1::%2")
                .arg(QDir::fromNativeSeparators(m_loader->volumePath()))
                .arg(m_filelist[m_cnt]);
        } else {
            return QDir::fromNativeSeparators(QDir(m_loader->volumePath()).absoluteFilePath(m_filelist[m_cnt]));
        }
    }
    QString currentPathWithSeparator()
    {
        if (!m_loader || m_cnt < 0 || m_cnt >= m_filelist.size()) {
            return "";
        }
        return QString("%1::%2")
            .arg(QDir::fromNativeSeparators(m_loader->volumePath()))
            .arg(m_filelist[m_cnt]);
    }

    QString getPathByFileName(QString name)
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
    QString getIndexedFileName(int idx);
    QString getPathByIndex(int idx)
    {
        if (idx < 0 || idx >= m_filelist.size()) {
            return "";
        }
        return QDir(m_loader->volumePath()).absoluteFilePath(m_filelist[idx]);
    }
    void setCacheMode(CacheMode cachemode) { m_cacheMode = cachemode; }
    CacheMode cacheMode() const { return m_cacheMode; }

    const ImageContent currentImage()
    {
        if (m_cacheMode == CreateThumbnail) {
            return m_currentCacheSync;
        }
        return m_currentCache.isValid() ? m_currentCache.result() : ImageContent();
    }
    QString volumePath() { return m_loader ? m_loader->volumePath() : QString(); }
    QString realVolumePath() { return m_loader ? m_loader->realVolumePath() : QString(); }

    bool nextPage();
    bool prevPage();
    bool findPageByIndex(int idx);

    /**
     * @brief Move to the file corresponding to the idx value specified in the file list(Max is size()-1)
     */
    bool findImageByIndex(int idx);

    /**
     * @brief Move to the file corresponding to the file name specified in the current file list
     */
    bool findImageByName(QString name);

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
    int size() { return m_filelist.size(); }
    /**
     * @brief on_ready Called when the application is ready. First, or the image to be displayed next and its file path are emitted
     */
    void on_ready();
    int pageCount() { return m_cnt; }

//    QPixmap getIndexedImage(int idx);
//    QString getIndexedImageName(int idx) { return m_filelist[idx]; }
//    QString currentImageName() const { return m_filelist[m_cnt]; }
    const ImageContent getIndexedImageContent(int idx);
    bool openedWithSpecifiedImageFile() { return m_openedWithSpecifiedImageFile; }
    void setOpenedWithSpecifiedImageFile(bool openedWithSpecifiedImageFile) { m_openedWithSpecifiedImageFile = openedWithSpecifiedImageFile; }
    void setViewportSize(QSize size) { m_viewportSize = size; }
    void moveToThread(QThread *targetThread);

signals:
    void enumerationFinished();

public slots:
    void on_enmumerated();

private:
    future_image scheduleImageLoad(const QString &path, const QSize &pageSize, bool requiredForDisplay);
    future_image scheduleResize(ImageContent content, const QSize &pageSize);

    /**
     * @brief m_cnt File counter in the volume
     */
    int m_cnt;
    QList<QString> m_filelist;
    QList<QString> m_randomfilelist;
    QList<QvImageMetadata> m_imageMetadataList;
    future_image m_currentCache;
    ImageContent m_currentCacheSync;

    FutureCache<int, ImageContent> m_imageCache;
//    QMap<int, future_image> m_imageCache;
//    QList<int> m_pageCache;

    QSharedPointer<ImageLoadContext> m_loadContext;
    IFileLoader *m_loader;
    CacheMode m_cacheMode;
    QSize m_viewportSize;
    bool m_enumerated;
    bool m_openedWithSpecifiedImageFile;
    QString m_volumePath;

    // fast image loading
    QString m_subfilename;
    QFutureWatcher<void> m_watcher;

    friend class VolumeManagerBuilder;
};

#endif // VOLUMEMANAGER_H
