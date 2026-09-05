#include <QtTest>

#include <type_traits>

#include "imageview.h"
#include "models/cursorscrollmapping.h"
#include "models/imagestring.h"
#include "models/loupecontroller.h"
#include "models/viewersession.h"
#include "models/qvapplication.h"
#include "models/volumecache.h"
#include "models/volumehandle.h"

class EmptyFileLoader final : public IFileLoader
{
public:
    explicit EmptyFileLoader(QObject *parent = nullptr)
        : IFileLoader(parent)
    {}

    QString volumePath() override { return "empty"; }
    QString realVolumePath() override { return "empty"; }
    bool isArchive() override { return false; }
    bool isValid() override { return true; }
    bool hasSubDirectories() override { return false; }
    QStringList contents() override { return {}; }
    QStringList subArchives() override { return {}; }
    QByteArray getFile(QString, QMutex &) override { return {}; }
    InflateCacheMode getCacheMode() override { return InflateNoCached; }
};

class MemoryFileLoader final : public IFileLoader
{
public:
    explicit MemoryFileLoader(int imageCount, QObject *parent = nullptr)
        : IFileLoader(parent)
    {
        for (int imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
            const QString name = QString("page-%1.bmp").arg(imageIndex);
            QImage image(16 + imageIndex, 24 + imageIndex, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(imageIndex * 60, 255, 255));
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "BMP");
            m_names.append(name);
            m_images.insert(name, bytes);
        }
    }

    QString volumePath() override { return "memory"; }
    QString realVolumePath() override { return "memory"; }
    bool isArchive() override { return false; }
    bool isValid() override { return true; }
    bool hasSubDirectories() override { return false; }
    QStringList contents() override { return m_names; }
    QStringList subArchives() override { return {}; }
    QByteArray getFile(QString name, QMutex &mutex) override
    {
        QMutexLocker locker(&mutex);
        m_requestedNames.append(name);
        return m_images.value(name);
    }
    InflateCacheMode getCacheMode() override { return InflateNoCached; }

    QStringList requestedNames() const { return m_requestedNames; }

private:
    QStringList m_names;
    QHash<QString, QByteArray> m_images;
    QStringList m_requestedNames;
};

class ViewerNavigationTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        qApp->setSeparatePagesWhenWideImage(true);
        qApp->setDualView(false);
        qApp->setFitting(true);
    }

    void emptyViewerSessionOperationsAreSafe()
    {
        ViewerSession session(nullptr);

        QCOMPARE(session.stateKind(), ViewerStateKind::Empty);
        QVERIFY(!session.advanceSpread());
        QVERIFY(!session.retreatSpread());
        QVERIFY(!session.fastForwardPage());
        QVERIFY(!session.fastBackwardPage());
        QVERIFY(!session.firstPage());
        QVERIFY(!session.lastPage());
        QVERIFY(!session.advanceOnePage());
        QVERIFY(!session.retreatOnePage());
        QVERIFY(!session.nextVolume());
        QVERIFY(!session.prevVolume());
        QVERIFY(!session.reloadVisiblePages());
        QCOMPARE(session.currentPagePath(), QString());
        QCOMPARE(session.currentPageName(), QString());
        QCOMPARE(session.currentPageNumberText(), QString());
        QCOMPARE(session.currentPageStatusText(), QString());
        QCOMPARE(session.pageSignage(0), QString());
        QCOMPARE(session.pageSignage(-1), QString());
    }

    void directImageTransitionsThroughStandalonePreview()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath("preview.bmp");
        QImage image(640, 480, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));

        ViewerSession session(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&session);

        QVERIFY(session.loadVolumeWithFile(imagePath));
        QCOMPARE(session.stateKind(), ViewerStateKind::Loading);
        QVERIFY(session.initialImagePaintPending());

        QTRY_COMPARE(session.stateKind(), ViewerStateKind::StandalonePreview);
        QCOMPARE(QFileInfo(session.currentPageName()).fileName(),
                 QString("preview.bmp"));
        QVERIFY(session.initialImagePaintPending());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);

        // Navigation while the parent folder is not ready must remain a no-op.
        QVERIFY(!session.advanceSpread());
        QVERIFY(!session.retreatSpread());
        QVERIFY(!session.nextVolume());
        QVERIFY(!session.prevVolume());

        session.notifyInitialImagePainted();
        QTRY_COMPARE(session.stateKind(), ViewerStateKind::VolumeReady);
        QVERIFY(!session.initialImagePaintPending());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void selectedPageIsRestoredAsReadProgress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 4; ++page) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 60, 255, 255));
            QVERIFY(image.save(directory.filePath(
                QString("page-%1.bmp").arg(page))));
        }

        qApp->setOpenVolumeWithProgress(false);
        {
            ViewerSession session(nullptr);
            QVERIFY(session.loadVolume(directory.path()));
            QVERIFY(session.selectPage(2));
            QCOMPARE(session.currentPageIndex(), 2);
        }

        const QString volumePath = QDir::fromNativeSeparators(directory.path());
        QVERIFY(qApp->readProgressStore()->contains(volumePath));
        const ReadProgress progress = qApp->readProgressStore()->at(volumePath);
        QCOMPARE(progress.resumePageIndex, 2);
        QCOMPARE(progress.currentPageName, QString("page-2.bmp"));

        qApp->setOpenVolumeWithProgress(true);
        ViewerSession restoredSession(nullptr);
        QVERIFY(restoredSession.loadVolume(directory.path()));
        QCOMPARE(restoredSession.currentPageIndex(), 2);
        QCOMPARE(restoredSession.currentPageName(), QString("page-2.bmp"));
    }

    void readProgressKeepsLegacyIniKeys()
    {
        const QString volumePath = "read-progress-key-compatibility";
        const ReadProgress progress = {
            "Compatibility title",
            volumePath,
            "page-7.bmp",
            12,
            7,
            false};
        qApp->readProgressStore()->insert(volumePath, progress);
        qApp->readProgressStore()->save();

        QSettings settings(
            qApp->getFilePathOfApplicationSetting(PROGRESS_INI),
            QSettings::IniFormat);
        bool found = false;
        for (const QString &group : settings.childGroups()) {
            settings.beginGroup(group);
            if (settings.value("Path").toString() == volumePath) {
                found = true;
                QCOMPARE(settings.value("Title").toString(), QString("Compatibility title"));
                QCOMPARE(settings.value("CurrenPage").toString(), QString("page-7.bmp"));
                QCOMPARE(settings.value("Pages").toInt(), 12);
                QCOMPARE(settings.value("Current").toInt(), 7);
                QCOMPARE(settings.value("Completed").toBool(), false);
                QVERIFY(!settings.contains("CurrentPage"));
            }
            settings.endGroup();
        }
        QVERIFY(found);
    }

    void pageAndSpreadNavigationKeepTheirExistingStepSizes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 6; ++page) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 40, 255, 255));
            QVERIFY(image.save(directory.filePath(
                QString("page-%1.bmp").arg(page))));
        }

        qApp->setOpenVolumeWithProgress(false);
        qApp->setDualView(false);
        ViewerSession singlePageSession(nullptr);
        QVERIFY(singlePageSession.loadVolume(directory.path()));
        QCOMPARE(singlePageSession.currentPageIndex(), 0);
        QCOMPARE(singlePageSession.visiblePageCount(), 1);
        QVERIFY(singlePageSession.advanceSpread());
        QCOMPARE(singlePageSession.currentPageIndex(), 1);
        QVERIFY(singlePageSession.advanceOnePage());
        QCOMPARE(singlePageSession.currentPageIndex(), 2);
        QVERIFY(singlePageSession.retreatOnePage());
        QCOMPARE(singlePageSession.currentPageIndex(), 1);
        QVERIFY(singlePageSession.lastPage());
        QCOMPARE(singlePageSession.currentPageIndex(), 5);
        QVERIFY(singlePageSession.firstPage());
        QCOMPARE(singlePageSession.currentPageIndex(), 0);

        qApp->setDualView(true);
        qApp->setFirstImageAsOnePageInDualView(false);
        qApp->setWideImageAsOnePageInDualView(true);
        ViewerSession spreadSession(nullptr);
        QVERIFY(spreadSession.loadVolume(directory.path()));
        QCOMPARE(spreadSession.currentPageIndex(), 0);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
        QVERIFY(spreadSession.advanceSpread());
        QCOMPARE(spreadSession.currentPageIndex(), 2);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
        QVERIFY(spreadSession.advanceOnePage());
        QCOMPARE(spreadSession.currentPageIndex(), 3);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
        QVERIFY(spreadSession.retreatSpread());
        QCOMPARE(spreadSession.currentPageIndex(), 1);
        QCOMPARE(spreadSession.visiblePageCount(), 2);
    }

    void visiblePagesAreReadOnlySnapshots()
    {
        ViewerSession session(nullptr);
        int notificationCount = 0;
        VisiblePages latest;
        connect(&session, &ViewerSession::visiblePagesChanged, this, [&](VisiblePages pages) {
            ++notificationCount;
            latest = std::move(pages);
        });
        QVERIFY(session.addVisiblePage(ImageContent("first.bmp", 0), true));

        const VisiblePages pages = session.visiblePages();
        QCOMPARE(notificationCount, 1);
        QCOMPARE(latest.count(), 1);
        QCOMPARE(pages.count(), 1);
        QVERIFY(pages.at(-1) == nullptr);
        QVERIFY(pages.at(1) == nullptr);
        QVERIFY(pages.first() != nullptr);
        QCOMPARE(pages.first()->path, QString("first.bmp"));

        QVERIFY(session.addVisiblePage(ImageContent("second.bmp", 0), true));
        QCOMPARE(notificationCount, 2);
        QCOMPARE(latest.count(), 2);
        QVERIFY(!session.addVisiblePage(ImageContent("third.bmp", 0), true));
        QCOMPARE(notificationCount, 2);

        session.clearVisiblePages();
        QCOMPARE(notificationCount, 3);
        QVERIFY(latest.isEmpty());
        QVERIFY(session.visiblePages().isEmpty());
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first()->path, QString("first.bmp"));
    }

    void imageStringUsesValueSnapshots()
    {
        ImageString imageString;
        QCOMPARE(imageString.formatString("%p"), QString());

        ViewerSession session(nullptr);
        QImage image(100, 200, QImage::Format_RGB32);
        QVERIFY(session.addVisiblePage(
            ImageContent(image, "sample.png", image.size(), {}, 1024),
            true));
        imageString.initialize(&session, [] {
            return RenderedPageMetrics(QVector<qreal>{0.5});
        });

        QCOMPARE(imageString.formatString("%p|%s|%m"),
                 QString("sample.png|100x200|50%"));
    }

    void pageItemUsesRenderSettingsSnapshot()
    {
        QGraphicsScene scene;
        QImage image(400, 400, QImage::Format_RGB32);
        image.fill(Qt::red);
        PageRenderSettings settings;
        settings.pixelRatio = 2.0;
        PageItem page(nullptr, &scene, ImageContent(image, "page.bmp", image.size(), {}, 0), settings);
        settings.pixelRatio = 3.0;

        page.setPageLayoutFitting(QRect(0, 0, 100, 100),
                                  PageItem::PageCenter,
                                  qvEnums::FitToRect,
                                  1.0);
        QCOMPARE(page.displayScale, 0.5);

        page.setRenderSettings(settings);
        page.setPageLayoutFitting(QRect(0, 0, 100, 100),
                                  PageItem::PageCenter,
                                  qvEnums::FitToRect,
                                  1.0);
        QCOMPARE(page.displayScale, 0.75);

        PageItem pageWithDefaults(nullptr, &scene, ImageContent(image, "preview.bmp", image.size(), {}, 0));
        pageWithDefaults.setPageLayoutFitting(QRect(0, 0, 100, 100),
                                              PageItem::PageCenter,
                                              qvEnums::FitToRect,
                                              1.0);
        QCOMPARE(pageWithDefaults.displayScale, 0.25);
    }

    void emptyVolumeOperationsAreSafe()
    {
        EmptyFileLoader loader;
        Volume volume(nullptr, &loader);
        QSignalSpy pageListLoadedSpy(&volume, &Volume::pageListLoaded);

        QCOMPARE(volume.currentPath(), QString());
        QCOMPARE(volume.currentPathWithSeparator(), QString());
        QCOMPARE(volume.pageNameAt(0), QString());
        QVERIFY(volume.currentImage().loadedImage.isNull());
        QVERIFY(!volume.imageLoadAt(0).isValid());
        QVERIFY(!volume.advanceOnePage());
        QVERIFY(!volume.retreatOnePage());
        QVERIFY(!volume.selectPage(0));
        QVERIFY(!volume.selectPageAndRefresh(0));
        QVERIFY(!volume.selectPageByName("missing.png"));
        volume.handlePageListLoaded();
        QCOMPARE(pageListLoadedSpy.count(), 1);
        volume.moveToThread(nullptr);
    }

    void volumeSeparatesCoverAndThumbnailImageLoading()
    {
        auto *coverLoader = new MemoryFileLoader(3);
        Volume coverVolume(nullptr, coverLoader);
        coverVolume.prefetchCoverImages();

        const Volume::ImageLoadFuture firstCoverLoad = coverVolume.imageLoadAt(0);
        const Volume::ImageLoadFuture secondCoverLoad = coverVolume.imageLoadAt(1);
        QVERIFY(firstCoverLoad.isValid());
        QVERIFY(secondCoverLoad.isValid());
        QVERIFY(!firstCoverLoad.result().loadedImage.isNull());
        QVERIFY(!secondCoverLoad.result().loadedImage.isNull());
        QVERIFY(!coverVolume.imageLoadAt(2).isValid());
        QStringList coverRequests = coverLoader->requestedNames();
        coverRequests.sort();
        QCOMPARE(coverRequests,
                 QStringList({"page-0.bmp", "page-1.bmp"}));

        auto *thumbnailLoader = new MemoryFileLoader(3);
        Volume thumbnailVolume(nullptr, thumbnailLoader);
        const ImageContent thumbnailSource = thumbnailVolume.loadThumbnailSourceImage();

        QCOMPARE(thumbnailSource.path, QString("page-0.bmp"));
        QCOMPARE(thumbnailSource.loadedImage.size(), QSize(16, 24));
        QVERIFY(thumbnailSource.resizedImage.isNull());
        QCOMPARE(thumbnailLoader->requestedNames(),
                 QStringList({"page-0.bmp"}));
    }

    void volumeHandleDestroysOnOwnerThread()
    {
        auto *loader = new EmptyFileLoader;
        auto *volume = new Volume(nullptr, loader);
        QThread *destructionThread = nullptr;
        QObject::connect(volume, &QObject::destroyed, this, [&destructionThread] { destructionThread = QThread::currentThread(); }, Qt::DirectConnection);

        VolumeHandle handle = makeVolumeHandle(volume);
        QFuture<void> release = QtConcurrent::run(
            [workerHandle = std::move(handle)]() mutable {
                workerHandle.reset();
            });
        release.waitForFinished();

        QTRY_COMPARE(destructionThread, QThread::currentThread());
    }

    void activeVolumeSurvivesCacheEviction()
    {
        auto *volume = new Volume(
            nullptr, new EmptyFileLoader);
        bool destroyed = false;
        QObject::connect(volume, &QObject::destroyed, this, [&destroyed] { destroyed = true; });

        VolumeHandle active = makeVolumeHandle(volume);
        VolumeCache cache(1);
        const VolumeCacheKey first{"first", false, false};
        const VolumeCacheKey second{"second", false, false};
        cache.insertReady(first, active);
        cache.insertReady(second, {});

        QVERIFY(!destroyed);
        active.reset();
        QTRY_VERIFY(destroyed);
    }

    void volumeCacheSharesLoadsAndDoesNotWaitWhenLookingUp()
    {
        VolumeCache cache(2);
        const VolumeCacheKey key{"shared", false, false};
        QPromise<VolumeHandle> pendingLoad;
        pendingLoad.start();
        int startCount = 0;
        const auto startLoad = [&] {
            ++startCount;
            return pendingLoad.future();
        };

        const VolumeLoadFuture firstRequest = cache.request(key, startLoad);
        const VolumeLoadFuture secondRequest = cache.request(key, startLoad);

        QCOMPARE(startCount, 1);
        QVERIFY(firstRequest.isValid());
        QVERIFY(secondRequest.isValid());
        QVERIFY(!cache.findReady(key));

        VolumeHandle loadedVolume = makeVolumeHandle(
            new Volume(nullptr, new EmptyFileLoader));
        pendingLoad.addResult(loadedVolume);
        pendingLoad.finish();

        QCOMPARE(cache.findReady(key), loadedVolume);
        QVERIFY(cache.markUsed(key));
    }

    void volumeCacheRetriesFailedLoads()
    {
        VolumeCache cache(1);
        const VolumeCacheKey key{"retry", false, false};
        int startCount = 0;
        const auto failedLoad = [&] {
            ++startCount;
            QPromise<VolumeHandle> promise;
            promise.start();
            VolumeLoadFuture future = promise.future();
            promise.addResult({});
            promise.finish();
            return future;
        };

        cache.request(key, failedLoad);
        QVERIFY(!cache.findReady(key));
        QVERIFY(!cache.contains(key));
        cache.request(key, failedLoad);
        QCOMPARE(startCount, 2);
    }

    void emptyImageViewNavigationIsSafe()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        view.handleNextPageActionTriggered();
        view.handlePrevPageActionTriggered();
        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();
        view.handleFastForwardActionTriggered();
        view.handleFastBackwardActionTriggered();
        view.handleFirstPageActionTriggered();
        view.handleLastPageActionTriggered();
        view.handleNextOnePageActionTriggered();
        view.handlePrevOnePageActionTriggered();
        view.handleNextVolumeActionTriggered();
        view.handlePrevVolumeActionTriggered();
        view.handleRotateActionTriggered();
        view.handleSlideShowTimerTimeout();
        view.handleCopyPageActionTriggered();
        view.handleCopyFileActionTriggered();
    }

    void gestureStateIsIndependentBetweenViews()
    {
        ImageView firstView;
        ImageView secondView;

        firstView.updateGestureTransform(2.0, 0.0);
        secondView.updateGestureTransform(3.0, 0.0);
        firstView.commitGestureTransform();
        firstView.updateGestureTransform(1.0, 0.0);

        QCOMPARE(firstView.transform().m11(), 2.0);
        QCOMPARE(secondView.transform().m11(), 3.0);

        firstView.resetGestureTransform();
        QVERIFY(firstView.transform().isIdentity());
        QCOMPARE(secondView.transform().m11(), 3.0);
    }

    void cursorZoomMappingUsesBothScrollBarRanges()
    {
        const QSize viewportSize(400, 200);
        const QPoint minimum(10, 20);
        const QPoint maximum(110, 220);

        QCOMPARE(CursorScrollMapping::zoomScrollPosition(
                     QPoint(100, 50), viewportSize, minimum, maximum),
                 minimum);
        QCOMPARE(CursorScrollMapping::zoomScrollPosition(
                     QPoint(200, 100), viewportSize, minimum, maximum),
                 QPoint(60, 120));
        QCOMPARE(CursorScrollMapping::zoomScrollPosition(
                     QPoint(300, 150), viewportSize, minimum, maximum),
                 maximum);
    }

    void cursorLoupeMappingKeepsAnchorStable()
    {
        const std::optional<QPoint> position = CursorScrollMapping::loupeScrollPosition(
            QPoint(200, 150), QPoint(200, 150), QSize(400, 300), QRect(0, 0, 400, 300), QRectF(0, 0, 800, 600), QPoint());

        QVERIFY(position);
        QCOMPARE(*position, QPoint(200, 150));
        QVERIFY(!CursorScrollMapping::loupeScrollPosition(
            QPoint(200, 150), QPoint(0, 150), QSize(400, 300), QRect(0, 0, 400, 300), QRectF(0, 0, 800, 600), QPoint()));
    }

    void loupeControllerTracksActivationAndRestoration()
    {
        LoupeController loupe;
        const QRect contentRect(0, 0, 400, 300);
        const QPoint initialScrollPosition(25, 40);

        QVERIFY(!loupe.isActive());
        LoupeController::SceneUpdate update = loupe.prepareSceneUpdate(contentRect, initialScrollPosition);
        QVERIFY(!update.leavingLoupe);

        loupe.activate();
        QVERIFY(loupe.isActive());
        update = loupe.prepareSceneUpdate(QRect(0, 0, 800, 600), initialScrollPosition);
        QVERIFY(!update.leavingLoupe);
        QCOMPARE(update.scrollPositionToRestore, initialScrollPosition);

        loupe.setAnchorPosition(QPoint(200, 150));
        const std::optional<QPoint> mappedPosition = loupe.scrollPositionForCursor(
            QPoint(200, 150), QSize(400, 300), QRectF(0, 0, 800, 600));
        QVERIFY(mappedPosition);
        QCOMPARE(*mappedPosition, QPoint(250, 230));

        loupe.deactivate();
        QVERIFY(!loupe.isActive());
        update = loupe.prepareSceneUpdate(contentRect, QPoint(100, 120));
        QVERIFY(update.leavingLoupe);
        QCOMPARE(update.scrollPositionToRestore, initialScrollPosition);

        update = loupe.prepareSceneUpdate(contentRect, QPoint(100, 120));
        QVERIFY(!update.leavingLoupe);
    }

    void loupeControllerAdjustsScaleWithinLowerBound()
    {
        LoupeController loupe;

        QCOMPARE(loupe.scaleFactor(), 3.0);
        loupe.adjustScaleFromWheel(-120);
        loupe.adjustScaleFromWheel(-120);
        loupe.adjustScaleFromWheel(-120);
        loupe.adjustScaleFromWheel(-120);
        QCOMPARE(loupe.scaleFactor(), 1.5);

        loupe.adjustScaleFromWheel(120);
        QCOMPARE(loupe.scaleFactor(), 2.0);
        loupe.adjustScaleFromWheel(0);
        QCOMPARE(loupe.scaleFactor(), 2.0);
    }

    void loupeControllersKeepIndependentState()
    {
        LoupeController first;
        LoupeController second;

        first.activate();
        first.adjustScaleFromWheel(120);

        QVERIFY(first.isActive());
        QCOMPARE(first.scaleFactor(), 3.5);
        QVERIFY(!second.isActive());
        QCOMPARE(second.scaleFactor(), 3.0);
    }

    void standalonePreviewNavigationIsSafe()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        const QImage image(8, 8, QImage::Format_ARGB32);
        session.addVisiblePage(ImageContent(image, "preview.png", image.size(), {}, 0), true);

        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();
        QCOMPARE(session.currentPageName(), QString("preview.png"));
    }

    void renderedPagesOwnItemsAndExposeSnapshots()
    {
        static_assert(!std::is_copy_constructible_v<PageItem>);
        static_assert(!std::is_copy_assignable_v<PageItem>);

        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        const QImage image(8, 4, QImage::Format_ARGB32);
        QCOMPARE(view.addRenderedPage(
                     ImageContent(image, "first.png", image.size(), {}, 0), true),
                 ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 1);
        QCOMPARE(view.renderedPageContents().count(), 1);

        QCOMPARE(view.addRenderedPage(
                     ImageContent(image, "second.png", image.size(), {}, 0), true),
                 ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 2);
        VisiblePages contents = view.renderedPageContents();
        QCOMPARE(contents.at(0)->path, QString("first.png"));
        QCOMPARE(contents.at(1)->path, QString("second.png"));
        QCOMPARE(view.addRenderedPage(
                     ImageContent(image, "third.png", image.size(), {}, 0), true),
                 ImageView::AddRenderedPageResult::Rejected);

        view.clearRenderedPages();
        QCOMPARE(view.renderedPageCount(), 0);
        QVERIFY(view.renderedPageContents().isEmpty());

        QCOMPARE(view.addRenderedPage(
                     ImageContent(image, "replacement.png", image.size(), {}, 0), false),
                 ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 1);
        QCOMPARE(view.addRenderedPage(
                     ImageContent(image, "prepended.png", image.size(), {}, 0), false),
                 ImageView::AddRenderedPageResult::AddedLandscape);
        QCOMPARE(view.renderedPageCount(), 2);
        contents = view.renderedPageContents();
        QCOMPARE(contents.at(0)->path, QString("prepended.png"));
        QCOMPARE(contents.at(1)->path, QString("replacement.png"));
        QVERIFY(!contents.at(-1));
        QVERIFY(!contents.at(2));
    }

    void fittingModeRelayoutsRenderedPage()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&session);

        QImage image(640, 480, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(session.addVisiblePage(
            ImageContent(image, "fitting.png", image.size(), {}, 0),
            true));

        qApp->setFitting(false);
        view.refreshRenderedPages();
        QCOMPARE(view.renderedPageMetrics().notationalScaleAt(0), 1.0);

        view.handleFittingActionTriggered(true);
        QVERIFY(qApp->Fitting());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void fittingShortcutTriggersViewAction()
    {
        ViewerSession session(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&session);

        QAction fittingAction;
        fittingAction.setCheckable(true);
        connect(&fittingAction, &QAction::triggered, &view, &ImageView::handleFittingActionTriggered);
        QAction *previousAction =
            qApp->keyActions().actions().value("actionFitting", nullptr);
        auto restoreAction = qScopeGuard([previousAction]() {
            qApp->keyActions().actions()["actionFitting"] = previousAction;
        });
        QAction *fittingActionPtr = &fittingAction;
        qApp->keyActions().registerAction(
            "actionFitting", fittingActionPtr, "Image");

        QImage image(640, 480, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(session.addVisiblePage(
            ImageContent(image, "shortcut.png", image.size(), {}, 0),
            true));

        qApp->setFitting(false);
        fittingAction.setChecked(false);
        view.refreshRenderedPages();
        QCOMPARE(view.renderedPageMetrics().notationalScaleAt(0), 1.0);

        QKeySequence fittingKey("M");
        QAction *mappedAction = qApp->keyActions().getActionByKey(fittingKey);
        QCOMPARE(mappedAction, &fittingAction);
        mappedAction->trigger();

        QVERIFY(fittingAction.isChecked());
        QVERIFY(qApp->Fitting());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void emptyDirectoryNavigationIsSafe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        QVERIFY(!session.loadVolume(directory.path()));
        QCOMPARE(session.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(session.pageCount(), 0);
        QVERIFY(!session.firstPage());
        QVERIFY(!session.lastPage());
        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();

        session.reset();
        QCOMPARE(session.stateKind(), ViewerStateKind::Empty);
    }

    void emptyArchiveNavigationIsSafe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString archivePath = directory.filePath("empty.zip");
        QFile archive(archivePath);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        // Empty ZIP end-of-central-directory record.
        QCOMPARE(archive.write(QByteArray::fromHex(
                     "504b0506000000000000000000000000000000000000")),
                 qint64(22));
        archive.close();

        ViewerSession session(nullptr);
        ImageView view;
        view.setViewerSession(&session);

        QVERIFY(!session.loadVolume(archivePath));
        QCOMPARE(session.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(session.pageCount(), 0);
        QVERIFY(!session.isArchive());
        QVERIFY(!session.firstPage());
        QVERIFY(!session.lastPage());
        QVERIFY(!session.reloadVisiblePages());
        QCOMPARE(session.currentPagePath(), QString());
        QCOMPARE(session.currentPageName(), QString());
        QCOMPARE(session.pageSignage(0), QString());
        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QVApplication application(argc, argv);
    ViewerNavigationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_viewernavigationtest.moc"
