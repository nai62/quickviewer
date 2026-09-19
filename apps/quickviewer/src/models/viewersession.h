#ifndef VIEWERSESSION_H
#define VIEWERSESSION_H

#include <QtGui>
#include <memory>
#include "latestresultdispatcher.h"
#include "pagenavigator.h"
#include "visiblepagecomposer.h"
#include "visiblepages.h"
#include "viewerstate.h"
#include "viewerloadstatus.h"
#include "volume.h"
#include "volumecache.h"
#include "volumelocation.h"

class Volume;

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

class ViewerSession : public QObject, public PageInfoProvider
{
    Q_OBJECT
public:
    ViewerSession(QObject *parent);

    // Volumes
    /**
     * Opens a folder or an archive as a volume.
     */
    bool openContainer(const QString &containerPath, bool coverOnly = false);
    /**
     * Opens an entry the caller knows is inside the container.
     */
    bool openEntry(const VolumeLocation &location, bool coverOnly = false);
    /**
     * Opens a plain image file and, once it has been painted, the volume that
     * contains it.
     */
    bool openFileInContainer(const QString &filePath, bool allowSecondPage = false);
    bool nextVolume();
    bool prevVolume();
    void reloadVolumeAfterImageRemoval();
    /**
     * Drops the cached listing of a container whose contents changed on disk.
     */
    void invalidateVolumeCache(const QString &containerPath);
    /**
     * Re-reads the container: the cached listing is dropped and, when the
     * active volume comes from that container, the volume is loaded again on
     * the page that is displayed now.
     */
    void reloadContainer(const QString &containerPath);

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
    bool appendVisiblePage(ImageContent content);
    void clearVisiblePages();
    QSize viewportSize() const { return m_viewportSize; }
    void setViewportSize(QSize size);
    bool initialImagePaintPending() const;
    ViewerStateKind stateKind() const { return viewerStateKind(m_state); }
    const ViewerLoadStatus &loadStatus() const { return m_loadStatus; }
    void deferFolderWorkUntilNextPaint();
    void notifyInitialImagePainted();
    void notifyPagePresentationChanged();
    void updateReadProgress();
    void sortActiveVolumePages(qvEnums::ImageSortBy sortBy);

    // Get String
    int visiblePageCount() const { return m_visiblePages.size(); }
    /**
     * Returns the zero-based index of the first currently visible page.
     */
    int currentPageIndex() const override { return m_pageNavigator.currentPageIndex(); }
    VisiblePages visiblePages() const override { return VisiblePages(m_visiblePages); }
    QString currentPagePath() const override
    {
        Volume *volume = activeVolume();
        if (!volume || m_visiblePages.isEmpty()) {
            return "";
        }
        return volumeLocationDisplayText({volume->volumePath(), m_visiblePages[0].path});
    }
    QString currentPageName() const
    {
        return m_visiblePages.isEmpty() ? QString() : m_visiblePages[0].path;
    }
    /**
     * Address of the currently displayed page, if any. Used by the code that
     * persists the current position.
     */
    VolumeLocation currentLocation() const
    {
        Volume *volume = activeVolume();
        if (!volume || m_visiblePages.isEmpty()) {
            return {};
        }
        return {volume->volumePath(), m_visiblePages[0].path};
    }

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
        m_volumeCache.clear();
        m_savedPagePositions.clear();
        m_loadStatus = ViewerLoadStatus{};
        emit loadStatusChanged();
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
    void archiveOpenFailed(QString path, ArchiveOpenError error);
    void loadStatusChanged();
    /**
     * Emitted after the directly opened image has had a chance to paint. Heavy
     * folder-related GUI work can resume after this signal.
     */
    void initialImageDisplayFinished();

public slots:
    void handleSlideShowStarted();
    void handleSlideShowStopped();

private:
    bool openLocation(const VolumeLocation &location, bool coverOnly);
    void startContainingVolumeLoad(const QString &normalizedImagePath,
                                   const QString &basePath,
                                   const QString &subfileName);
    void finishInitialImageDisplay(quint64 generation);
    CachedVolumeLoadResult loadCachedVolume(const VolumeLocation &location, bool onlyCover);
    void prefetchVolume(const QString &containerPath);
    VolumeHandle activeVolumeHandle() const;
    Volume *activeVolume() const;
    void setVolumeReady(VolumeHandle volume);
    void rememberActivePagePosition();
    bool failActiveArchiveLoad(ArchiveOpenError error, const QString &path);
    void beginLoad(const QString &path, LoadTargetKind targetKind);
    void setLoadFailure(const QString &path,
                        LoadTargetKind targetKind,
                        LoadFailureReason reason,
                        bool terminal);
    void setLoadReady(const QString &path, LoadTargetKind targetKind);
    int initialPageIndex(const VolumeHandle &volume, const QString &pageName, bool coverOnly);
    void replaceVisiblePages(QVector<ImageContent> pages);
    static QStringList siblingVolumeNames(const QDir &directory);

    PageNavigator m_pageNavigator;
    struct SavedPagePosition
    {
        std::weak_ptr<Volume> volume;
        int pageIndex = 0;
    };
    QHash<Volume *, SavedPagePosition> m_savedPagePositions;
    PrefetchMode m_prefetchMode;

    bool m_allowSecondVisiblePage;
    QVector<ImageContent> m_visiblePages;
    VolumeCache m_volumeCache;
    QStringList m_volumeNames;

    ViewerState m_state;
    ViewerLoadStatus m_loadStatus;
    QSize m_viewportSize;

    LatestResultDispatcher<ImageContent> m_initialImageLoadDispatcher;
    LatestResultDispatcher<CachedVolumeLoadResult> m_volumeLoadDispatcher;
    quint64 m_initialDisplayGeneration;
    QString m_pendingContainingImagePath;
    QString m_pendingContainingVolumePath;
    QString m_pendingContainingPageName;
};

#endif // VIEWERSESSION_H
