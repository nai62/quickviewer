#ifndef VIEWERSESSION_H
#define VIEWERSESSION_H

#include <QtGui>
#include "latestresultdispatcher.h"
#include "lrucache.h"
#include "visiblepages.h"
#include "viewerstate.h"
#include "volume.h"

class Volume;

struct DeferredVolumeLoadCleanup
{
    void operator()(QFuture<VolumeHandle> evictedLoad) const;
};

using VolumeLoadCache = LruCache<QString, QFuture<VolumeHandle>, DeferredVolumeLoadCleanup>;

class PageInfoProvider
{
public:
    virtual ~PageInfoProvider() = default;
    virtual int pageCount() const = 0;
    virtual int currentPageIndex() const = 0;
    virtual VisiblePages visiblePages() const = 0;
    virtual QString volumePath() const = 0;
    virtual QString currentPagePath() const = 0;
};

constexpr QEvent::Type ReloadedEventType = static_cast<QEvent::Type>(QEvent::User + 50);

class ReloadedEvent : public QEvent
{
public:
    ReloadedEvent()
        : QEvent(ReloadedEventType)
    {}
};

class ViewerSession : public QObject, public PageInfoProvider
{
    Q_OBJECT
public:
    ViewerSession(QObject *parent);

    // Volumes
    bool loadVolume(QString path, bool coverOnly = false);
    bool loadVolumeWithFile(QString path, bool allowSecondPage = false);
    bool nextVolume();
    bool prevVolume();
    void reloadVolumeAfterImageRemoval();

    // Pages
    bool advanceSpread();
    bool retreatSpread();
    bool fastForwardPage();
    bool fastBackwardPage();
    bool selectPage(int pageIndex, PrefetchMode prefetchMode = PrefetchMode::Normal);
    bool firstPage();
    bool lastPage();
    bool advanceOnePage();
    bool retreatOnePage();
    bool reloadVisiblePages();
    bool addVisiblePage(ImageContent content, bool append);
    void clearVisiblePages();
    QSize viewportSize() const { return m_viewportSize; }
    void setViewportSize(QSize size);
    bool initialImagePaintPending() const;
    ViewerStateKind stateKind() const { return viewerStateKind(m_state); }
    void deferFolderWorkUntilNextPaint();
    void notifyInitialImagePainted();
    void updateReadProgress();
    void sortActiveVolumePages(qvEnums::ImageSortBy sortBy);
    bool eventFilter(QObject *obj, QEvent *event) override;

    // Get String
    int visiblePageCount() const { return m_visiblePages.size(); }
    /**
     * Returns the zero-based index of the first currently visible page.
     */
    int currentPageIndex() const override { return m_currentPageIndex; }
    VisiblePages visiblePages() const override { return VisiblePages(m_visiblePages); }
    QString currentPagePath() const override
    {
        Volume *volume = activeVolume();
        if (!volume || m_visiblePages.isEmpty()) {
            return "";
        }
        return QDir::toNativeSeparators(volume->pagePathForName(m_visiblePages[0].path));
    }
    QString nextPagePathAfterDeleted() const
    {
        Volume *volume = activeVolume();
        if (!volume || volume->isArchive() || volume->pageCount() <= 1) {
            return "";
        }
        const int index = volume->pageCount() - 1 == m_currentPageIndex ? m_currentPageIndex - 1 : m_currentPageIndex + 1;
        return QDir::toNativeSeparators(volume->pagePathAt(index));
    }
    QString currentPageName() const { return m_visiblePages.isEmpty() ? QString() : m_visiblePages[0].path; }

    /**
     * @brief currentPageNumberText: for the label text on PageBar
     * @return (10-11/2182)
     *      or (10/2182)
     */
    QString currentPageNumberText() const;
    /**
     * @brief currentPageStatusText: for statusbar
     * @return some1.jpg (10-11/2182)[WIDTHxHEIGHT] | some2.jpg [WIDTHxHEIGHT]
     *      or some1.jpg (10/2182)[WIDTHxHEIGHT]
     */
    QString currentPageStatusText() const;
    QString pageSignage(int pageIndex) const;

    QString volumePath() const override
    {
        Volume *volume = activeVolume();
        return volume ? volume->volumePath() : "";
    }
    QString realVolumePath() const
    {
        Volume *volume = activeVolume();
        return volume ? volume->realVolumePath() : "";
    }
    bool isArchive() const
    {
        Volume *volume = activeVolume();
        return volume && volume->isArchive();
    }
    bool isFolder() const
    {
        Volume *volume = activeVolume();
        return volume && !volume->isArchive();
    }

    int pageCount() const override
    {
        Volume *volume = activeVolume();
        return volume ? volume->pageCount() : 0;
    }
    bool shouldShowSecondPage() const;
    void reset()
    {
        ++m_initialDisplayGeneration;
        m_state = EmptyViewerState{};
        m_pendingContainingImagePath.clear();
        m_pendingContainingVolumePath.clear();
        m_pendingContainingPageName.clear();
        m_initialImageLoadDispatcher.invalidate();
        m_volumeLoadDispatcher.invalidate();
        clearVisiblePages();
        m_volumeLoadCache.clear();
    }
signals:
    void visiblePagesChanged(VisiblePages pages);
    void readyForPaint();
    /**
     * @brief pageChanged pages have been changed
     */
    void pageChanged();
    /**
     * @brief volumeChanged the volume has been changed
     */
    void volumeChanged(QString path);
    /**
     * Emitted after the directly opened image has had a chance to paint. Heavy
     * folder-related GUI work can resume after this signal.
     */
    void initialImageDisplayFinished();
public slots:
    void handleVolumePageListLoaded();
    void handleSlideShowStarted();
    void handleSlideShowStopped();

private:
    void startContainingVolumeLoad(const QString &normalizedPath,
                                   const QString &basePath,
                                   const QString &subfileName);
    void finishInitialImageDisplay(quint64 generation);
    VolumeHandle getOrLoadVolume(QString path, bool onlyCover, bool immediate);
    VolumeHandle activeVolumeHandle() const;
    Volume *activeVolume() const;
    void setVolumeReady(VolumeHandle volume);
    void configureVolume(Volume *volume);
    void replaceVisiblePages(QVector<ImageContent> pages);
    static QStringList enumerateVolumes(const QDir &directory);
    /**
     * @brief Index of the first currently visible page
     */
    int m_currentPageIndex;

    bool m_firstVisiblePageIsLandscape;
    bool m_allowSecondVisiblePage;
    QVector<ImageContent> m_visiblePages;
    VolumeLoadCache m_volumeLoadCache;
    QStringList m_volumeNames;

    ViewerState m_state;
    QSize m_viewportSize;

    LatestResultDispatcher<ImageContent> m_initialImageLoadDispatcher;
    LatestResultDispatcher<VolumeHandle> m_volumeLoadDispatcher;
    quint64 m_initialDisplayGeneration;
    QString m_pendingContainingImagePath;
    QString m_pendingContainingVolumePath;
    QString m_pendingContainingPageName;

    //    VolumeLoader m_builderForAssoc;
};

#endif // VIEWERSESSION_H
