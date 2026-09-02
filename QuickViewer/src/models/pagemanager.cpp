#include "pagemanager.h"
#include "qvapplication.h"
#include "fileloadersubdirectory.h"
#include "volumemanagerbuilder.h"

static QFuture<VolumeHandle> readyVolumeFuture(VolumeHandle volume)
{
    QPromise<VolumeHandle> promise;
    promise.start();
    QFuture<VolumeHandle> future = promise.future();
    promise.addResult(std::move(volume));
    promise.finish();
    return future;
}

PageManager::PageManager(QObject *parent)
    : QObject(parent),
      m_currentPage(0),
      m_wideImage(false),
      m_prohibit2Pages(false),
      m_volumes(qApp->MaxVolumesCache()),
      m_state(EmptyViewerState{}),
      m_viewportSize(),
      m_initialImageLoads(),
      m_volumeLoads(),
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
    m_pendingAssociatedPathbase.clear();
    m_pendingAssociatedFilename.clear();
    m_initialImageLoads.invalidate();
    m_volumeLoads.invalidate();
    clearPages();
    VolumeHandle newer = addVolumeCache(path, coverOnly, true);
    if (!newer) {
        m_state = FailedViewerState{};
        emit volumeChanged("");
        return false;
    }
    setVolumeReady(newer);
    VolumeManager *volume = newer.get();
    if (coverOnly) {
        volume->setCacheMode(VolumeManager::CoverOnly);
    } else {
        m_volumenames = QStringList();
    }
    m_currentPage = 0;
    const int initialPage = coverOnly ? 0 : volume->pageCount();
    emit volumeChanged(volume->volumePath());
    if (activeVolume() != volume) {
        return true;
    }
    // if volume is folder and the path incluces filename, pageCount() != 0
    selectPage(initialPage);
    return true;
}

bool PageManager::loadVolumeWithFile(QString path, bool prohibitProhibit2Page)
{
    QString qpath = QDir::fromNativeSeparators(path);
    const QFileInfo imageInfo(qpath);
    QString pathbase = imageInfo.absolutePath();
    const QString subfilename = imageInfo.fileName();
    if (m_volumes.contains(pathbase) || (prohibitProhibit2Page && qApp->DualView())) {
        m_prohibit2Pages = !prohibitProhibit2Page;
        bool result = loadVolume(QString("%1::%2").arg(pathbase).arg(subfilename));
        m_prohibit2Pages = false;
        return result;
    }

    m_initialImageLoads.invalidate();
    m_volumeLoads.invalidate();
    const quint64 displayGeneration = ++m_initialDisplayGeneration;
    m_state = LoadingViewerState{displayGeneration};
    m_pendingAssociatedPath.clear();
    m_pendingAssociatedPathbase.clear();
    m_pendingAssociatedFilename.clear();
    const QSize pageSize = viewportSize();
    const QFuture<ImageContent> initialImage = QtConcurrent::run(
        [qpath, pageSize] {
            return VolumeManager::loadImageFromFile(qpath, pageSize);
        });
    m_initialImageLoads.submit(initialImage, [this, qpath, pathbase, subfilename, displayGeneration](ImageContent content) mutable {
            m_pendingAssociatedPath = qpath;
            m_pendingAssociatedPathbase = pathbase;
            m_pendingAssociatedFilename = subfilename;
            const bool imageReady = !content.Image.isNull() || !content.ResizedImage.isNull()
                    || !content.Movie.isNull();
            m_state = StandalonePreviewViewerState{
                displayGeneration, imageReady, false, false};
            if(imageReady) {
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
            } }, [](ImageContent) {});
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
    m_pendingAssociatedPathbase.clear();
    m_pendingAssociatedFilename.clear();

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
    const QString qpath = m_pendingAssociatedPath;
    const QString pathbase = m_pendingAssociatedPathbase;
    const QString subfilename = m_pendingAssociatedFilename;
    m_pendingAssociatedPath.clear();
    m_pendingAssociatedPathbase.clear();
    m_pendingAssociatedFilename.clear();

    if (shouldStartAssociatedVolume && !qpath.isEmpty()) {
        startAssociatedVolumeBuild(qpath, pathbase, subfilename);
    }
    emit initialImageDisplayFinished();
}

void PageManager::startAssociatedVolumeBuild(const QString &qpath,
                                             const QString &pathbase,
                                             const QString &subfilename)
{
    QThread *guiThread = thread();
    const QFuture<VolumeHandle> future = QtConcurrent::run([qpath, guiThread] {
        VolumeManagerBuilder builder(qpath);
        VolumeManager *newer = builder.buildForAssoc();
        if (newer) {
            newer->moveToThread(guiThread);
        }
        return makeVolumeHandle(newer);
    });
    m_volumeLoads.submit(future, [this, pathbase, subfilename](VolumeHandle newer) {
            if(!newer) {
                loadVolume(QString("%1::%2").arg(pathbase).arg(subfilename));
                return;
            }
            configureVolume(newer.get());
            emit volumeChanged("");
            m_volumes.insert(pathbase, readyVolumeFuture(newer));
            setVolumeReady(newer);
            VolumeManager *volume = newer.get();
            clearPages();
            if(activeVolume() != volume){
                return;
}
            m_currentPage = volume->pageCount();
            reloadCurrentPage(true);
            if(activeVolume() != volume){
                return;
}
            emit pageChanged();
            if(activeVolume() != volume){
                return;
}
            emit volumeChanged(volume->volumePath()); }, [](VolumeHandle) {});
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
    QFileInfo fileinfo(volume->volumePath());
    QString current = fileinfo.fileName();
    if (!dir.cdUp()) {
        return false;
    }
    if (m_volumenames.size() == 0) {
        m_volumenames = enumVolumes(dir);
    }
    bool beforeMatch = true;
    bool loaded = false;
    int preloadCount = 0;
    foreach (const QString &name, m_volumenames) {
        if (beforeMatch) {
            if (name == current) {
                beforeMatch = false;
            }
            continue;
        }
        QString path = dir.filePath(name);
        if (preloadCount++ == 0) {
            // if load new volume failed, search continue
            if (!loadVolume(path, true)) {
                preloadCount = 0;
            } else {
                loaded = true;
            }
        } else {
            addVolumeCache(path, true, false);
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
    QFileInfo fileinfo(volume->volumePath());
    QString current = fileinfo.fileName();
    if (!dir.cdUp()) {
        return false;
    }
    int matchCount = 0;
    bool loaded = false;
    if (m_volumenames.size() == 0) {
        m_volumenames = enumVolumes(dir);
    }
    QListIterator<QString> it(m_volumenames);
    it.toBack();
    bool beforeMatch = true;
    while (it.hasPrevious()) {
        QString name = it.previous();
        if (beforeMatch) {
            if (name == current) {
                beforeMatch = false;
            }
            continue;
        }
        QString path = dir.filePath(name);
        if (matchCount++ == 0) {
            // if load new volume failed, search continue
            if (!loadVolume(path, true)) {
                matchCount = 0;
            } else {
                loaded = true;
            }
        } else {
            addVolumeCache(path, true, false);
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
    QString volumepath = QDir::fromNativeSeparators(volume->volumePath());
    if (volume->size() > 1) {
        // if(!m_fileVolume->nextPage())
        //     m_fileVolume->prevPage();
        QString fullpath = volume->currentPathWithSeparator();
        m_volumes.remove(volumepath);
        m_state = EmptyViewerState{};
        loadVolume(fullpath);
    } else {
        m_volumes.remove(volumepath);
        m_state = EmptyViewerState{};
    }
}

VolumeHandle PageManager::addVolumeCache(QString path, bool onlyCover, bool immediate)
{
    QString pathbase = QDir::fromNativeSeparators(VolumeManager::FullPathToVolumePath(path));
    QString subfilename = VolumeManager::FullPathToSubFilePath(path);
    if (!m_volumes.contains(pathbase)) {
        if (!immediate) {
            qDebug() << "addVolumeCache:prefetch" << path;
            QThread *guiThread = thread();
            m_volumes.insert(pathbase, QtConcurrent::run([path, onlyCover, guiThread] {
                                 VolumeManager *volume =
                                     VolumeManagerBuilder::buildAsync(path, onlyCover);
                                 if (volume) {
                                     volume->moveToThread(guiThread);
                                 }
                                 return makeVolumeHandle(volume);
                             }));
            return {};
        }
        qDebug() << "addVolumeCache:immediate" << path;
        VolumeManagerBuilder builder(path);
        VolumeHandle imm = makeVolumeHandle(builder.build(onlyCover));
        qDebug() << "addVolumeCache:imm" << imm.get();
        m_volumes.insert(pathbase, readyVolumeFuture(imm));
    }
    QFuture<VolumeHandle> future = m_volumes.object(pathbase);
    if (!immediate && !future.isFinished()) {
        return nullptr;
    }

    // Wait until the loading is complete
    VolumeHandle newer = future.result();
    qDebug() << "addVolumeCache:newer" << newer.get();
    if (!newer) {
        m_volumes.remove(pathbase);
        return nullptr;
    }
    configureVolume(newer.get());
    // If the subdirectory search is switched valid, we need to recreate the instance
    if (!newer->isArchive() &&
        ((qApp->ShowSubfolders() && !newer->hasSubDirectories()) || (!qApp->ShowSubfolders() && newer->hasSubDirectories()))) {
        qDebug() << qApp->ShowSubfolders() << newer->hasSubDirectories();
        m_volumes.remove(pathbase);
    }
    m_volumes.retain(pathbase);
    qDebug() << "addVolumeCache:retain";

    if (newer && subfilename.length()) {
        newer->findImageByName(subfilename);
    }
    return newer;
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
        const ImageContent ic0 = volume->getIndexedImageContent(m_currentPage);
        const ImageContent ic1 = volume->getIndexedImageContent(m_currentPage - 1);
        if (!qApp->WideImageAsOnePageInDualView() || (!ic0.wideImage() && !ic1.wideImage())) {
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

bool PageManager::selectPage(int idx, VolumeManager::CacheMode cacheMode)
{
    //qDebug() << "PageManager::selectPage()" << idx;
    VolumeManager *volume = activeVolume();
    if (!volume || idx < 0 || idx >= volume->size()) {
        return false;
    }
    volume->setCacheMode(cacheMode);
    bool result = volume->findPageByIndex(idx);
    if (!result) {
        return false;
    }
    m_currentPage = idx;

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

bool PageManager::reloadCurrentPage(bool)
{
    //qDebug() << "ImageView::reloadCurrentPage()";
    VolumeHandle volumeHandle = activeVolumeHandle();
    VolumeManager *volume = volumeHandle.get();
    if (!volume || m_currentPage < 0 || m_currentPage >= volume->size()) {
        return false;
    }
    QVector<ImageContent> pages;
    ImageContent ic0 = volume->getIndexedImageContent(m_currentPage);
    ic0.initialize();
    pages.push_back(ic0);
    if (activeVolume() != volume) {
        return false;
    }
    m_wideImage = ic0.wideImage();
    if (!(m_currentPage == 0 && qApp->FirstImageAsOnePageInDualView()) && canDualView()) {
        if (!m_prohibit2Pages && volume->pageCount() < volume->size() - 1) {
            ImageContent ic1 = volume->getIndexedImageContent(m_currentPage + 1);
            if (!qApp->WideImageAsOnePageInDualView() || (!ic0.wideImage() && !ic1.wideImage())) {
                volume->nextPage();
                ic1.initialize();
                pages.push_back(ic1);
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

bool PageManager::addNewPage(ImageContent ic, bool pageNext)
{
    if (m_pages.size() >= VisiblePages::Capacity) {
        return false;
    }
    ic.initialize();
    if (pageNext) {
        m_pages.push_back(ic);
    } else {
        m_pages.push_front(ic);
    }
    emit visiblePagesChanged(visiblePages());
    return true;
}

void PageManager::clearPages()
{
    m_pages.clear();
    emit visiblePagesChanged({});
}

void PageManager::replaceVisiblePages(QVector<ImageContent> pages)
{
    if (pages.size() > VisiblePages::Capacity) {
        pages.resize(VisiblePages::Capacity);
    }
    for (ImageContent &page : pages) {
        page.initialize();
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
        reloadCurrentPage(true);
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

QString PageManager::currentPageNumAsString() const
{
    VolumeManager *volume = activeVolume();
    if (!volume || volume->size() == 0 || m_pages.isEmpty()) {
        return "";
    }
    if (m_pages.size() == 2) {
        return QString("(%1-%2/%3)").arg(m_currentPage + 1).arg(m_currentPage + 2).arg(volume->size());
    } else {
        return QString("(%1/%3)").arg(m_currentPage + 1).arg(volume->size());
    }
}

QString PageManager::currentPageStatusAsString() const
{
    // StatusBar
    QString pagestr = currentPageNumAsString();
    QString status;
    switch (m_pages.size()) {
    case 1:
        status = QString("%1 %2[%3x%4]")
                     .arg(m_pages[0].Path)
                     .arg(pagestr)
                     .arg(m_pages[0].BaseSize.width())
                     .arg(m_pages[0].BaseSize.height());
        break;
    case 2:
        status = QString("%1 %2[%3x%4] | %5 [%6x%7]")
                     .arg(m_pages[0].Path)
                     .arg(pagestr)
                     .arg(m_pages[0].BaseSize.width())
                     .arg(m_pages[0].BaseSize.height())
                     .arg(m_pages[1].Path)
                     .arg(m_pages[1].BaseSize.width())
                     .arg(m_pages[1].BaseSize.height());
        break;
    default:
        break;
    }
    return status;
}

QString PageManager::pageSignage(int page) const
{
    VolumeManager *volume = activeVolume();
    if (!volume || page < 0 || m_pages.size() <= page) {
        return "";
    }
    return QString("%1 (%2/%3)")
        .arg(QDir::toNativeSeparators(volume->getPathByFileName(m_pages[page].Path)))
        .arg(m_currentPage + 1 + page)
        .arg(volume->size());
}
bool PageManager::canDualView() const
{
    QVApplication *myapp = qApp;
    return qApp->DualView() && !(m_wideImage && myapp->WideImageAsOnePageInDualView());
}

QStringList PageManager::enumVolumes(QDir dir)
{
    QStringList folders, archives;
    folders = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::Name);
    IFileLoader::sortFiles(folders);
    archives = dir.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::Name);
    IFileLoader::sortFiles(archives);
    return folders + archives;
}
