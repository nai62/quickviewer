#ifndef VIEWERSESSION_H
#define VIEWERSESSION_H

#include <QtGui>
#include <memory>
#include "latestresultdispatcher.h"
#include "pagenavigator.h"
#include "visiblepagecomposer.h"
#include "visiblepages.h"
#include "viewerstate.h"
#include "volume.h"
#include "volumecache.h"

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

    bool loadVolume(QString path, bool coverOnly = false);
    bool loadVolumeWithFile(QString path, bool allowSecondPage = false);
    bool nextVolume();
    bool prevVolume();
    void reloadVolumeAfterImageRemoval();

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
    void deferFolderWorkUntilNextPaint();
    void notifyInitialImagePainted();
    void notifyPagePresentationChanged();
    void updateReadProgress();
    void sortActiveVolumePages(qvEnums::ImageSortBy sortBy);

    int visiblePageCount() const { return m_visiblePages.size(); }
    int currentPageIndex() const override { return m_pageNavigator.currentPageIndex(); }
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
        const int currentPageIndex = m_pageNavigator.currentPageIndex();
        const int index = volume->pageCount() - 1 == currentPageIndex ? currentPageIndex - 1 : currentPageIndex + 1;
        return QDir::toNativeSeparators(volume->pagePathAt(index));
    }
    QString currentPageName() const { return m_visiblePages.isEmpty() ? QString() : m_visiblePages[0].path; }

    QString currentPageNumberText() const;
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
    }

signals:
    void visiblePagesChanged(VisiblePages pages);
    void readyForPaint();
    void pageChanged();
    void volumeChanged(QString path);
    void archiveOpenFailed(QString path, ArchiveOpenError error);
    void initialImageDisplayFinished();

public slots:
    void handleVolumePageListLoaded();
    void handleSlideShowStarted();
    void handleSlideShowStopped();

private:
    void startContainingVolumeLoad(const QString &normalizedImagePath,
                                   const QString &basePath,
                                   const QString &subfileName);
    void finishInitialImageDisplay(quint64 generation);
    CachedVolumeLoadResult loadCachedVolume(QString path, bool onlyCover);
    void prefetchVolume(QString path);
    VolumeHandle activeVolumeHandle() const;
    Volume *activeVolume() const;
    void setVolumeReady(VolumeHandle volume);
    void configureVolume(Volume *volume);
    void rememberActivePagePosition();
    bool failActiveArchiveLoad(ArchiveOpenError error, const QString &path);
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
    QSize m_viewportSize;

    LatestResultDispatcher<ImageContent> m_initialImageLoadDispatcher;
    LatestResultDispatcher<CachedVolumeLoadResult> m_volumeLoadDispatcher;
    quint64 m_initialDisplayGeneration;
    QString m_pendingContainingImagePath;
    QString m_pendingContainingVolumePath;
    QString m_pendingContainingPageName;
};

#endif // VIEWERSESSION_H
