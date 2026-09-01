#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <QtGui>
#include "volumemanager.h"
#include "volumemanagerbuilder.h"
#include "latestresultdispatcher.h"
#include "visiblepages.h"
#include "viewerstate.h"

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

#define ReloadedEventType (QEvent::Type)(QEvent::Type::User + 50)

class ReloadedEvent : public QEvent {
public:
    ReloadedEvent() : QEvent(ReloadedEventType) {}
};


class PageManager : public QObject, public PageManagerProtocol
{
    Q_OBJECT
public:
    PageManager(QObject* parent);

    // Volumes
    bool loadVolume(QString path, bool coverOnly=false);
    bool loadVolumeWithFile(QString path, bool prohibitProhibit2Page=false);
    bool nextVolume();
    bool prevVolume();
    void reloadVolumeAfterRemoveImage();

    // Pages
    bool nextPage();
    bool prevPage();
    bool fastForwardPage();
    bool fastBackwardPage();
    bool selectPage(int pageNum, VolumeManager::CacheMode cacheMode=VolumeManager::Normal);
    bool firstPage();
    bool lastPage();
    bool nextOnlyOnePage();
    bool prevOnlyOnePage();
    bool reloadCurrentPage(bool pageNext = true);
    bool addNewPage(ImageContent ic, bool pageNext);
    void clearPages();
    QSize viewportSize() const { return m_viewportSize; }
    void setViewportSize(QSize size);
    bool initialImagePaintPending() const;
    ViewerStateKind stateKind() const { return viewerStateKind(m_state); }
    void deferFolderWorkUntilNextPaint();
    void notifyInitialImagePainted();
    void bookProgress();
    void sort(qvEnums::ImageSortBy sortBy);
    virtual bool eventFilter(QObject *obj, QEvent *event) override;

    // Get String
    int currentPageCount() const { return m_pages.size(); }
    int currentPage() const override { return m_currentPage; }
    VisiblePages visiblePages() const override { return VisiblePages(m_pages); }
    QString currentPagePath() const override {
        VolumeManager *volume = activeVolume();
        if(!volume || m_pages.isEmpty())
            return "";
        return QDir::toNativeSeparators(volume->getPathByFileName(m_pages[0].Path));
    }
    QString nextPagePathAfterDeleted() {
        VolumeManager *volume = activeVolume();
        if(!volume || volume->isArchive() || volume->size() <= 1)
            return "";
        int idx = volume->size()-1==m_currentPage ? m_currentPage-1 : m_currentPage+1;
        return QDir::toNativeSeparators(volume->getPathByIndex(idx));
    }
    QString currentPageName() { return m_pages.isEmpty() ? QString() : m_pages[0].Path; }

    /**
     * @brief currentPageNumAsString: for the label text on PageBar
     * @return (10-11/2182)
     *      or (10/2182)
     */
    QString currentPageNumAsString() const;
    /**
     * @brief currentPageStatusAsString: for statusbar
     * @return some1.jpg (10-11/2182)[WIDTHxHEIGHT] | some2.jpg [WIDTHxHEIGHT]
     *      or some1.jpg (10/2182)[WIDTHxHEIGHT]
     */
    QString currentPageStatusAsString() const;
    QString pageSignage(int page) const;

    QString volumePath() const override {
        VolumeManager *volume = activeVolume();
        return volume ? volume->volumePath() : "";
    }
    QString realVolumePath() {
        VolumeManager *volume = activeVolume();
        return volume ? volume->realVolumePath() : "";
    }
    bool isArchive() {
        VolumeManager *volume = activeVolume();
        return volume && volume->isArchive();
    }
    bool isFolder() {
        VolumeManager *volume = activeVolume();
        return volume && !volume->isArchive();
    }

    int size() const override {
        VolumeManager *volume = activeVolume();
        return volume ? volume->size() : 0;
    }
    bool canDualView() const;
    void dispose() {
        ++m_initialDisplayGeneration;
        m_state = EmptyViewerState{};
        m_pendingAssociatedPath.clear();
        m_pendingAssociatedPathbase.clear();
        m_pendingAssociatedFilename.clear();
        m_initialImageLoads.invalidate();
        m_volumeLoads.invalidate();
        clearPages();
        m_volumes.clear();
    }
    QStringList enumVolumes(QDir dir);

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
    void on_pageEnumerated();
    void onSlideShowStarted();
    void onSlideShowStopped();


private:
    void startAssociatedVolumeBuild(const QString &qpath,
                                    const QString &pathbase,
                                    const QString &subfilename);
    void finishInitialImageDisplay(quint64 generation);
    VolumeHandle addVolumeCache(QString path, bool onlyCover, bool immediate);
    VolumeHandle activeVolumeHandle() const;
    VolumeManager *activeVolume() const;
    void setVolumeReady(VolumeHandle volume);
    void configureVolume(VolumeManager *volume);
    void replaceVisiblePages(QVector<ImageContent> pages);
    /**
     * @brief younger page number
     */
    int m_currentPage;

    bool m_wideImage;
    bool m_prohibit2Pages;
    QVector<ImageContent> m_pages;
    TimeOrderdCacheFutureSharedPtr<QString, VolumeManager> m_volumes;
    QStringList m_volumenames;

    ViewerState m_state;
    QSize m_viewportSize;

    bool m_waitForReloaded;

    LatestResultDispatcher<ImageContent> m_initialImageLoads;
    LatestResultDispatcher<VolumeHandle> m_volumeLoads;
    quint64 m_initialDisplayGeneration;
    QString m_pendingAssociatedPath;
    QString m_pendingAssociatedPathbase;
    QString m_pendingAssociatedFilename;

//    VolumeManagerBuilder m_builderForAssoc;
};

#endif // PAGEMANAGER_H
