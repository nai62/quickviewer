#include "viewersession.h"

#include "fileloadersubdirectory.h"
#include "pagedisplayformatter.h"
#include "qvapplication.h"
#include "volumeloader.h"

struct VolumeLocation
{
    QString volumePath;
    QString pageName;
};

static VolumeLocation parseVolumeLocation(const QString &path)
{
    return {
        QDir::fromNativeSeparators(Volume::FullPathToVolumePath(path)),
        Volume::FullPathToSubFilePath(path)};
}

static VolumeCacheKey volumeCacheKey(const QString &volumePath)
{
    return {
        QDir::fromNativeSeparators(volumePath),
        qApp->ShowSubfolders(),
        qApp->ExtractSolidArchiveToTemporaryDir()};
}

static ImageContent waitForImageAt(const Volume &volume, int pageIndex)
{
    const Volume::ImageLoadFuture imageLoad = volume.imageLoadAt(pageIndex);
    return imageLoad.isValid() ? imageLoad.result() : ImageContent();
}

ViewerSession::ViewerSession(QObject *parent)
    : QObject(parent),
      m_prefetchMode(PrefetchMode::Normal),
      m_allowSecondVisiblePage(true),
      m_volumeCache(qApp->MaxVolumesCache()),
      m_state(EmptyViewerState{}),
      m_viewportSize(),
      m_initialImageLoadDispatcher(),
      m_volumeLoadDispatcher(),
      m_initialDisplayGeneration(0)
{}

VolumeHandle ViewerSession::activeVolumeHandle() const
{
    const auto *ready = std::get_if<VolumeReadyViewerState>(&m_state);
    return ready ? ready->volume : VolumeHandle{};
}

Volume *ViewerSession::activeVolume() const
{
    const auto *ready = std::get_if<VolumeReadyViewerState>(&m_state);
    return ready ? ready->volume.get() : nullptr;
}

void ViewerSession::setVolumeReady(VolumeHandle volume)
{
    if (volume) {
        m_state = VolumeReadyViewerState{std::move(volume), std::nullopt};
    } else {
        m_state = FailedViewerState{};
    }
}

void ViewerSession::configureVolume(Volume *volume)
{
    if (!volume) {
        return;
    }
    connect(volume, &Volume::pageListLoaded, this, &ViewerSession::handleVolumePageListLoaded, Qt::UniqueConnection);
}

void ViewerSession::setViewportSize(QSize size)
{
    m_viewportSize = size;
}

void ViewerSession::rememberActivePagePosition()
{
    const VolumeHandle volume = activeVolumeHandle();
    if (!volume) {
        return;
    }
    m_savedPagePositions.insert(
        volume.get(), {volume, m_pageNavigator.currentPageIndex()});
}

int ViewerSession::initialPageIndex(
    const VolumeHandle &volume, const QString &pageName, bool coverOnly)
{
    if (!volume || coverOnly) {
        return 0;
    }
    if (!pageName.isEmpty()) {
        const int specifiedPageIndex = volume->pageIndexForName(pageName);
        if (specifiedPageIndex >= 0) {
            return specifiedPageIndex;
        }
    }

    auto savedPosition = m_savedPagePositions.find(volume.get());
    if (savedPosition != m_savedPagePositions.end()) {
        const VolumeHandle savedVolume = savedPosition->volume.lock();
        if (savedVolume == volume) {
            return savedPosition->pageIndex >= 0 && savedPosition->pageIndex < volume->pageCount()
                       ? savedPosition->pageIndex
                       : 0;
        }
        m_savedPagePositions.erase(savedPosition);
    }

    const QString path = QDir::fromNativeSeparators(volume->volumePath());
    if (qApp->OpenVolumeWithProgress() && !volume->openedWithSpecifiedImageFile() && qApp->readProgressStore()->contains(path)) {
        const int resumePageIndex = qApp->readProgressStore()->at(path).resumePageIndex;
        return resumePageIndex >= 0 && resumePageIndex < volume->pageCount()
                   ? resumePageIndex
                   : 0;
    }
    return 0;
}

bool ViewerSession::initialImagePaintPending() const
{
    if (std::holds_alternative<LoadingViewerState>(m_state)) {
        return true;
    }
    if (const auto *preview = std::get_if<StandalonePreviewViewerState>(&m_state)) {
        return !preview->folderScanStarted;
    }
    if (const auto *ready = std::get_if<VolumeReadyViewerState>(&m_state)) {
        return ready->initialPaintDeferral.has_value();
    }
    return false;
}

bool ViewerSession::loadVolume(QString path, bool coverOnly)
{
    rememberActivePagePosition();
    const quint64 generation = ++m_initialDisplayGeneration;
    m_state = LoadingViewerState{generation};
    m_pendingContainingImagePath.clear();
    m_pendingContainingVolumePath.clear();
    m_pendingContainingPageName.clear();
    m_initialImageLoadDispatcher.invalidate();
    m_volumeLoadDispatcher.invalidate();
    clearVisiblePages();
    VolumeHandle loadedVolume = loadCachedVolume(path, coverOnly);
    if (!loadedVolume) {
        m_state = FailedViewerState{};
        emit volumeChanged("");
        return false;
    }
    setVolumeReady(loadedVolume);
    Volume *volume = loadedVolume.get();
    if (!coverOnly) {
        m_volumeNames = QStringList();
    }
    m_pageNavigator.reset();
    const VolumeLocation location = parseVolumeLocation(path);
    const int initialPage = initialPageIndex(loadedVolume, location.pageName, coverOnly);
    emit volumeChanged(volume->volumePath());
    if (activeVolume() != volume) {
        return true;
    }
    if (coverOnly) {
        m_pageNavigator.selectPage(initialPage, volume->pageCount());
        m_prefetchMode = PrefetchMode::Normal;
        reloadVisiblePages();
        updateReadProgress();
        emit pageChanged();
    } else {
        selectPage(initialPage);
    }
    return true;
}

bool ViewerSession::loadVolumeWithFile(QString path, bool allowSecondPage)
{
    QString normalizedPath = QDir::fromNativeSeparators(path);
    const QFileInfo imageInfo(normalizedPath);
    QString basePath = imageInfo.absolutePath();
    const QString subfileName = imageInfo.fileName();
    if (m_volumeCache.contains(volumeCacheKey(basePath)) || (allowSecondPage && qApp->DualView())) {
        m_allowSecondVisiblePage = allowSecondPage;
        const bool loaded = loadVolume(QString("%1::%2").arg(basePath).arg(subfileName));
        m_allowSecondVisiblePage = true;
        return loaded;
    }

    m_initialImageLoadDispatcher.invalidate();
    m_volumeLoadDispatcher.invalidate();
    const quint64 displayGeneration = ++m_initialDisplayGeneration;
    m_state = LoadingViewerState{displayGeneration};
    m_pendingContainingImagePath.clear();
    m_pendingContainingVolumePath.clear();
    m_pendingContainingPageName.clear();
    const QSize pageSize = viewportSize();
    const QFuture<ImageContent> initialImage = QtConcurrent::run(
        [normalizedPath, pageSize] {
            return Volume::loadImageFromFile(normalizedPath, pageSize);
        });
    m_initialImageLoadDispatcher.submit(
        initialImage,
        [this, normalizedPath, basePath, subfileName, displayGeneration](ImageContent content) mutable {
            m_pendingContainingImagePath = normalizedPath;
            m_pendingContainingVolumePath = basePath;
            m_pendingContainingPageName = subfileName;
            const bool imageReady = !content.loadedImage.isNull() || !content.resizedImage.isNull() || !content.movie.isNull();
            m_state = StandalonePreviewViewerState{
                displayGeneration, imageReady, false, false};
            if (imageReady) {
                m_pageNavigator.reset();
                replaceVisiblePages({std::move(content)});
                emit readyForPaint();
                // Normally paintEvent releases the deferred folder work. Keep
                // a fallback for hidden/minimized windows that may not paint.
                QTimer::singleShot(1000, this, [this, displayGeneration] {
                    finishInitialImageDisplay(displayGeneration);
                });
            } else {
                finishInitialImageDisplay(displayGeneration);
            }
        },
        [](ImageContent) {});
    return true;
}

void ViewerSession::deferFolderWorkUntilNextPaint()
{
    const quint64 displayGeneration = ++m_initialDisplayGeneration;
    auto *ready = std::get_if<VolumeReadyViewerState>(&m_state);
    if (!ready || !ready->volume) {
        return;
    }
    ready->initialPaintDeferral = InitialPaintDeferral{displayGeneration, false};
    m_pendingContainingImagePath.clear();
    m_pendingContainingVolumePath.clear();
    m_pendingContainingPageName.clear();

    QTimer::singleShot(1000, this, [this, displayGeneration] {
        finishInitialImageDisplay(displayGeneration);
    });
}

void ViewerSession::notifyInitialImagePainted()
{
    // Paints of the empty/background view can happen while the image is still
    // decoding. Only release work after the decoded page has been installed.
    quint64 generation = 0;
    if (auto *preview = std::get_if<StandalonePreviewViewerState>(&m_state)) {
        if (preview->folderScanStarted || preview->paintCompletionQueued || !preview->imageReadyForPaint) {
            return;
        }
        preview->paintCompletionQueued = true;
        generation = preview->generation;
    } else if (auto *ready = std::get_if<VolumeReadyViewerState>(&m_state)) {
        if (!ready->initialPaintDeferral || ready->initialPaintDeferral->completionQueued) {
            return;
        }
        ready->initialPaintDeferral->completionQueued = true;
        generation = ready->initialPaintDeferral->generation;
    } else {
        return;
    }
    // Return from paintEvent before starting directory I/O or synchronous GUI
    // updates, so the backing-store paint can be committed first.
    QTimer::singleShot(0, this, [this, generation] {
        finishInitialImageDisplay(generation);
    });
}

void ViewerSession::notifyPagePresentationChanged()
{
    emit pageChanged();
}

void ViewerSession::finishInitialImageDisplay(quint64 generation)
{
    if (generation != m_initialDisplayGeneration) {
        return;
    }

    bool shouldStartContainingVolume = false;
    if (auto *preview = std::get_if<StandalonePreviewViewerState>(&m_state)) {
        if (preview->generation != generation || preview->folderScanStarted) {
            return;
        }
        preview->folderScanStarted = true;
        preview->paintCompletionQueued = false;
        shouldStartContainingVolume = true;
    } else if (auto *ready = std::get_if<VolumeReadyViewerState>(&m_state)) {
        if (!ready->initialPaintDeferral || ready->initialPaintDeferral->generation != generation) {
            return;
        }
        ready->initialPaintDeferral.reset();
    } else {
        return;
    }
    const QString normalizedPath = m_pendingContainingImagePath;
    const QString basePath = m_pendingContainingVolumePath;
    const QString subfileName = m_pendingContainingPageName;
    m_pendingContainingImagePath.clear();
    m_pendingContainingVolumePath.clear();
    m_pendingContainingPageName.clear();

    if (shouldStartContainingVolume && !normalizedPath.isEmpty()) {
        startContainingVolumeLoad(normalizedPath, basePath, subfileName);
    }
    emit initialImageDisplayFinished();
}

void ViewerSession::startContainingVolumeLoad(const QString &normalizedImagePath,
                                              const QString &basePath,
                                              const QString &subfileName)
{
    const VolumeCacheKey cacheKey = volumeCacheKey(basePath);
    QThread *guiThread = thread();
    const VolumeLoadFuture volumeLoad = m_volumeCache.request(cacheKey, [normalizedImagePath, guiThread] {
        return QtConcurrent::run([normalizedImagePath, guiThread] {
            VolumeLoader volumeLoader(normalizedImagePath);
            Volume *volume = volumeLoader.buildForContainingImage();
            if (volume) {
                volume->moveToThread(guiThread);
            }
            return makeVolumeHandle(volume);
        });
    });
    m_volumeLoadDispatcher.submit(
        volumeLoad,
        [this, basePath, subfileName, cacheKey](VolumeHandle loadedVolume) {
            if (!loadedVolume) {
                m_volumeCache.invalidate(cacheKey);
                loadVolume(QString("%1::%2").arg(basePath).arg(subfileName));
                return;
            }
            configureVolume(loadedVolume.get());
            emit volumeChanged("");
            m_volumeCache.markUsed(cacheKey);
            setVolumeReady(loadedVolume);
            Volume *volume = loadedVolume.get();
            clearVisiblePages();
            if (activeVolume() != volume) {
                return;
            }
            const int pageIndex = volume->pageIndexForName(subfileName);
            if (!m_pageNavigator.selectPage(pageIndex, volume->pageCount())) {
                m_pageNavigator.reset();
            }
            m_prefetchMode = PrefetchMode::Normal;
            reloadVisiblePages();
            if (activeVolume() != volume) {
                return;
            }
            emit pageChanged();
            if (activeVolume() != volume) {
                return;
            }
            emit volumeChanged(volume->volumePath());
        },
        [](VolumeHandle) {});
}

void ViewerSession::handleVolumePageListLoaded()
{
    if (auto *source = qobject_cast<Volume *>(sender())) {
        if (source != activeVolume()) {
            return;
        }
    }
    Volume *volume = activeVolume();
    if (!volume) {
        return;
    }
    emit volumeChanged(volume->volumePath());
    emit pageChanged();
}

void ViewerSession::handleSlideShowStarted()
{
    Volume *volume = activeVolume();
    if (!volume) {
        return;
    }
    volume->startSlideShow();
    if (qApp->SlideShowRandomly()) {
        firstPage();
    }
}

void ViewerSession::handleSlideShowStopped()
{
    Volume *volume = activeVolume();
    if (!volume) {
        return;
    }
    volume->stopSlideShow();
    if (qApp->SlideShowRandomly()) {
        firstPage();
    }
}

bool ViewerSession::nextVolume()
{
    Volume *volume = activeVolume();
    if (!volume) {
        return false;
    }
    QDir parentDirectory(volume->volumePath());
    const QFileInfo fileInfo(volume->volumePath());
    const QString currentVolumeName = fileInfo.fileName();
    if (!parentDirectory.cdUp()) {
        return false;
    }
    if (m_volumeNames.isEmpty()) {
        m_volumeNames = siblingVolumeNames(parentDirectory);
    }
    bool beforeMatch = true;
    bool loaded = false;
    int preloadCount = 0;
    foreach (const QString &volumeName, m_volumeNames) {
        if (beforeMatch) {
            if (volumeName == currentVolumeName) {
                beforeMatch = false;
            }
            continue;
        }
        const QString volumePath = parentDirectory.filePath(volumeName);
        if (preloadCount++ == 0) {
            // Continue searching if the next volume cannot be loaded.
            if (!loadVolume(volumePath, true)) {
                preloadCount = 0;
            } else {
                loaded = true;
            }
        } else {
            prefetchVolume(volumePath);
        }
        // preloadCount <- MaxVolumesCache()
        // 0            <- 1
        // 0            <- 2
        // 1            <- 3
        // 1            <- 4
        // 2            <- 5
        // 3            <- 6
        // 4            <- 7
        // 4            <- 8
        // 5            <- 9
        // 6            <-10
        if (preloadCount >= (qApp->MaxVolumesCache() - 1) * 2 / 3) {
            break;
        }
    }
    return loaded;
}

bool ViewerSession::prevVolume()
{
    Volume *volume = activeVolume();
    if (!volume) {
        return false;
    }
    QDir parentDirectory(volume->volumePath());
    const QFileInfo fileInfo(volume->volumePath());
    const QString currentVolumeName = fileInfo.fileName();
    if (!parentDirectory.cdUp()) {
        return false;
    }
    int matchCount = 0;
    bool loaded = false;
    if (m_volumeNames.isEmpty()) {
        m_volumeNames = siblingVolumeNames(parentDirectory);
    }
    QListIterator<QString> volumeNameIterator(m_volumeNames);
    volumeNameIterator.toBack();
    bool beforeMatch = true;
    while (volumeNameIterator.hasPrevious()) {
        const QString volumeName = volumeNameIterator.previous();
        if (beforeMatch) {
            if (volumeName == currentVolumeName) {
                beforeMatch = false;
            }
            continue;
        }
        const QString volumePath = parentDirectory.filePath(volumeName);
        if (matchCount++ == 0) {
            // Continue searching if the previous volume cannot be loaded.
            if (!loadVolume(volumePath, true)) {
                matchCount = 0;
            } else {
                loaded = true;
            }
        } else {
            prefetchVolume(volumePath);
        }
        // preloadCount <- MaxVolumesCache()
        // 0            <- 1
        // 0            <- 2
        // 1            <- 3
        // 1            <- 4
        // 2            <- 5
        // 3            <- 6
        // 4            <- 7
        // 4            <- 8
        // 5            <- 9
        // 6            <-10
        if (matchCount >= (qApp->MaxVolumesCache() - 1) * 2 / 3) {
            break;
        }
    }
    return loaded;
}

void ViewerSession::reloadVolumeAfterImageRemoval()
{
    Volume *volume = activeVolume();
    if (!volume) {
        return;
    }
    clearVisiblePages();
    const QString volumePath = QDir::fromNativeSeparators(volume->volumePath());
    if (volume->pageCount() > 1) {
        const QString fullPath = volume->pagePathWithSeparatorAt(
            m_pageNavigator.currentPageIndex());
        m_volumeCache.invalidate(volumeCacheKey(volumePath));
        m_savedPagePositions.remove(volume);
        m_state = EmptyViewerState{};
        loadVolume(fullPath);
    } else {
        m_volumeCache.invalidate(volumeCacheKey(volumePath));
        m_savedPagePositions.remove(volume);
        m_state = EmptyViewerState{};
    }
}

VolumeHandle ViewerSession::loadCachedVolume(QString path, bool onlyCover)
{
    const VolumeLocation location = parseVolumeLocation(path);
    const VolumeCacheKey key = volumeCacheKey(location.volumePath);
    if (!m_volumeCache.contains(key)) {
        VolumeLoader volumeLoader(location.volumePath);
        Volume *loaded = onlyCover
                             ? volumeLoader.buildForCoverPrefetch()
                             : volumeLoader.build();
        VolumeHandle loadedVolume = makeVolumeHandle(loaded);
        m_volumeCache.insertReady(key, loadedVolume);
    }
    const VolumeLoadFuture cachedLoad = m_volumeCache.request(key, {});
    if (!cachedLoad.isValid()) {
        return {};
    }
    VolumeHandle loadedVolume = cachedLoad.result();
    if (!loadedVolume) {
        m_volumeCache.invalidate(key);
        return {};
    }
    configureVolume(loadedVolume.get());
    m_volumeCache.markUsed(key);
    if (onlyCover) {
        loadedVolume->prefetchCoverImages();
    }

    return loadedVolume;
}

void ViewerSession::prefetchVolume(QString path)
{
    const VolumeLocation location = parseVolumeLocation(path);
    const VolumeCacheKey key = volumeCacheKey(location.volumePath);
    QThread *guiThread = thread();
    m_volumeCache.request(key, [location, guiThread] {
        return QtConcurrent::run([location, guiThread] {
            Volume *volume = VolumeLoader::buildForCoverPrefetchAsync(location.volumePath);
            if (volume) {
                volume->moveToThread(guiThread);
            }
            return makeVolumeHandle(volume);
        });
    });
}

bool ViewerSession::advanceSpread()
{
    Volume *volume = activeVolume();
    if (!volume || !volume->isPageListLoaded()) {
        return false;
    }
    const int nextPageIndex = m_pageNavigator.currentPageIndex() + m_visiblePages.size();
    if (nextPageIndex >= volume->pageCount()) {
        return false;
    }
    return selectPage(nextPageIndex, PrefetchMode::NormalForward);
}

bool ViewerSession::retreatSpread()
{
    Volume *volume = activeVolume();
    if (!volume || !volume->isPageListLoaded() || m_pageNavigator.currentPageIndex() == 0) {
        return false;
    }
    int targetPageIndex = m_pageNavigator.currentPageIndex() - 1;
    if (qApp->DualView() && targetPageIndex >= 1) {
        const ImageContent currentContent = waitForImageAt(*volume, targetPageIndex);
        const ImageContent previousContent = waitForImageAt(*volume, targetPageIndex - 1);
        if (!qApp->WideImageAsOnePageInDualView() || (!currentContent.isLandscape() && !previousContent.isLandscape())) {
            --targetPageIndex;
        }
    }
    selectPage(qMax(0, targetPageIndex), PrefetchMode::NormalBackward);
    return true;
}

#define PAGE_INTERVAL 10

bool ViewerSession::fastForwardPage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0) {
        return false;
    }
    if (m_pageNavigator.currentPageIndex() == volume->pageCount() - 1) {
        return false;
    }
    const int targetPageIndex = qMin(
        m_pageNavigator.currentPageIndex() + PAGE_INTERVAL,
        volume->pageCount() - 1);
    return selectPage(targetPageIndex, PrefetchMode::FastForward);
}

bool ViewerSession::fastBackwardPage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0) {
        return false;
    }
    if (m_pageNavigator.currentPageIndex() == 0) {
        return false;
    }

    const int targetPageIndex = qMax(
        0, m_pageNavigator.currentPageIndex() - PAGE_INTERVAL);
    return selectPage(targetPageIndex, PrefetchMode::FastBackward);
}

bool ViewerSession::selectPage(int pageIndex, PrefetchMode prefetchMode)
{
    Volume *volume = activeVolume();
    if (!volume || pageIndex < 0 || pageIndex >= volume->pageCount()) {
        return false;
    }
    if (!m_pageNavigator.selectPage(pageIndex, volume->pageCount())) {
        return false;
    }
    m_prefetchMode = prefetchMode;
    volume->updatePrefetchCache(pageIndex, prefetchMode, m_viewportSize);

    reloadVisiblePages();
    updateReadProgress();
    emit pageChanged();
    return true;
}

bool ViewerSession::firstPage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0) {
        return false;
    }
    return selectPage(0);
}

bool ViewerSession::lastPage()
{
    Volume *volume = activeVolume();
    if (volume && volume->pageCount() > 0) {
        return selectPage(volume->pageCount() - 1);
    }
    return false;
}

bool ViewerSession::advanceOnePage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0 || m_pageNavigator.currentPageIndex() + m_visiblePages.size() >= volume->pageCount()) {
        return false;
    }
    const int nextPageIndex = qMin(
        m_pageNavigator.currentPageIndex() + 1,
        volume->pageCount() - 1);
    return selectPage(nextPageIndex);
}

bool ViewerSession::retreatOnePage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0) {
        return false;
    }

    if (m_pageNavigator.currentPageIndex() == 0) {
        return false;
    }

    return selectPage(qMax(0, m_pageNavigator.currentPageIndex() - 1));
}

bool ViewerSession::reloadVisiblePages()
{
    VolumeHandle volumeHandle = activeVolumeHandle();
    Volume *volume = volumeHandle.get();
    const int currentPageIndex = m_pageNavigator.currentPageIndex();
    if (!volume || currentPageIndex < 0 || currentPageIndex >= volume->pageCount()) {
        return false;
    }
    ImageContent firstContent = waitForImageAt(*volume, currentPageIndex);
    firstContent.initializeAnimation();
    if (activeVolume() != volume) {
        return false;
    }

    VisiblePageCompositionRequest compositionRequest{
        currentPageIndex,
        volume->pageCount(),
        firstContent.isLandscape(),
        false,
        {qApp->DualView(),
         qApp->FirstImageAsOnePageInDualView(),
         qApp->WideImageAsOnePageInDualView(),
         m_allowSecondVisiblePage}};
    ImageContent secondContent;
    if (VisiblePageComposer::shouldLoadSecondPageCandidate(compositionRequest)) {
        secondContent = waitForImageAt(*volume, currentPageIndex + 1);
        compositionRequest.secondPageIsLandscape = secondContent.isLandscape();
    }
    const VisiblePageComposition composition =
        VisiblePageComposer::compose(compositionRequest);

    QVector<ImageContent> pages;
    pages.push_back(std::move(firstContent));
    if (composition.pageIndexes.size() == 2) {
        secondContent.initializeAnimation();
        pages.push_back(std::move(secondContent));
        volume->updatePrefetchCache(
            composition.prefetchAnchorIndex, m_prefetchMode, m_viewportSize);
        if (activeVolume() != volume) {
            return false;
        }
    }
    replaceVisiblePages(std::move(pages));
    emit readyForPaint();
    return true;
}

bool ViewerSession::appendVisiblePage(ImageContent content)
{
    if (m_visiblePages.size() >= VisiblePages::Capacity) {
        return false;
    }
    content.initializeAnimation();
    m_visiblePages.push_back(std::move(content));
    emit visiblePagesChanged(visiblePages());
    return true;
}

void ViewerSession::clearVisiblePages()
{
    m_visiblePages.clear();
    emit visiblePagesChanged({});
}

void ViewerSession::replaceVisiblePages(QVector<ImageContent> pages)
{
    if (pages.size() > VisiblePages::Capacity) {
        pages.resize(VisiblePages::Capacity);
    }
    for (ImageContent &page : pages) {
        page.initializeAnimation();
    }
    m_visiblePages = std::move(pages);
    emit visiblePagesChanged(visiblePages());
}

void ViewerSession::updateReadProgress()
{
    Volume *volume = activeVolume();
    if (!volume || m_visiblePages.isEmpty() || !qApp->readProgressStore()) {
        return;
    }
    QString path = QDir::fromNativeSeparators(volume->volumePath());
    ReadProgress progress = {
        QFileInfo(volume->volumePath()).fileName(),
        path,
        volume->pageNameAt(m_pageNavigator.currentPageIndex()),
        volume->pageCount(),
        m_pageNavigator.currentPageIndex(),
        false};
    if (m_pageNavigator.currentPageIndex() + m_visiblePages.size() >= pageCount()) {
        progress.completed = true;
        progress.resumePageIndex = 0;
    }
    qApp->readProgressStore()->insert(path, progress);
}

void ViewerSession::sortActiveVolumePages(qvEnums::ImageSortBy sortBy)
{
    if (Volume *volume = activeVolume()) {
        volume->sortPages(sortBy);
        firstPage();
    }
}

QString ViewerSession::currentPageNumberText() const
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0 || m_visiblePages.isEmpty()) {
        return "";
    }
    return PageDisplayFormatter::pageNumberText(
        m_pageNavigator.currentPageIndex(), volume->pageCount(), m_visiblePages.size());
}

QString ViewerSession::currentPageStatusText() const
{
    Volume *volume = activeVolume();
    if (!volume) {
        return {};
    }
    QVector<PageDisplayEntry> visiblePages;
    visiblePages.reserve(m_visiblePages.size());
    for (const ImageContent &content : m_visiblePages) {
        visiblePages.push_back({content.path, content.originalSize});
    }
    return PageDisplayFormatter::statusText(
        m_pageNavigator.currentPageIndex(), volume->pageCount(), visiblePages);
}

QString ViewerSession::pageSignage(int pageIndex) const
{
    Volume *volume = activeVolume();
    if (!volume || pageIndex < 0 || m_visiblePages.size() <= pageIndex) {
        return "";
    }
    return PageDisplayFormatter::signageText(
        QDir::toNativeSeparators(volume->pagePathForName(m_visiblePages[pageIndex].path)),
        m_pageNavigator.currentPageIndex() + pageIndex,
        volume->pageCount());
}

QStringList ViewerSession::siblingVolumeNames(const QDir &directory)
{
    QStringList folders = directory.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::Name);
    IFileLoader::sortFiles(folders);
    QStringList archives = directory.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::Name);
    IFileLoader::sortFiles(archives);
    return folders + archives;
}
