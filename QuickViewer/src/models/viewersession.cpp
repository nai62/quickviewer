#include "viewersession.h"

#include "fileloadersubdirectory.h"
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
      m_currentPageIndex(0),
      m_firstVisiblePageIsLandscape(false),
      m_allowSecondVisiblePage(true),
      m_volumeCache(qApp->MaxVolumesCache()),
      m_state(EmptyViewerState{}),
      m_viewportSize(),
      m_initialImageLoadDispatcher(),
      m_volumeLoadDispatcher(),
      m_initialDisplayGeneration(0)
//    , m_builderForAssoc("", this)
{
    installEventFilter(this);
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
    volume->setViewportSize(m_viewportSize);
    connect(volume, &Volume::pageListLoaded, this, &ViewerSession::handleVolumePageListLoaded, Qt::UniqueConnection);
}

void ViewerSession::setViewportSize(QSize size)
{
    m_viewportSize = size;
    if (Volume *volume = activeVolume()) {
        volume->setViewportSize(size);
    }
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
    VolumeHandle previousVolume;
    if (const auto *ready = std::get_if<VolumeReadyViewerState>(&m_state)) {
        previousVolume = ready->volume;
    }
    if (previousVolume && m_visiblePages.size() == 2) {
        previousVolume->retreatOnePage();
    }
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
    m_currentPageIndex = 0;
    const int initialPage = coverOnly ? 0 : volume->currentPageIndex();
    emit volumeChanged(volume->volumePath());
    if (activeVolume() != volume) {
        return true;
    }
    // A folder path that includes a filename may already select a page.
    selectPage(initialPage);
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
        bool result = loadVolume(QString("%1::%2").arg(basePath).arg(subfileName));
        m_allowSecondVisiblePage = true;
        return result;
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
                m_currentPageIndex = 0;
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

void ViewerSession::startContainingVolumeLoad(const QString &normalizedPath,
                                              const QString &basePath,
                                              const QString &subfileName)
{
    QThread *guiThread = thread();
    const QFuture<VolumeHandle> volumeLoad = QtConcurrent::run([normalizedPath, guiThread] {
        VolumeLoader volumeLoader(normalizedPath);
        Volume *volume = volumeLoader.buildForContainingImage();
        if (volume) {
            volume->moveToThread(guiThread);
        }
        return makeVolumeHandle(volume);
    });
    m_volumeLoadDispatcher.submit(
        volumeLoad,
        [this, basePath, subfileName](VolumeHandle loadedVolume) {
            if (!loadedVolume) {
                loadVolume(QString("%1::%2").arg(basePath).arg(subfileName));
                return;
            }
            configureVolume(loadedVolume.get());
            emit volumeChanged("");
            m_volumeCache.insertReady(volumeCacheKey(basePath), loadedVolume);
            setVolumeReady(loadedVolume);
            Volume *volume = loadedVolume.get();
            clearVisiblePages();
            if (activeVolume() != volume) {
                return;
            }
            m_currentPageIndex = volume->currentPageIndex();
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
    m_currentPageIndex = volume->currentPageIndex();
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
    QDir dir(volume->volumePath());
    const QFileInfo fileInfo(volume->volumePath());
    const QString current = fileInfo.fileName();
    if (!dir.cdUp()) {
        return false;
    }
    if (m_volumeNames.isEmpty()) {
        m_volumeNames = enumerateVolumes(dir);
    }
    bool beforeMatch = true;
    bool loaded = false;
    int preloadCount = 0;
    foreach (const QString &name, m_volumeNames) {
        if (beforeMatch) {
            if (name == current) {
                beforeMatch = false;
            }
            continue;
        }
        const QString path = dir.filePath(name);
        if (preloadCount++ == 0) {
            // Continue searching if the next volume cannot be loaded.
            if (!loadVolume(path, true)) {
                preloadCount = 0;
            } else {
                loaded = true;
            }
        } else {
            prefetchVolume(path);
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
    QDir dir(volume->volumePath());
    const QFileInfo fileInfo(volume->volumePath());
    const QString current = fileInfo.fileName();
    if (!dir.cdUp()) {
        return false;
    }
    int matchCount = 0;
    bool loaded = false;
    if (m_volumeNames.isEmpty()) {
        m_volumeNames = enumerateVolumes(dir);
    }
    QListIterator<QString> it(m_volumeNames);
    it.toBack();
    bool beforeMatch = true;
    while (it.hasPrevious()) {
        const QString name = it.previous();
        if (beforeMatch) {
            if (name == current) {
                beforeMatch = false;
            }
            continue;
        }
        const QString path = dir.filePath(name);
        if (matchCount++ == 0) {
            // Continue searching if the previous volume cannot be loaded.
            if (!loadVolume(path, true)) {
                matchCount = 0;
            } else {
                loaded = true;
            }
        } else {
            prefetchVolume(path);
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
        // if(!m_fileVolume->nextPage())
        //     m_fileVolume->prevPage();
        const QString fullPath = volume->currentPathWithSeparator();
        m_volumeCache.invalidate(volumeCacheKey(volumePath));
        m_state = EmptyViewerState{};
        loadVolume(fullPath);
    } else {
        m_volumeCache.invalidate(volumeCacheKey(volumePath));
        m_state = EmptyViewerState{};
    }
}

VolumeHandle ViewerSession::loadCachedVolume(QString path, bool onlyCover)
{
    const VolumeLocation location = parseVolumeLocation(path);
    const VolumeCacheKey key = volumeCacheKey(location.volumePath);
    if (!m_volumeCache.contains(key)) {
        VolumeLoader volumeLoader(location.volumePath);
        VolumeHandle loadedVolume = makeVolumeHandle(volumeLoader.build(onlyCover));
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

    if (!location.pageName.isEmpty()) {
        loadedVolume->selectPageByName(location.pageName);
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
            Volume *volume = VolumeLoader::buildAsync(location.volumePath, true);
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
    if (!volume || !volume->isPageListLoaded() || volume->currentPageIndex() >= volume->pageCount() - 1) {
        return false;
    }

    volume->setPrefetchMode(PrefetchMode::NormalForward);
    bool result = volume->advanceOnePage();
    if (!result) {
        return false;
    }

    int pageIncr = m_visiblePages.size();
    m_currentPageIndex += pageIncr;
    if (m_currentPageIndex >= volume->pageCount() - 1) {
        m_currentPageIndex = volume->pageCount() - 1;
    }

    reloadVisiblePages();
    updateReadProgress();
    emit pageChanged();
    return true;
}

bool ViewerSession::retreatSpread()
{
    Volume *volume = activeVolume();
    if (!volume || !volume->isPageListLoaded() || volume->currentPageIndex() < m_visiblePages.size()) {
        return false;
    }

    volume->setPrefetchMode(PrefetchMode::NormalBackward);
    bool result = volume->retreatOnePage();
    if (!result) {
        return false;
    }
    m_currentPageIndex--;
    if (qApp->DualView() && m_currentPageIndex >= 1) {
        const ImageContent currentContent = waitForImageAt(*volume, m_currentPageIndex);
        const ImageContent previousContent = waitForImageAt(*volume, m_currentPageIndex - 1);
        if (!qApp->WideImageAsOnePageInDualView() || (!currentContent.isLandscape() && !previousContent.isLandscape())) {
            m_currentPageIndex--;
        }
    }
    if (m_currentPageIndex < 0) {
        m_currentPageIndex = 0;
    }

    selectPage(m_currentPageIndex, PrefetchMode::NormalBackward);
    return true;
}

#define PAGE_INTERVAL 10

bool ViewerSession::fastForwardPage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0) {
        return false;
    }
    if (volume->currentPageIndex() == volume->pageCount() - 1) {
        return false;
    }
    m_currentPageIndex += PAGE_INTERVAL;
    if (m_currentPageIndex >= volume->pageCount() - 1) {
        m_currentPageIndex = volume->pageCount() - 1;
    }

    return selectPage(m_currentPageIndex, PrefetchMode::FastForward);
}

bool ViewerSession::fastBackwardPage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0) {
        return false;
    }
    if (volume->currentPageIndex() < m_visiblePages.size()) {
        return false;
    }

    m_currentPageIndex -= PAGE_INTERVAL;
    if (m_currentPageIndex < 0) {
        m_currentPageIndex = 0;
    }
    return selectPage(m_currentPageIndex, PrefetchMode::FastBackward);
}

bool ViewerSession::selectPage(int pageIndex, PrefetchMode prefetchMode)
{
    Volume *volume = activeVolume();
    if (!volume || pageIndex < 0 || pageIndex >= volume->pageCount()) {
        return false;
    }
    volume->setPrefetchMode(prefetchMode);
    const bool result = volume->selectPage(pageIndex);
    if (!result) {
        return false;
    }
    m_currentPageIndex = pageIndex;

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
    volume->setPrefetchMode(PrefetchMode::Normal);
    return selectPage(0);
}

bool ViewerSession::lastPage()
{
    Volume *volume = activeVolume();
    if (volume && volume->pageCount() > 0) {
        volume->setPrefetchMode(PrefetchMode::Normal);
        return selectPage(volume->pageCount() - 1);
    }
    return false;
}

bool ViewerSession::advanceOnePage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0 || volume->currentPageIndex() == volume->pageCount() - 1) {
        return false;
    }
    volume->setPrefetchMode(PrefetchMode::Normal);
    if (m_visiblePages.size() == 1) {
        volume->advanceOnePage();
    }
    m_currentPageIndex++;
    if (m_currentPageIndex >= volume->pageCount() - 1) {
        m_currentPageIndex = volume->pageCount() - 1;
    }
    reloadVisiblePages();
    updateReadProgress();
    emit pageChanged();
    return true;
}

bool ViewerSession::retreatOnePage()
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0) {
        return false;
    }

    if (volume->currentPageIndex() < m_visiblePages.size()) {
        return false;
    }
    volume->setPrefetchMode(PrefetchMode::Normal);

    m_currentPageIndex--;
    if (m_currentPageIndex < 0) {
        m_currentPageIndex = 0;
    }
    return selectPage(m_currentPageIndex);
}

bool ViewerSession::reloadVisiblePages()
{
    VolumeHandle volumeHandle = activeVolumeHandle();
    Volume *volume = volumeHandle.get();
    if (!volume || m_currentPageIndex < 0 || m_currentPageIndex >= volume->pageCount()) {
        return false;
    }
    QVector<ImageContent> pages;
    ImageContent firstContent = waitForImageAt(*volume, m_currentPageIndex);
    firstContent.initializeAnimation();
    const bool firstPageIsLandscape = firstContent.isLandscape();
    pages.push_back(std::move(firstContent));
    if (activeVolume() != volume) {
        return false;
    }
    m_firstVisiblePageIsLandscape = firstPageIsLandscape;
    if (!(m_currentPageIndex == 0 && qApp->FirstImageAsOnePageInDualView()) && shouldShowSecondPage()) {
        if (m_allowSecondVisiblePage && volume->currentPageIndex() < volume->pageCount() - 1) {
            ImageContent secondContent = waitForImageAt(*volume, m_currentPageIndex + 1);
            if (!qApp->WideImageAsOnePageInDualView() || (!firstPageIsLandscape && !secondContent.isLandscape())) {
                volume->advanceOnePage();
                secondContent.initializeAnimation();
                pages.push_back(std::move(secondContent));
                if (activeVolume() != volume) {
                    return false;
                }
            }
        }
    }
    replaceVisiblePages(std::move(pages));
    emit readyForPaint();
    return true;
}

bool ViewerSession::addVisiblePage(ImageContent content, bool append)
{
    if (m_visiblePages.size() >= VisiblePages::Capacity) {
        return false;
    }
    content.initializeAnimation();
    if (!append || m_visiblePages.isEmpty()) {
        m_firstVisiblePageIsLandscape = content.isLandscape();
    }
    if (append) {
        m_visiblePages.push_back(std::move(content));
    } else {
        m_visiblePages.push_front(std::move(content));
    }
    emit visiblePagesChanged(visiblePages());
    return true;
}

void ViewerSession::clearVisiblePages()
{
    m_visiblePages.clear();
    m_firstVisiblePageIsLandscape = false;
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
        volume->pageNameAt(m_currentPageIndex),
        volume->pageCount(),
        m_currentPageIndex,
        false};
    if (m_currentPageIndex + m_visiblePages.size() >= pageCount()) {
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

bool ViewerSession::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case ReloadedEventType: {
        VolumeHandle volume = activeVolumeHandle();
        if (!volume) {
            return true;
        }
        reloadVisiblePages();
        if (activeVolume() != volume.get()) {
            return true;
        }
        emit pageChanged();
        if (activeVolume() == volume.get()) {
            emit volumeChanged(volume->volumePath());
        }
        return true;
    }

    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

QString ViewerSession::currentPageNumberText() const
{
    Volume *volume = activeVolume();
    if (!volume || volume->pageCount() == 0 || m_visiblePages.isEmpty()) {
        return "";
    }
    if (m_visiblePages.size() == 2) {
        return QString("(%1-%2/%3)").arg(m_currentPageIndex + 1).arg(m_currentPageIndex + 2).arg(volume->pageCount());
    }
    return QString("(%1/%2)").arg(m_currentPageIndex + 1).arg(volume->pageCount());
}

QString ViewerSession::currentPageStatusText() const
{
    const QString pageNumberText = currentPageNumberText();
    QString status;
    switch (m_visiblePages.size()) {
    case 1:
        status = QString("%1 %2[%3x%4]")
                     .arg(m_visiblePages[0].path)
                     .arg(pageNumberText)
                     .arg(m_visiblePages[0].originalSize.width())
                     .arg(m_visiblePages[0].originalSize.height());
        break;
    case 2:
        status = QString("%1 %2[%3x%4] | %5 [%6x%7]")
                     .arg(m_visiblePages[0].path)
                     .arg(pageNumberText)
                     .arg(m_visiblePages[0].originalSize.width())
                     .arg(m_visiblePages[0].originalSize.height())
                     .arg(m_visiblePages[1].path)
                     .arg(m_visiblePages[1].originalSize.width())
                     .arg(m_visiblePages[1].originalSize.height());
        break;
    default:
        break;
    }
    return status;
}

QString ViewerSession::pageSignage(int pageIndex) const
{
    Volume *volume = activeVolume();
    if (!volume || pageIndex < 0 || m_visiblePages.size() <= pageIndex) {
        return "";
    }
    return QString("%1 (%2/%3)")
        .arg(QDir::toNativeSeparators(volume->pagePathForName(m_visiblePages[pageIndex].path)))
        .arg(m_currentPageIndex + 1 + pageIndex)
        .arg(volume->pageCount());
}

bool ViewerSession::shouldShowSecondPage() const
{
    return qApp->DualView() && !(m_firstVisiblePageIsLandscape && qApp->WideImageAsOnePageInDualView());
}

QStringList ViewerSession::enumerateVolumes(const QDir &directory)
{
    QStringList folders = directory.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::Name);
    IFileLoader::sortFiles(folders);
    QStringList archives = directory.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::Name);
    IFileLoader::sortFiles(archives);
    return folders + archives;
}
