#include <QtTest>

#include <type_traits>

#include "imageview.h"
#include "models/cursorscrollmapping.h"
#include "models/imagestring.h"
#include "models/loupecontroller.h"
#include "models/viewersession.h"
#include "models/qvapplication.h"
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
        ViewerSession manager(nullptr);

        QCOMPARE(manager.stateKind(), ViewerStateKind::Empty);
        QVERIFY(!manager.advanceSpread());
        QVERIFY(!manager.retreatSpread());
        QVERIFY(!manager.fastForwardPage());
        QVERIFY(!manager.fastBackwardPage());
        QVERIFY(!manager.firstPage());
        QVERIFY(!manager.lastPage());
        QVERIFY(!manager.advanceOnePage());
        QVERIFY(!manager.retreatOnePage());
        QVERIFY(!manager.nextVolume());
        QVERIFY(!manager.prevVolume());
        QVERIFY(!manager.reloadVisiblePages());
        QCOMPARE(manager.currentPagePath(), QString());
        QCOMPARE(manager.currentPageName(), QString());
        QCOMPARE(manager.currentPageNumberText(), QString());
        QCOMPARE(manager.currentPageStatusText(), QString());
        QCOMPARE(manager.pageSignage(0), QString());
        QCOMPARE(manager.pageSignage(-1), QString());
    }

    void directImageTransitionsThroughStandalonePreview()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath("preview.bmp");
        QImage image(640, 480, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));

        ViewerSession manager(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&manager);

        QVERIFY(manager.loadVolumeWithFile(imagePath));
        QCOMPARE(manager.stateKind(), ViewerStateKind::Loading);
        QVERIFY(manager.initialImagePaintPending());

        QTRY_COMPARE(manager.stateKind(), ViewerStateKind::StandalonePreview);
        QCOMPARE(QFileInfo(manager.currentPageName()).fileName(),
                 QString("preview.bmp"));
        QVERIFY(manager.initialImagePaintPending());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);

        // Navigation while the parent folder is not ready must remain a no-op.
        QVERIFY(!manager.advanceSpread());
        QVERIFY(!manager.retreatSpread());
        QVERIFY(!manager.nextVolume());
        QVERIFY(!manager.prevVolume());

        manager.notifyInitialImagePainted();
        QTRY_COMPARE(manager.stateKind(), ViewerStateKind::VolumeReady);
        QVERIFY(!manager.initialImagePaintPending());
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
            ViewerSession manager(nullptr);
            QVERIFY(manager.loadVolume(directory.path()));
            QVERIFY(manager.selectPage(2));
            QCOMPARE(manager.currentPageIndex(), 2);
        }

        const QString volumePath = QDir::fromNativeSeparators(directory.path());
        QVERIFY(qApp->readProgressStore()->contains(volumePath));
        const ReadProgress progress = qApp->readProgressStore()->at(volumePath);
        QCOMPARE(progress.resumePageIndex, 2);
        QCOMPARE(progress.currentPageName, QString("page-2.bmp"));

        qApp->setOpenVolumeWithProgress(true);
        ViewerSession restoredManager(nullptr);
        QVERIFY(restoredManager.loadVolume(directory.path()));
        QCOMPARE(restoredManager.currentPageIndex(), 2);
        QCOMPARE(restoredManager.currentPageName(), QString("page-2.bmp"));
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
        ViewerSession singleViewerSession(nullptr);
        QVERIFY(singleViewerSession.loadVolume(directory.path()));
        QCOMPARE(singleViewerSession.currentPageIndex(), 0);
        QCOMPARE(singleViewerSession.visiblePageCount(), 1);
        QVERIFY(singleViewerSession.advanceSpread());
        QCOMPARE(singleViewerSession.currentPageIndex(), 1);
        QVERIFY(singleViewerSession.advanceOnePage());
        QCOMPARE(singleViewerSession.currentPageIndex(), 2);
        QVERIFY(singleViewerSession.retreatOnePage());
        QCOMPARE(singleViewerSession.currentPageIndex(), 1);
        QVERIFY(singleViewerSession.lastPage());
        QCOMPARE(singleViewerSession.currentPageIndex(), 5);
        QVERIFY(singleViewerSession.firstPage());
        QCOMPARE(singleViewerSession.currentPageIndex(), 0);

        qApp->setDualView(true);
        qApp->setFirstImageAsOnePageInDualView(false);
        qApp->setWideImageAsOnePageInDualView(true);
        ViewerSession spreadManager(nullptr);
        QVERIFY(spreadManager.loadVolume(directory.path()));
        QCOMPARE(spreadManager.currentPageIndex(), 0);
        QCOMPARE(spreadManager.visiblePageCount(), 2);
        QVERIFY(spreadManager.advanceSpread());
        QCOMPARE(spreadManager.currentPageIndex(), 2);
        QCOMPARE(spreadManager.visiblePageCount(), 2);
        QVERIFY(spreadManager.advanceOnePage());
        QCOMPARE(spreadManager.currentPageIndex(), 3);
        QCOMPARE(spreadManager.visiblePageCount(), 2);
        QVERIFY(spreadManager.retreatSpread());
        QCOMPARE(spreadManager.currentPageIndex(), 1);
        QCOMPARE(spreadManager.visiblePageCount(), 2);
    }

    void visiblePagesAreReadOnlySnapshots()
    {
        ViewerSession manager(nullptr);
        int notificationCount = 0;
        VisiblePages latest;
        connect(&manager, &ViewerSession::visiblePagesChanged, this, [&](VisiblePages pages) {
            ++notificationCount;
            latest = std::move(pages);
        });
        QVERIFY(manager.addVisiblePage(ImageContent("first.bmp", 0), true));

        const VisiblePages pages = manager.visiblePages();
        QCOMPARE(notificationCount, 1);
        QCOMPARE(latest.count(), 1);
        QCOMPARE(pages.count(), 1);
        QVERIFY(pages.at(-1) == nullptr);
        QVERIFY(pages.at(1) == nullptr);
        QVERIFY(pages.first() != nullptr);
        QCOMPARE(pages.first()->path, QString("first.bmp"));

        QVERIFY(manager.addVisiblePage(ImageContent("second.bmp", 0), true));
        QCOMPARE(notificationCount, 2);
        QCOMPARE(latest.count(), 2);
        QVERIFY(!manager.addVisiblePage(ImageContent("third.bmp", 0), true));
        QCOMPARE(notificationCount, 2);

        manager.clearVisiblePages();
        QCOMPARE(notificationCount, 3);
        QVERIFY(latest.isEmpty());
        QVERIFY(manager.visiblePages().isEmpty());
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first()->path, QString("first.bmp"));
    }

    void imageStringUsesValueSnapshots()
    {
        ImageString imageString;
        QCOMPARE(imageString.formatString("%p"), QString());

        ViewerSession manager(nullptr);
        QImage image(100, 200, QImage::Format_RGB32);
        QVERIFY(manager.addVisiblePage(
            ImageContent(image, "sample.png", image.size(), {}, 1024),
            true));
        imageString.initialize(&manager, [] {
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
        QSignalSpy enumerationSpy(&volume, &Volume::enumerationFinished);

        QCOMPARE(volume.currentPath(), QString());
        QCOMPARE(volume.currentPathWithSeparator(), QString());
        QCOMPARE(volume.pageNameAt(0), QString());
        QVERIFY(volume.currentImage().loadedImage.isNull());
        QVERIFY(volume.pageAt(0).loadedImage.isNull());
        QVERIFY(!volume.advanceOnePage());
        QVERIFY(!volume.retreatOnePage());
        QVERIFY(!volume.selectPage(0));
        QVERIFY(!volume.findImageByIndex(0));
        QVERIFY(!volume.selectPageByName("missing.png"));
        volume.handleEnumerationFinished();
        QCOMPARE(enumerationSpy.count(), 1);
        volume.moveToThread(nullptr);
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
        auto readyFuture = [](VolumeHandle handle) {
            QPromise<VolumeHandle> promise;
            promise.start();
            QFuture<VolumeHandle> future = promise.future();
            promise.addResult(std::move(handle));
            promise.finish();
            return future;
        };

        auto *volume = new Volume(
            nullptr, new EmptyFileLoader);
        bool destroyed = false;
        QObject::connect(volume, &QObject::destroyed, this, [&destroyed] { destroyed = true; });

        VolumeHandle active = makeVolumeHandle(volume);
        VolumeLoadCache cache(1);
        QString first = "first";
        QString second = "second";
        cache.insert(first, readyFuture(active));
        cache.insert(second, readyFuture({}));

        QVERIFY(!destroyed);
        active.reset();
        QTRY_VERIFY(destroyed);
    }

    void emptyImageViewNavigationIsSafe()
    {
        ViewerSession manager(nullptr);
        ImageView view;
        view.setViewerSession(&manager);

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
        ViewerSession manager(nullptr);
        ImageView view;
        view.setViewerSession(&manager);

        const QImage image(8, 8, QImage::Format_ARGB32);
        manager.addVisiblePage(ImageContent(image, "preview.png", image.size(), {}, 0), true);

        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();
        QCOMPARE(manager.currentPageName(), QString("preview.png"));
    }

    void renderedPagesOwnItemsAndExposeSnapshots()
    {
        static_assert(!std::is_copy_constructible_v<PageItem>);
        static_assert(!std::is_copy_assignable_v<PageItem>);

        ViewerSession manager(nullptr);
        ImageView view;
        view.setViewerSession(&manager);

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
        ViewerSession manager(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&manager);

        QImage image(640, 480, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(manager.addVisiblePage(
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
        ViewerSession manager(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setViewerSession(&manager);

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
        QVERIFY(manager.addVisiblePage(
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

        ViewerSession manager(nullptr);
        ImageView view;
        view.setViewerSession(&manager);

        QVERIFY(!manager.loadVolume(directory.path()));
        QCOMPARE(manager.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(manager.pageCount(), 0);
        QVERIFY(!manager.firstPage());
        QVERIFY(!manager.lastPage());
        view.handleNextPageOrVolumeActionTriggered();
        view.handlePrevPageOrVolumeActionTriggered();

        manager.reset();
        QCOMPARE(manager.stateKind(), ViewerStateKind::Empty);
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

        ViewerSession manager(nullptr);
        ImageView view;
        view.setViewerSession(&manager);

        QVERIFY(!manager.loadVolume(archivePath));
        QCOMPARE(manager.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(manager.pageCount(), 0);
        QVERIFY(!manager.isArchive());
        QVERIFY(!manager.firstPage());
        QVERIFY(!manager.lastPage());
        QVERIFY(!manager.reloadVisiblePages());
        QCOMPARE(manager.currentPagePath(), QString());
        QCOMPARE(manager.currentPageName(), QString());
        QCOMPARE(manager.pageSignage(0), QString());
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
