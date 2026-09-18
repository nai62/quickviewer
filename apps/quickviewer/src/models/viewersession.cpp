#include "viewersession.h"

#include "fileloadersubdirectory.h"
#include "pagedisplayformatter.h"
#include "qvapplication.h"
#include "volumeloader.h"
#include "startupprofiler.h"

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

static LoadTargetKind targetKindForLocation(const VolumeLocation &location)
{
    if (!location.isContainer()) {
        return LoadTargetKind::ImageFile;
    }
    if (QFileInfo(location.containerPath).isDir()) {
        return LoadTargetKind::Folder;
    }
    if (IFileLoader::isArchiveFile(location.containerPath)) {
        return LoadTargetKind::Archive;
    }
    if (IFileLoader::isImageFile(location.containerPath)) {
        return LoadTargetKind::ImageFile;
    }
    return LoadTargetKind::Unknown;
}

static LoadFailureReason failureReasonForArchiveError(ArchiveOpenError error)
{
    switch (error) {
    case ArchiveOpenError::None:
        return LoadFailureReason::None;
    case ArchiveOpenError::PasswordProtected:
        return LoadFailureReason::PasswordProtected;
    case ArchiveOpenError::Unsupported:
        return LoadFailureReason::UnsupportedFormat;
    case ArchiveOpenError::Corrupt:
        return LoadFailureReason::CorruptData;
    case ArchiveOpenError::IoError:
        return LoadFailureReason::IoError;
    }
    return LoadFailureReason::IoError;
}

ViewerSession::ViewerSession(QObject *parent)
    : QObject(parent),
      m_prefetchMode(PrefetchMode::Normal),
      m_allowSecondVisiblePage(true),
      m_volumeCache(qApp->MaxVolumesCache(), this),
      m_state(EmptyViewerState{}),
      m_viewportSize(),
      m_initialImageLoadDispatcher(),
      m_volumeLoadDispatcher(),
      m_initialDisplayGeneration(0)
{}

void ViewerSession::beginLoad(const QString &path, LoadTargetKind targetKind)
{
    m_loadStatus = {
        ViewerLoadPhase::Loading,
        targetKind,
        LoadFailureReason::None,
        QDir::toNativeSeparators(path),
        {}};
    emit loadStatusChanged();
}

void ViewerSession::setLoadFailure(const QString &path,
                                   LoadTargetKind targetKind,
                                   LoadFailureReason reason,
                                   bool terminal)
{
    m_loadStatus.phase = terminal ? ViewerLoadPhase::Failed
                                  : (activeVolume() ? ViewerLoadPhase::Ready : ViewerLoadPhase::Loading);
    m_loadStatus.targetKind = targetKind;
    m_loadStatus.failureReason = reason;
    if (m_loadStatus.requestedPath.isEmpty()) {
        m_loadStatus.requestedPath = QDir::toNativeSeparators(path);
    }
    m_loadStatus.failurePath = QDir::toNativeSeparators(path);
    emit loadStatusChanged();
}

void ViewerSession::setLoadReady(const QString &path, LoadTargetKind targetKind)
{
    m_loadStatus = {
        ViewerLoadPhase::Ready,
        targetKind,
        LoadFailureReason::None,
        QDir::toNativeSeparators(path),
        {}};
    emit loadStatusChanged();
}

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

bool ViewerSession::failActiveArchiveLoad(ArchiveOpenError error, const QString &path)
{
    if (error == ArchiveOpenError::None) {
        return false;
    }

    const VolumeHandle volume = activeVolumeHandle();
    if (volume) {
        m_savedPagePositions.remove(volume.get());
        m_volumeCache.invalidate(volumeCacheKey(volume->volumePath()));
    }
    ++m_initialDisplayGeneration;
    m_initialImageLoadDispatcher.invalidate();
    m_volumeLoadDispatcher.invalidate();
    m_state = FailedViewerState{};
    clearVisiblePages();
    emit archiveOpenFailed(path, error);
    setLoadFailure(path,
                   LoadTargetKind::Archive,
                   failureReasonForArchiveError(error),
                   true);
    emit volumeChanged("");
    return false;
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
        const ReadProgress progress = qApp->readProgressStore()->at(path);
        // A finished volume starts over, even though the stored name is its
        // last page.
        if (!progress.completed && !progress.currentPageName.isEmpty()) {
            // The stored name follows the page itself, while the stored index
            // only means something for the listing it was recorded with: the
            // panel changes the listing with its sort and its subfolder option,
            // and files come and go.
            const int namedPageIndex = volume->pageIndexForName(progress.currentPageName);
            if (namedPageIndex >= 0) {
                return namedPageIndex;
            }
        }
        const int resumePageIndex = progress.resumePageIndex;
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

bool ViewerSession::openContainer(const QString &containerPath, bool coverOnly)
{
    return openLocation(VolumeLocation{QDir::fromNativeSeparators(containerPath), QString()},
                        coverOnly);
}

bool ViewerSession::openEntry(const VolumeLocation &location, bool coverOnly)
{
    return openLocation(
        VolumeLocation{QDir::fromNativeSeparators(location.containerPath), location.entryName},
        coverOnly);
}

bool ViewerSession::openLocation(const VolumeLocation &location, bool coverOnly)
{
    StartupProfiler::mark("session.load-volume.begin");
    rememberActivePagePosition();
    const quint64 generation = ++m_initialDisplayGeneration;
    const LoadTargetKind targetKind = targetKindForLocation(location);
    beginLoad(location.isContainer() ? location.containerPath : volumeLocationDisplayText(location),
              targetKind);
    m_state = LoadingViewerState{generation};
    m_pendingContainingImagePath.clear();
    m_pendingContainingVolumePath.clear();
    m_pendingContainingPageName.clear();
    m_initialImageLoadDispatcher.invalidate();
    m_volumeLoadDispatcher.invalidate();
    clearVisiblePages();

    const CachedVolumeLoadResult loadResult = loadCachedVolume(location, coverOnly);
    VolumeHandle loadedVolume = loadResult.volume;
    StartupProfiler::mark("session.volume-built");
    if (!loadedVolume) {
        m_state = FailedViewerState{};
        if (loadResult.error != ArchiveOpenError::None) {
            emit archiveOpenFailed(location.containerPath, loadResult.error);
        }
        LoadFailureReason reason = failureReasonForArchiveError(loadResult.error);
        if (reason == LoadFailureReason::None) {
            if (targetKind == LoadTargetKind::Folder || targetKind == LoadTargetKind::Archive) {
                reason = LoadFailureReason::NoViewableImages;
            } else if (!QFileInfo(location.containerPath).exists()) {
                reason = LoadFailureReason::NotFound;
            } else {
                reason = LoadFailureReason::DecodeFailed;
            }
        }
        setLoadFailure(location.containerPath, targetKind, reason, true);
        emit volumeChanged("");
        return false;
    }

    setVolumeReady(loadedVolume);
    Volume *volume = loadedVolume.get();
    setLoadReady(volume->volumePath(), volume->isArchive() ? LoadTargetKind::Archive : targetKind);
    if (!location.isContainer() && volume->pageIndexForName(location.entryName) < 0) {
        m_state = FailedViewerState{};
        setLoadFailure(volumeLocationDisplayText(location),
                       LoadTargetKind::ImageFile,
                       LoadFailureReason::NotFound,
                       true);
        emit volumeChanged("");
        return false;
    }
    if (!coverOnly && volume->isArchive()) {
        deferFolderWorkUntilNextPaint();
    }
    if (!coverOnly) {
        m_volumeNames = QStringList();
    }
    m_pageNavigator.reset();
    const int initialPage = initialPageIndex(loadedVolume, location.entryName, coverOnly);
    if (coverOnly) {
        if (!m_pageNavigator.selectPage(initialPage, volume->pageCount())) {
            return false;
        }
        m_prefetchMode = PrefetchMode::Normal;
        if (!reloadVisiblePages()) {
            return false;
        }
        updateReadProgress();
        emit pageChanged();
    } else if (!selectPage(initialPage)) {
        return false;
    }
    if (activeVolume() != volume) {
        return false;
    }
    emit volumeChanged(volume->volumePath());
    return true;
}

bool ViewerSession::openFileInContainer(const QString &filePath, bool allowSecondPage)
{
    const QString normalizedPath = QDir::fromNativeSeparators(filePath);
    beginLoad(normalizedPath, LoadTargetKind::ImageFile);
    const QFileInfo imageInfo(normalizedPath);
    const QString basePath = imageInfo.absolutePath();
    const QString subfileName = imageInfo.fileName();
    if (m_volumeCache.contains(volumeCacheKey(basePath)) || (allowSecondPage && qApp->DualView())) {
        m_allowSecondVisiblePage = allowSecondPage;
        const bool opened = openEntry(VolumeLocation{basePath, subfileName});
        m_allowSecondVisiblePage = true;
        if (opened || !QFileInfo(normalizedPath).exists()) {
            return opened;
        }
        // The file exists but the cached listing does not know it yet: drop the
        // listing and try once more.
        invalidateVolumeCache(basePath);
        return openEntry(VolumeLocation{basePath, subfileName});
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
            const bool imageReady = content.isRenderable();
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
                setLoadFailure(normalizedPath,
                               LoadTargetKind::ImageFile,
                               QFileInfo(normalizedPath).exists()
                                   ? LoadFailureReason::DecodeFailed
                                   : LoadFailureReason::NotFound,
                               false);
                finishInitialImageDisplay(displayGeneration);
            }
        },
        [](ImageContent) {});
    return true;
}

void ViewerSession::deferFolderWorkUntilNextPaint()
{
    auto *ready = std::get_if<VolumeReadyViewerState>(&m_state);
    if (!ready || !ready->volume || ready->initialPaintDeferral) {
        return;
    }
    const quint64 displayGeneration = ++m_initialDisplayGeneration;
    ready->initialPaintDeferral = InitialPaintDeferral{displayGeneration, false};
    m_pendingContainingImagePath.clear();
    m_pendingContainingVolumePath.clear();
    m_pendingContainingPageName.clear();

    if (!ready->volume->isArchive()) {
        QTimer::singleShot(1000, this, [this, displayGeneration] {
            finishInitialImageDisplay(displayGeneration);
        });
    }
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
    Volume *volumeToPrefetch = nullptr;
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
        volumeToPrefetch = ready->volume.get();
    } else {
        return;
    }
    const QString normalizedPath = m_pendingContainingImagePath;
    const QString basePath = m_pendingContainingVolumePath;
    const QString subfileName = m_pendingContainingPageName;
    m_pendingContainingImagePath.clear();
    m_pendingContainingVolumePath.clear();
    m_pendingContainingPageName.clear();

    StartupProfiler::mark("first-image-painted");
    if (shouldStartContainingVolume && !normalizedPath.isEmpty()) {
        startContainingVolumeLoad(normalizedPath, basePath, subfileName);
    }
    if (volumeToPrefetch && activeVolume() == volumeToPrefetch && !m_visiblePages.isEmpty()) {
        const int prefetchAnchorIndex = m_pageNavigator.currentPageIndex() + m_visiblePages.size() - 1;
        volumeToPrefetch->updatePrefetchCache(
            prefetchAnchorIndex, m_prefetchMode, m_viewportSize);
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
            return CachedVolumeLoadResult{makeVolumeHandle(volume), ArchiveOpenError::None};
        });
    });
    m_volumeLoadDispatcher.submit(
        volumeLoad,
        [this, basePath, subfileName, cacheKey](CachedVolumeLoadResult result) {
            VolumeHandle loadedVolume = result.volume;
            if (!loadedVolume) {
                m_volumeCache.invalidate(cacheKey);
                openEntry(VolumeLocation{basePath, subfileName});
                return;
            }
            configureVolume(loadedVolume.get());
            emit volumeChanged("");
            m_volumeCache.markUsed(cacheKey);
            setVolumeReady(loadedVolume);
            Volume *volume = loadedVolume.get();
            setLoadReady(volume->volumePath(), LoadTargetKind::Folder);
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
        [](CachedVolumeLoadResult) {});
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
            if (!openContainer(volumePath, true)) {
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
            if (!openContainer(volumePath, true)) {
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
    // The displayed page no longer exists, so the reload has to target the page
    // that takes its place: the next one, or the previous one at the end.
    VolumeLocation nextLocation;
    if (!volume->isArchive() && volume->pageCount() > 1) {
        const int currentPageIndex = m_pageNavigator.currentPageIndex();
        const int nextPageIndex =
            volume->pageCount() - 1 == currentPageIndex ? currentPageIndex - 1
                                                        : currentPageIndex + 1;
        nextLocation = {volumePath, volume->pageNameAt(nextPageIndex)};
    }
    invalidateVolumeCache(volumePath);
    m_savedPagePositions.remove(volume);
    m_state = EmptyViewerState{};
    if (!nextLocation.isContainer()) {
        openEntry(nextLocation);
    }
}

void ViewerSession::invalidateVolumeCache(const QString &containerPath)
{
    m_volumeCache.invalidate(volumeCacheKey(QDir::fromNativeSeparators(containerPath)));
}

void ViewerSession::reloadContainer(const QString &containerPath)
{
    const QString path = QDir::fromNativeSeparators(containerPath);
    invalidateVolumeCache(path);

    Volume *volume = activeVolume();
    if (!volume || volume->isArchive() || QDir::fromNativeSeparators(volume->volumePath()) != path) {
        return;
    }
    const QString pageName = currentPageName();
    if (pageName.isEmpty()) {
        return;
    }
    openEntry(VolumeLocation{path, pageName});
}

CachedVolumeLoadResult ViewerSession::loadCachedVolume(const VolumeLocation &location, bool onlyCover)
{
    const VolumeCacheKey key = volumeCacheKey(location.containerPath);

    const ArchiveOpenError prefetchedError = m_volumeCache.takeFailure(key);
    if (prefetchedError != ArchiveOpenError::None) {
        return {{}, prefetchedError};
    }

    if (!m_volumeCache.contains(key)) {
        VolumeLoader volumeLoader(location.containerPath);
        const VolumeBuildResult built = onlyCover
                                            ? volumeLoader.buildForCoverPrefetchResult()
                                            : volumeLoader.buildResult();
        if (!built.volume) {
            return {{}, built.error};
        }
        m_volumeCache.insertReady(key, makeVolumeHandle(built.volume));
    }

    const VolumeLoadFuture cachedLoad = m_volumeCache.request(key, {});
    if (!cachedLoad.isValid()) {
        const ArchiveOpenError error = m_volumeCache.takeFailure(key);
        return {{}, error};
    }
    const CachedVolumeLoadResult result = cachedLoad.result();
    if (!result.volume) {
        m_volumeCache.invalidate(key);
        return result;
    }

    configureVolume(result.volume.get());
    m_volumeCache.markUsed(key);
    if (onlyCover) {
        result.volume->prefetchCoverImages();
    }
    return result;
}

void ViewerSession::prefetchVolume(const QString &containerPath)
{
    const VolumeCacheKey key = volumeCacheKey(containerPath);
    QThread *guiThread = thread();
    m_volumeCache.request(key, [containerPath, guiThread] {
        return QtConcurrent::run([containerPath, guiThread] {
            VolumeLoader loader(containerPath);
            VolumeBuildResult built = loader.buildForCoverPrefetchResult();
            if (built.volume) {
                built.volume->moveToThread(guiThread);
            }
            return CachedVolumeLoadResult{makeVolumeHandle(built.volume), built.error};
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

constexpr int PageInterval = 10;

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
        m_pageNavigator.currentPageIndex() + PageInterval,
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
        0, m_pageNavigator.currentPageIndex() - PageInterval);
    return selectPage(targetPageIndex, PrefetchMode::FastBackward);
}

bool ViewerSession::selectPage(int pageIndex, PrefetchMode prefetchMode)
{
    StartupProfiler::mark("session.select-page.begin");
    Volume *volume = activeVolume();
    if (!volume || pageIndex < 0 || pageIndex >= volume->pageCount()) {
        return false;
    }
    if (!m_pageNavigator.selectPage(pageIndex, volume->pageCount())) {
        return false;
    }
    m_prefetchMode = prefetchMode;
    volume->updatePrefetchCache(
        pageIndex,
        initialImagePaintPending() ? PrefetchMode::InitialDisplay : prefetchMode,
        m_viewportSize);
    StartupProfiler::mark("session.prefetch-scheduled");

    if (!reloadVisiblePages()) {
        return false;
    }
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
    const bool initialDisplayPending = initialImagePaintPending();
    ImageContent firstContent = waitForImageAt(*volume, currentPageIndex);
    StartupProfiler::mark("session.first-image-ready");
    const ArchiveOpenError firstLoadError = volume->fileLoader()
                                                ? volume->fileLoader()->archiveOpenError()
                                                : ArchiveOpenError::None;
    if (firstLoadError != ArchiveOpenError::None) {
        return failActiveArchiveLoad(firstLoadError, volume->volumePath());
    }
    QString failedImagePath;
    if (!firstContent.isRenderable()) {
        failedImagePath =
            volumeLocationDisplayText({volume->volumePath(), volume->pageNameAt(currentPageIndex)});
    }
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
        if (initialDisplayPending) {
            volume->updatePrefetchCache(
                currentPageIndex + 1, PrefetchMode::InitialDisplay, m_viewportSize);
        }
        secondContent = waitForImageAt(*volume, currentPageIndex + 1);
        const ArchiveOpenError secondLoadError = volume->fileLoader()
                                                     ? volume->fileLoader()->archiveOpenError()
                                                     : ArchiveOpenError::None;
        if (secondLoadError != ArchiveOpenError::None) {
            return failActiveArchiveLoad(secondLoadError, volume->volumePath());
        }
        if (failedImagePath.isEmpty() && !secondContent.isRenderable()) {
            failedImagePath = volumeLocationDisplayText(
                {volume->volumePath(), volume->pageNameAt(currentPageIndex + 1)});
        }
        compositionRequest.secondPageIsLandscape = secondContent.isLandscape();
    }
    const VisiblePageComposition composition =
        VisiblePageComposer::compose(compositionRequest);

    QVector<ImageContent> pages;
    pages.push_back(std::move(firstContent));
    if (composition.pageIndexes.size() == 2) {
        secondContent.initializeAnimation();
        pages.push_back(std::move(secondContent));
        if (!initialDisplayPending) {
            volume->updatePrefetchCache(
                composition.prefetchAnchorIndex, m_prefetchMode, m_viewportSize);
        }
        if (activeVolume() != volume) {
            return false;
        }
    }
    replaceVisiblePages(std::move(pages));
    if (failedImagePath.isEmpty()) {
        setLoadReady(volume->volumePath(),
                     volume->isArchive() ? LoadTargetKind::Archive : LoadTargetKind::Folder);
    } else {
        setLoadFailure(failedImagePath,
                       LoadTargetKind::ImageFile,
                       LoadFailureReason::DecodeFailed,
                       false);
    }
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
    Volume *volume = activeVolume();
    if (!volume) {
        return;
    }
    // Sorting only changes the order of the pages, so keep the page that is
    // displayed, and load it again because the decoded caches are keyed by the
    // page index.
    const QString pageName = currentPageName();
    volume->sortPages(sortBy);
    const int pageIndex = pageName.isEmpty() ? -1 : volume->pageIndexForName(pageName);
    if (pageIndex >= 0) {
        selectPage(pageIndex);
    } else {
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
        volumeLocationDisplayText({volume->volumePath(), m_visiblePages[pageIndex].path}),
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
