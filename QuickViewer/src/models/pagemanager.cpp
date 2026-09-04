#include "pagemanager.h"
#include "qvapplication.h"
#include "fileloadersubdirectory.h"
#include "volumemanagerbuilder.h"

static QFuture<VolumeHandle> makeReadyVolumeLoad(VolumeHandle volume)
{
    QPromise<VolumeHandle> promise;
    promise.start();
    QFuture<VolumeHandle> volumeLoad = promise.future();
    promise.addResult(std::move(volume));
    promise.finish();
    return volumeLoad;
}

void DeferVolumeLoadCleanup::operator()(QFuture<VolumeHandle> evictedLoad) const
{
    QThreadPool::globalInstance()->start(
        [evictedLoad = std::move(evictedLoad)]() mutable {
            evictedLoad.waitForFinished();
        });
}

PageManager::PageManager(QObject *parent)
    : QObject(parent),
      m_currentPage(0),
      m_currentPageIsLandscape(false),
      m_allowSecondPage(true),
      m_volumeLoadCache(qApp->MaxVolumesCache()),
      m_state(EmptyViewerState{}),
      m_viewportSize(),
      m_initialImageLoadDispatcher(),
      m_volumeLoadDispatcher(),
      m_initialDisplayGeneration(0)
//    , m_builderForAssoc("", this)
{
    installEventFilter(this);
}

VolumeHandle PageManager::activeVolumeHandle() const
{
    const auto *ready = std::get_if<VolumeReadyViewerState>(&m_state);
    return ready ? ready->volume : VolumeHandle{};
}

VolumeManager *PageManager::activeVolume() const
{
    const auto *ready = std::get_if<VolumeReadyViewerState>(&m_state);
    return ready ? ready->volume.get() : nullptr;
}

void PageManager::setVolumeReady(VolumeHandle volume)
{
    if (volume) {
        m_state = VolumeReadyViewerState{std::move(volume), std::nullopt};
    } else {
        m_state = FailedViewerState{};
    }
}

void PageManager::configureVolume(VolumeManager *volume)
{
    if (!volume) {
        return;
    }
    volume->setViewportSize(m_viewportSize);
    connect(volume, &VolumeManager::enumerationFinished, this, &PageManager::handlePageEnumerated, Qt::UniqueConnection);
}

void PageManager::setViewportSize(QSize size)
{
    m_viewportSize = size;
    if (VolumeManager *volume = activeVolume()) {
        volume->setViewportSize(size);
    }
}

bool PageManager::initialImagePaintPending() const
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

bool PageManager::loadVolume(QString path, bool coverOnly)
{
    VolumeHandle previousVolume;
    if (const auto *ready = std::get_if<VolumeReadyViewerState>(&m_state)) {
        previousVolume = ready->volume;
    }
    if (previousVolume && m_pages.size() == 2) {
        previousVolume->prevPage();
    }
    const quint64 generation = ++m_initialDisplayGeneration;
    m_state = LoadingViewerState{generation};
    m_pendingAssociatedPath.clear();
    m_pendingAssociatedBasePath.clear();
    m_pendingAssociatedFileName.clear();
    m_initialImageLoadDispatcher.invalidate();
    m_volumeLoadDispatcher.invalidate();
    clearPages();
    VolumeHandle loadedVolume = getOrLoadVolume(path, coverOnly, true);
    if (!loadedVolume) {
        m_state = FailedViewerState{};
        emit volumeChanged("");
        return false;
    }
    setVolumeReady(loadedVolume);
    VolumeManager *volume = loadedVolume.get();
    if (coverOnly) {
        volume->setCacheMode(VolumeManager::CoverOnly);
    } else {
        m_volumeNames = QStringList();
    }
    m_currentPage = 0;
    const int initialPage = coverOnly ? 0 : volume->pageCount();
    emit volumeChanged(volume->volumePath());
    if (activeVolume() != volume) {
        return true;
    }
    // A folder path that includes a filename may already select a page.
    selectPage(initialPage);
    return true;
}

bool PageManager::loadVolumeWithFile(QString path, bool allowSecondPage)
{
    QString normalizedPath = QDir::fromNativeSeparators(path);
    const QFileInfo imageInfo(normalizedPath);
    QString basePath = imageInfo.absolutePath();
    const QString subfileName = imageInfo.fileName();
    if (m_volumeLoadCache.contains(basePath) || (allowSecondPage && qApp->DualView())) {
        m_allowSecondPage = allowSecondPage;
        bool result = loadVolume(QString("%1::%2").arg(basePath).arg(subfileName));
        m_allowSecondPage = true;
        return result;
    }

    m_initialImageLoadDispatcher.invalidate();
    m_volumeLoadDispatcher.invalidate();
    const quint64 displayGeneration = ++m_initialDisplayGeneration;
    m_state = LoadingViewerState{displayGeneration};
    m_pendingAssociatedPath.clear();
    m_pendingAssociatedBasePath.clear();
    m_pendingAssociatedFileName.clear();
    const QSize pageSize = viewportSize();
    const QFuture<ImageContent> initialImage = QtConcurrent::run(
        [normalizedPath, pageSize] {
            return VolumeManager::loadImageFromFile(normalizedPath, pageSize);
        });
    m_initialImageLoadDispatcher.submit(
        initialImage,
        [this, normalizedPath, basePath, subfileName, displayGeneration](ImageContent content) mutable {
            m_pendingAssociatedPath = normalizedPath;
            m_pendingAssociatedBasePath = basePath;
            m_pendingAssociatedFileName = subfileName;
            const bool imageReady = !content.loadedImage.isNull() || !content.resizedImage.isNull() || !content.movie.isNull();
            m_state = StandalonePreviewViewerState{
                displayGeneration, imageReady, false, false};
            if (imageReady) {
                m_currentPage = 0;
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

void PageManager::deferFolderWorkUntilNextPaint()
{
    const quint64 displayGeneration = ++m_initialDisplayGeneration;
    auto *ready = std::get_if<VolumeReadyViewerState>(&m_state);
    if (!ready || !ready->volume) {
        return;
    }
    ready->initialPaintDeferral = InitialPaintDeferral{displayGeneration, false};
    m_pendingAssociatedPath.clear();
    m_pendingAssociatedBasePath.clear();
    m_pendingAssociatedFileName.clear();

    QTimer::singleShot(1000, this, [this, displayGeneration] {
        finishInitialImageDisplay(displayGeneration);
    });
}

void PageManager::notifyInitialImagePainted()
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

void PageManager::finishInitialImageDisplay(quint64 generation)
{
    if (generation != m_initialDisplayGeneration) {
        return;
    }

    bool shouldStartAssociatedVolume = false;
    if (auto *preview = std::get_if<StandalonePreviewViewerState>(&m_state)) {
        if (preview->generation != generation || preview->folderScanStarted) {
            return;
        }
        preview->folderScanStarted = true;
        preview->paintCompletionQueued = false;
        shouldStartAssociatedVolume = true;
    } else if (auto *ready = std::get_if<VolumeReadyViewerState>(&m_state)) {
        if (!ready->initialPaintDeferral || ready->initialPaintDeferral->generation != generation) {
            return;
        }
        ready->initialPaintDeferral.reset();
    } else {
        return;
    }
    const QString normalizedPath = m_pendingAssociatedPath;
    const QString basePath = m_pendingAssociatedBasePath;
    const QString subfileName = m_pendingAssociatedFileName;
    m_pendingAssociatedPath.clear();
    m_pendingAssociatedBasePath.clear();
    m_pendingAssociatedFileName.clear();

    if (shouldStartAssociatedVolume && !normalizedPath.isEmpty()) {
        startAssociatedVolumeBuild(normalizedPath, basePath, subfileName);
    }
    emit initialImageDisplayFinished();
}

void PageManager::startAssociatedVolumeBuild(const QString &normalizedPath,
                                             const QString &basePath,
                                             const QString &subfileName)
{
    QThread *guiThread = thread();
    const QFuture<VolumeHandle> volumeLoad = QtConcurrent::run([normalizedPath, guiThread] {
        VolumeManagerBuilder builder(normalizedPath);
        VolumeManager *volume = builder.buildForAssoc();
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
            m_volumeLoadCache.insert(basePath, makeReadyVolumeLoad(loadedVolume));
            setVolumeReady(loadedVolume);
            VolumeManager *volume = loadedVolume.get();
            clearPages();
            if (activeVolume() != volume) {
                return;
            }
            m_currentPage = volume->pageCount();
            reloadCurrentPage();
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

void PageManager::handlePageEnumerated()
{
    if (auto *source = qobject_cast<VolumeManager *>(sender())) {
        if (source != activeVolume()) {
            return;
        }
    }
    VolumeManager *volume = activeVolume();
    if (!volume) {
        return;
    }
    m_currentPage = volume->pageCount();
    emit volumeChanged(volume->volumePath());
    emit pageChanged();
}

void PageManager::handleSlideShowStarted()
{
    VolumeManager *volume = activeVolume();
    if (!volume) {
        return;
    }
    volume->startSlideShow();
    if (qApp->SlideShowRandomly()) {
        firstPage();
    }
}

void PageManager::handleSlideShowStopped()
{
    VolumeManager *volume = activeVolume();
    if (!volume) {
        return;
    }
    volume->stopSlideShow();
    if (qApp->SlideShowRandomly()) {
        firstPage();
    }
}

bool PageManager::nextVolume()
{
    VolumeManager *volume = activeVolume();
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
            getOrLoadVolume(path, true, false);
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

bool PageManager::prevVolume()
{
    VolumeManager *volume = activeVolume();
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
            getOrLoadVolume(path, true, false);
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

void PageManager::reloadVolumeAfterRemoveImage()
{
    VolumeManager *volume = activeVolume();
    if (!volume) {
        return;
    }
    clearPages();
    const QString volumePath = QDir::fromNativeSeparators(volume->volumePath());
    if (volume->size() > 1) {
        // if(!m_fileVolume->nextPage())
        //     m_fileVolume->prevPage();
        const QString fullPath = volume->currentPathWithSeparator();
        m_volumeLoadCache.remove(volumePath);
        m_state = EmptyViewerState{};
        loadVolume(fullPath);
    } else {
        m_volumeLoadCache.remove(volumePath);
        m_state = EmptyViewerState{};
    }
}

VolumeHandle PageManager::getOrLoadVolume(QString path, bool onlyCover, bool immediate)
{
    const QString basePath = QDir::fromNativeSeparators(VolumeManager::FullPathToVolumePath(path));
    const QString subfileName = VolumeManager::FullPathToSubFilePath(path);
    if (!m_volumeLoadCache.contains(basePath)) {
        if (!immediate) {
            qDebug() << "getOrLoadVolume:prefetch" << path;
            QThread *guiThread = thread();
            m_volumeLoadCache.insert(basePath, QtConcurrent::run([path, onlyCover, guiThread] {
                                         VolumeManager *volume =
                                             VolumeManagerBuilder::buildAsync(path, onlyCover);
                                         if (volume) {
                                             volume->moveToThread(guiThread);
                                         }
                                         return makeVolumeHandle(volume);
                                     }));
            return {};
        }
        qDebug() << "getOrLoadVolume:immediate" << path;
        VolumeManagerBuilder builder(path);
        VolumeHandle loadedVolume = makeVolumeHandle(builder.build(onlyCover));
        qDebug() << "getOrLoadVolume:immediate" << loadedVolume.get();
        m_volumeLoadCache.insert(basePath, makeReadyVolumeLoad(loadedVolume));
    }
    QFuture<VolumeHandle> *cachedLoad = m_volumeLoadCache.find(basePath);
    if (!cachedLoad) {
        return {};
    }
    if (!immediate && !cachedLoad->isFinished()) {
        return nullptr;
    }

    // Wait until the loading is complete
    VolumeHandle loadedVolume = cachedLoad->result();
    qDebug() << "getOrLoadVolume:loaded" << loadedVolume.get();
    if (!loadedVolume) {
        m_volumeLoadCache.remove(basePath);
        return nullptr;
    }
    configureVolume(loadedVolume.get());
    // Recreate the volume when the subdirectory-search setting changes.
    if (!loadedVolume->isArchive() &&
        ((qApp->ShowSubfolders() && !loadedVolume->hasSubDirectories()) || (!qApp->ShowSubfolders() && loadedVolume->hasSubDirectories()))) {
        qDebug() << qApp->ShowSubfolders() << loadedVolume->hasSubDirectories();
        m_volumeLoadCache.remove(basePath);
    }
    m_volumeLoadCache.touch(basePath);
    qDebug() << "getOrLoadVolume:touch";

    if (!subfileName.isEmpty()) {
        loadedVolume->findImageByName(subfileName);
    }
    return loadedVolume;
}

bool PageManager::nextPage()
{
    //qDebug() << "ImageView::nextPage()" << m_currentPage;
    VolumeManager *volume = activeVolume();
    if (!volume || !volume->enumerated() || volume->pageCount() >= volume->size() - 1) {
        return false;
    }

    volume->setCacheMode(VolumeManager::NormalForward);
    bool result = volume->nextPage();
    if (!result) {
        return false;
    }

    int pageIncr = m_pages.size();
    m_currentPage += pageIncr;
    if (m_currentPage >= volume->size() - 1) {
        m_currentPage = volume->size() - 1;
    }

    reloadCurrentPage();
    bookProgress();
    emit pageChanged();
    return true;
}

bool PageManager::prevPage()
{
    VolumeManager *volume = activeVolume();
    if (!volume || !volume->enumerated() || volume->pageCount() < m_pages.size()) {
        return false;
    }

    volume->setCacheMode(VolumeManager::NormalBackward);
    bool result = volume->prevPage();
    if (!result) {
        return false;
    }
    //QVApplication* app = qApp;
    m_currentPage--;
    if (qApp->DualView() && m_currentPage >= 1) {
        const ImageContent currentContent = volume->getIndexedImageContent(m_currentPage);
        const ImageContent previousContent = volume->getIndexedImageContent(m_currentPage - 1);
        if (!qApp->WideImageAsOnePageInDualView() || (!currentContent.isLandscape() && !previousContent.isLandscape())) {
            m_currentPage--;
        }
    }
    if (m_currentPage < 0) {
        m_currentPage = 0;
    }

    bookProgress();

    selectPage(m_currentPage, VolumeManager::NormalBackward);
    return true;
}

#define PAGE_INTERVAL 10

bool PageManager::fastForwardPage()
{
    VolumeManager *volume = activeVolume();
    if (!volume || volume->size() == 0) {
        return false;
    }
    if (volume->pageCount() == volume->size() - 1) {
        return false;
    }
    m_currentPage += PAGE_INTERVAL;
    if (m_currentPage >= volume->size() - 1) {
        m_currentPage = volume->size() - 1;
    }

    return selectPage(m_currentPage, VolumeManager::FastForward);
}

bool PageManager::fastBackwardPage()
{
    VolumeManager *volume = activeVolume();
    if (!volume || volume->size() == 0) {
        return false;
    }
    if (volume->pageCount() < m_pages.size()) {
        return false;
    }

    m_currentPage -= PAGE_INTERVAL;
    if (m_currentPage < 0) {
        m_currentPage = 0;
    }
    return selectPage(m_currentPage, VolumeManager::FastBackward);
}

bool PageManager::selectPage(int pageIndex, VolumeManager::CacheMode cacheMode)
{
    VolumeManager *volume = activeVolume();
    if (!volume || pageIndex < 0 || pageIndex >= volume->size()) {
        return false;
    }
    volume->setCacheMode(cacheMode);
    const bool result = volume->findPageByIndex(pageIndex);
    if (!result) {
        return false;
    }
    m_currentPage = pageIndex;

    reloadCurrentPage();
    emit pageChanged();
    return true;
}

bool PageManager::firstPage()
{
    VolumeManager *volume = activeVolume();
    if (!volume || volume->size() == 0) {
        return false;
    }
    volume->setCacheMode(VolumeManager::Normal);
    return selectPage(0);
}

bool PageManager::lastPage()
{
    VolumeManager *volume = activeVolume();
    if (volume && volume->size() > 0) {
        volume->setCacheMode(VolumeManager::Normal);
        return selectPage(volume->size() - 1);
    }
    return false;
}

bool PageManager::nextOnlyOnePage()
{
    VolumeManager *volume = activeVolume();
    if (!volume || volume->size() == 0 || volume->pageCount() == volume->size() - 1) {
        return false;
    }
    volume->setCacheMode(VolumeManager::Normal);
    if (m_pages.size() == 1) {
        volume->nextPage();
    }
    m_currentPage++;
    if (m_currentPage >= volume->size() - 1) {
        m_currentPage = volume->size() - 1;
    }
    reloadCurrentPage();
    emit pageChanged();
    return true;
}

bool PageManager::prevOnlyOnePage()
{
    VolumeManager *volume = activeVolume();
    if (!volume || volume->size() == 0) {
        return false;
    }

    if (volume->pageCount() < m_pages.size()) {
        return false;
    }
    volume->setCacheMode(VolumeManager::Normal);

    //QVApplication* app = qApp;
    m_currentPage--;
    if (m_currentPage < 0) {
        m_currentPage = 0;
    }
    return selectPage(m_currentPage);
}

bool PageManager::reloadCurrentPage()
{
    //qDebug() << "ImageView::reloadCurrentPage()";
    VolumeHandle volumeHandle = activeVolumeHandle();
    VolumeManager *volume = volumeHandle.get();
    if (!volume || m_currentPage < 0 || m_currentPage >= volume->size()) {
        return false;
    }
    QVector<ImageContent> pages;
    ImageContent firstContent = volume->getIndexedImageContent(m_currentPage);
    firstContent.initializeAnimation();
    const bool firstPageIsLandscape = firstContent.isLandscape();
    pages.push_back(std::move(firstContent));
    if (activeVolume() != volume) {
        return false;
    }
    m_currentPageIsLandscape = firstPageIsLandscape;
    if (!(m_currentPage == 0 && qApp->FirstImageAsOnePageInDualView()) && canDualView()) {
        if (m_allowSecondPage && volume->pageCount() < volume->size() - 1) {
            ImageContent secondContent = volume->getIndexedImageContent(m_currentPage + 1);
            if (!qApp->WideImageAsOnePageInDualView() || (!firstPageIsLandscape && !secondContent.isLandscape())) {
                volume->nextPage();
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

bool PageManager::addNewPage(ImageContent content, bool append)
{
    if (m_pages.size() >= VisiblePages::Capacity) {
        return false;
    }
    content.initializeAnimation();
    if (!append || m_pages.isEmpty()) {
        m_currentPageIsLandscape = content.isLandscape();
    }
    if (append) {
        m_pages.push_back(std::move(content));
    } else {
        m_pages.push_front(std::move(content));
    }
    emit visiblePagesChanged(visiblePages());
    return true;
}

void PageManager::clearPages()
{
    m_pages.clear();
    m_currentPageIsLandscape = false;
    emit visiblePagesChanged({});
}

void PageManager::replaceVisiblePages(QVector<ImageContent> pages)
{
    if (pages.size() > VisiblePages::Capacity) {
        pages.resize(VisiblePages::Capacity);
    }
    for (ImageContent &page : pages) {
        page.initializeAnimation();
    }
    m_pages = std::move(pages);
    emit visiblePagesChanged(visiblePages());
}

void PageManager::bookProgress()
{
    VolumeManager *volume = activeVolume();
    if (!volume || m_pages.isEmpty() || !qApp->bookshelfManager()) {
        return;
    }
    QString path = QDir::fromNativeSeparators(volume->volumePath());
    BookProgress book = {
        QFileInfo(volume->volumePath()).fileName(),
        path,
        volume->getIndexedFileName(m_currentPage),
        volume->size(),
        m_currentPage,
        false};
    if (m_currentPage + m_pages.size() >= size()) {
        book.Completed = true;
        book.Current = 0;
    }
    qApp->bookshelfManager()->insert(path, book);
}

void PageManager::sort(qvEnums::ImageSortBy sortBy)
{
    if (VolumeManager *volume = activeVolume()) {
        volume->sort(sortBy);
        firstPage();
    }
}

bool PageManager::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case ReloadedEventType: {
        VolumeHandle volume = activeVolumeHandle();
        if (!volume) {
            return true;
        }
        reloadCurrentPage();
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

QString PageManager::currentPageNumberText() const
{
    VolumeManager *volume = activeVolume();
    if (!volume || volume->size() == 0 || m_pages.isEmpty()) {
        return "";
    }
    if (m_pages.size() == 2) {
        return QString("(%1-%2/%3)").arg(m_currentPage + 1).arg(m_currentPage + 2).arg(volume->size());
    }
    return QString("(%1/%2)").arg(m_currentPage + 1).arg(volume->size());
}

QString PageManager::currentPageStatusText() const
{
    const QString pageNumberText = currentPageNumberText();
    QString status;
    switch (m_pages.size()) {
    case 1:
        status = QString("%1 %2[%3x%4]")
                     .arg(m_pages[0].path)
                     .arg(pageNumberText)
                     .arg(m_pages[0].originalSize.width())
                     .arg(m_pages[0].originalSize.height());
        break;
    case 2:
        status = QString("%1 %2[%3x%4] | %5 [%6x%7]")
                     .arg(m_pages[0].path)
                     .arg(pageNumberText)
                     .arg(m_pages[0].originalSize.width())
                     .arg(m_pages[0].originalSize.height())
                     .arg(m_pages[1].path)
                     .arg(m_pages[1].originalSize.width())
                     .arg(m_pages[1].originalSize.height());
        break;
    default:
        break;
    }
    return status;
}

QString PageManager::pageSignage(int pageIndex) const
{
    VolumeManager *volume = activeVolume();
    if (!volume || pageIndex < 0 || m_pages.size() <= pageIndex) {
        return "";
    }
    return QString("%1 (%2/%3)")
        .arg(QDir::toNativeSeparators(volume->getPathByFileName(m_pages[pageIndex].path)))
        .arg(m_currentPage + 1 + pageIndex)
        .arg(volume->size());
}

bool PageManager::canDualView() const
{
    return qApp->DualView() && !(m_currentPageIsLandscape && qApp->WideImageAsOnePageInDualView());
}

QStringList PageManager::enumerateVolumes(const QDir &directory)
{
    QStringList folders = directory.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::Name);
    IFileLoader::sortFiles(folders);
    QStringList archives = directory.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::Name);
    IFileLoader::sortFiles(archives);
    return folders + archives;
}
