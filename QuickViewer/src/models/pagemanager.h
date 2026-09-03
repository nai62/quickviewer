#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <QtGui>
#include "latestresultdispatcher.h"
#include "visiblepages.h"
#include "viewerstate.h"
#include "volumemanager.h"

class VolumeManager;
class PageManagerProtocol
{
public:
    virtual ~PageManagerProtocol() = default;
    virtual int size() const = 0;
    virtual int currentPage() const = 0;
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

class PageManager : public QObject, public PageManagerProtocol
{
    Q_OBJECT
public:
    PageManager(QObject *parent);

    // Volumes
    bool loadVolume(QString path, bool coverOnly = false);
    bool loadVolumeWithFile(QString path, bool allowSecondPage = false);
    bool nextVolume();
    bool prevVolume();
    void reloadVolumeAfterRemoveImage();

    // Pages
    bool nextPage();
    bool prevPage();
    bool fastForwardPage();
    bool fastBackwardPage();
    bool selectPage(int pageIndex, VolumeManager::CacheMode cacheMode = VolumeManager::Normal);
    bool firstPage();
    bool lastPage();
    bool nextOnlyOnePage();
    bool prevOnlyOnePage();
    bool reloadCurrentPage();
    bool addNewPage(ImageContent content, bool append);
    void clearPages();
    QSize viewportSize() const { return m_viewportSize; }
    void setViewportSize(QSize size);
    bool initialImagePaintPending() const;
    ViewerStateKind stateKind() const { return viewerStateKind(m_state); }
    void deferFolderWorkUntilNextPaint();
    void notifyInitialImagePainted();
    void bookProgress();
    void sort(qvEnums::ImageSortBy sortBy);
    bool eventFilter(QObject *obj, QEvent *event) override;

    // Get String
    int currentPageCount() const { return m_pages.size(); }
    int currentPage() const override { return m_currentPage; }
    VisiblePages visiblePages() const override { return VisiblePages(m_pages); }
    QString currentPagePath() const override
    {
        VolumeManager *volume = activeVolume();
        if (!volume || m_pages.isEmpty()) {
            return "";
        }
        return QDir::toNativeSeparators(volume->getPathByFileName(m_pages[0].path));
    }
    QString nextPagePathAfterDeleted() const
    {
        VolumeManager *volume = activeVolume();
        if (!volume || volume->isArchive() || volume->size() <= 1) {
            return "";
        }
        const int index = volume->size() - 1 == m_currentPage ? m_currentPage - 1 : m_currentPage + 1;
        return QDir::toNativeSeparators(volume->getPathByIndex(index));
    }
    QString currentPageName() const { return m_pages.isEmpty() ? QString() : m_pages[0].path; }

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
        VolumeManager *volume = activeVolume();
        return volume ? volume->volumePath() : "";
    }
    QString realVolumePath() const
    {
        VolumeManager *volume = activeVolume();
        return volume ? volume->realVolumePath() : "";
    }
    bool isArchive() const
    {
        VolumeManager *volume = activeVolume();
        return volume && volume->isArchive();
    }
    bool isFolder() const
    {
        VolumeManager *volume = activeVolume();
        return volume && !volume->isArchive();
    }

    int size() const override
    {
        VolumeManager *volume = activeVolume();
        return volume ? volume->size() : 0;
    }
    bool canDualView() const;
    void dispose()
    {
        ++m_initialDisplayGeneration;
        m_state = EmptyViewerState{};
        m_pendingAssociatedPath.clear();
        m_pendingAssociatedBasePath.clear();
        m_pendingAssociatedFileName.clear();
        m_initialImageLoads.invalidate();
        m_volumeLoads.invalidate();
        clearPages();
        m_volumes.clear();
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
    void handlePageEnumerated();
    void handleSlideShowStarted();
    void handleSlideShowStopped();

private:
    void startAssociatedVolumeBuild(const QString &normalizedPath,
                                    const QString &basePath,
                                    const QString &subfileName);
    void finishInitialImageDisplay(quint64 generation);
    VolumeHandle addVolumeCache(QString path, bool onlyCover, bool immediate);
    VolumeHandle activeVolumeHandle() const;
    VolumeManager *activeVolume() const;
    void setVolumeReady(VolumeHandle volume);
    void configureVolume(VolumeManager *volume);
    void replaceVisiblePages(QVector<ImageContent> pages);
    static QStringList enumerateVolumes(const QDir &directory);
    /**
     * @brief Index of the first currently visible page
     */
    int m_currentPage;

    bool m_currentPageIsLandscape;
    bool m_allowSecondPage;
    QVector<ImageContent> m_pages;
    TimeOrderdCacheFutureSharedPtr<QString, VolumeManager> m_volumes;
    QStringList m_volumeNames;

    ViewerState m_state;
    QSize m_viewportSize;

    LatestResultDispatcher<ImageContent> m_initialImageLoads;
    LatestResultDispatcher<VolumeHandle> m_volumeLoads;
    quint64 m_initialDisplayGeneration;
    QString m_pendingAssociatedPath;
    QString m_pendingAssociatedBasePath;
    QString m_pendingAssociatedFileName;

    //    VolumeManagerBuilder m_builderForAssoc;
};

#endif // PAGEMANAGER_H
