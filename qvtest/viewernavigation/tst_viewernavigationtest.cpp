#include <QtTest>

#include <type_traits>

#include "imageview.h"
#include "models/imagestring.h"
#include "models/pagemanager.h"
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

class StubPageRenderContext final : public PageRenderContext
{
public:
    qreal currentPixelRatio() const override
    {
        ++pixelRatioRequests;
        return 2.0;
    }

    ImageRetouch retouchParameters() const override { return {}; }

    mutable int pixelRatioRequests = 0;
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

    void emptyPageManagerOperationsAreSafe()
    {
        PageManager manager(nullptr);

        QCOMPARE(manager.stateKind(), ViewerStateKind::Empty);
        QVERIFY(!manager.nextPage());
        QVERIFY(!manager.prevPage());
        QVERIFY(!manager.fastForwardPage());
        QVERIFY(!manager.fastBackwardPage());
        QVERIFY(!manager.firstPage());
        QVERIFY(!manager.lastPage());
        QVERIFY(!manager.nextOnlyOnePage());
        QVERIFY(!manager.prevOnlyOnePage());
        QVERIFY(!manager.nextVolume());
        QVERIFY(!manager.prevVolume());
        QVERIFY(!manager.reloadCurrentPage());
        QCOMPARE(manager.currentPagePath(), QString());
        QCOMPARE(manager.currentPageName(), QString());
        QCOMPARE(manager.currentPageNumAsString(), QString());
        QCOMPARE(manager.currentPageStatusAsString(), QString());
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

        PageManager manager(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setPageManager(&manager);

        QVERIFY(manager.loadVolumeWithFile(imagePath));
        QCOMPARE(manager.stateKind(), ViewerStateKind::Loading);
        QVERIFY(manager.initialImagePaintPending());

        QTRY_COMPARE(manager.stateKind(), ViewerStateKind::StandalonePreview);
        QCOMPARE(QFileInfo(manager.currentPageName()).fileName(),
                 QString("preview.bmp"));
        QVERIFY(manager.initialImagePaintPending());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);

        // Navigation while the parent folder is not ready must remain a no-op.
        QVERIFY(!manager.nextPage());
        QVERIFY(!manager.prevPage());
        QVERIFY(!manager.nextVolume());
        QVERIFY(!manager.prevVolume());

        manager.notifyInitialImagePainted();
        QTRY_COMPARE(manager.stateKind(), ViewerStateKind::VolumeReady);
        QVERIFY(!manager.initialImagePaintPending());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void visiblePagesAreReadOnlySnapshots()
    {
        PageManager manager(nullptr);
        int notificationCount = 0;
        VisiblePages latest;
        connect(&manager, &PageManager::visiblePagesChanged, this, [&](VisiblePages pages) {
            ++notificationCount;
            latest = std::move(pages);
        });
        QVERIFY(manager.addNewPage(ImageContent("first.bmp", 0), true));

        const VisiblePages pages = manager.visiblePages();
        QCOMPARE(notificationCount, 1);
        QCOMPARE(latest.count(), 1);
        QCOMPARE(pages.count(), 1);
        QVERIFY(pages.at(-1) == nullptr);
        QVERIFY(pages.at(1) == nullptr);
        QVERIFY(pages.first() != nullptr);
        QCOMPARE(pages.first()->Path, QString("first.bmp"));

        QVERIFY(manager.addNewPage(ImageContent("second.bmp", 0), true));
        QCOMPARE(notificationCount, 2);
        QCOMPARE(latest.count(), 2);
        QVERIFY(!manager.addNewPage(ImageContent("third.bmp", 0), true));
        QCOMPARE(notificationCount, 2);

        manager.clearPages();
        QCOMPARE(notificationCount, 3);
        QVERIFY(latest.isEmpty());
        QVERIFY(manager.visiblePages().isEmpty());
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first()->Path, QString("first.bmp"));
    }

    void imageStringUsesValueSnapshots()
    {
        ImageString imageString;
        QCOMPARE(imageString.formatString("%p"), QString());

        PageManager manager(nullptr);
        QImage image(100, 200, QImage::Format_RGB32);
        QVERIFY(manager.addNewPage(
            ImageContent(image, "sample.png", image.size(), {}, 1024),
            true));
        imageString.initialize(&manager, [] {
            return RenderedPageMetrics(QVector<qreal>{0.5});
        });

        QCOMPARE(imageString.formatString("%p|%s|%m"),
                 QString("sample.png|100x200|50%"));
    }

    void pageItemUsesOnlyItsRenderContext()
    {
        QGraphicsScene scene;
        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::red);
        StubPageRenderContext context;
        PageItem page(nullptr, &scene, ImageContent(image, "page.bmp", image.size(), {}, 0), &context);

        page.setPageLayoutFitting(QRect(0, 0, 100, 100),
                                  PageItem::PageCenter,
                                  qvEnums::FitToRect,
                                  1.0);
        QCOMPARE(context.pixelRatioRequests, 1);

        PageItem pageWithoutContext(nullptr, &scene, ImageContent(image, "preview.bmp", image.size(), {}, 0));
        pageWithoutContext.setPageLayoutFitting(QRect(0, 0, 100, 100),
                                                PageItem::PageCenter,
                                                qvEnums::FitToRect,
                                                1.0);
    }

    void emptyVolumeManagerOperationsAreSafe()
    {
        EmptyFileLoader loader;
        VolumeManager volume(nullptr, &loader);
        QSignalSpy enumerationSpy(&volume, &VolumeManager::enumerationFinished);

        QCOMPARE(volume.currentPath(), QString());
        QCOMPARE(volume.currentPathWithSeparator(), QString());
        QCOMPARE(volume.getIndexedFileName(0), QString());
        QVERIFY(volume.currentImage().Image.isNull());
        QVERIFY(volume.getIndexedImageContent(0).Image.isNull());
        QVERIFY(!volume.nextPage());
        QVERIFY(!volume.prevPage());
        QVERIFY(!volume.findPageByIndex(0));
        QVERIFY(!volume.findImageByIndex(0));
        QVERIFY(!volume.findImageByName("missing.png"));
        volume.on_enmumerated();
        QCOMPARE(enumerationSpy.count(), 1);
        volume.moveToThread(nullptr);
    }

    void volumeHandleDestroysOnOwnerThread()
    {
        auto *loader = new EmptyFileLoader;
        auto *volume = new VolumeManager(nullptr, loader);
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

        auto *volume = new VolumeManager(
            nullptr, new EmptyFileLoader);
        bool destroyed = false;
        QObject::connect(volume, &QObject::destroyed, this, [&destroyed] { destroyed = true; });

        VolumeHandle active = makeVolumeHandle(volume);
        TimeOrderdCacheFutureSharedPtr<QString, VolumeManager> cache(1);
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
        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

        view.on_nextPage_triggered();
        view.on_prevPage_triggered();
        view.onActionNextPageOrVolume_triggered();
        view.onActionPrevPageOrVolume_triggered();
        view.on_fastForwardPage_triggered();
        view.on_fastBackwardPage_triggered();
        view.on_firstPage_triggered();
        view.on_lastPage_triggered();
        view.on_nextOnlyOnePage_triggered();
        view.on_prevOnlyOnePage_triggered();
        view.on_nextVolume_triggered();
        view.on_prevVolume_triggered();
        view.on_rotatePage_triggered();
        view.on_slideShowChanging_triggered();
        view.on_copyPage_triggered();
        view.on_copyFile_triggered();
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

    void standalonePreviewNavigationIsSafe()
    {
        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

        const QImage image(8, 8, QImage::Format_ARGB32);
        manager.addNewPage(ImageContent(image, "preview.png", image.size(), {}, 0), true);

        view.onActionNextPageOrVolume_triggered();
        view.onActionPrevPageOrVolume_triggered();
        QCOMPARE(manager.currentPageName(), QString("preview.png"));
    }

    void renderedPagesOwnItemsAndExposeSnapshots()
    {
        static_assert(!std::is_copy_constructible_v<PageItem>);
        static_assert(!std::is_copy_assignable_v<PageItem>);

        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

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
        QCOMPARE(contents.at(0)->Path, QString("first.png"));
        QCOMPARE(contents.at(1)->Path, QString("second.png"));
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
        QCOMPARE(contents.at(0)->Path, QString("prepended.png"));
        QCOMPARE(contents.at(1)->Path, QString("replacement.png"));
        QVERIFY(!contents.at(-1));
        QVERIFY(!contents.at(2));
    }

    void fittingModeRelayoutsRenderedPage()
    {
        PageManager manager(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setPageManager(&manager);

        QImage image(640, 480, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(manager.addNewPage(
            ImageContent(image, "fitting.png", image.size(), {}, 0),
            true));

        qApp->setFitting(false);
        view.refreshRenderedPages();
        QCOMPARE(view.renderedPageMetrics().notationalScaleAt(0), 1.0);

        view.on_fitting_triggered(true);
        QVERIFY(qApp->Fitting());
        QVERIFY(view.renderedPageMetrics().notationalScaleAt(0) < 1.0);
    }

    void fittingShortcutTriggersViewAction()
    {
        PageManager manager(nullptr);
        ImageView view;
        view.resize(320, 240);
        view.setPageManager(&manager);

        QAction fittingAction;
        fittingAction.setCheckable(true);
        connect(&fittingAction, &QAction::triggered, &view, &ImageView::on_fitting_triggered);
        QAction *previousAction =
            qApp->keyActions().actions().value("actionFitting", nullptr);
        auto restoreAction = qScopeGuard([previousAction]() {
            qApp->keyActions().actions()["actionFitting"] = previousAction;
        });
        QAction *fittingActionPtr = &fittingAction;
        qApp->keyActions().registAction(
            "actionFitting", fittingActionPtr, "Image");

        QImage image(640, 480, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(manager.addNewPage(
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

        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

        QVERIFY(!manager.loadVolume(directory.path()));
        QCOMPARE(manager.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(manager.size(), 0);
        QVERIFY(!manager.firstPage());
        QVERIFY(!manager.lastPage());
        view.onActionNextPageOrVolume_triggered();
        view.onActionPrevPageOrVolume_triggered();

        manager.dispose();
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

        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

        QVERIFY(!manager.loadVolume(archivePath));
        QCOMPARE(manager.stateKind(), ViewerStateKind::Failed);
        QCOMPARE(manager.size(), 0);
        QVERIFY(!manager.isArchive());
        QVERIFY(!manager.firstPage());
        QVERIFY(!manager.lastPage());
        QVERIFY(!manager.reloadCurrentPage());
        QCOMPARE(manager.currentPagePath(), QString());
        QCOMPARE(manager.currentPageName(), QString());
        QCOMPARE(manager.pageSignage(0), QString());
        view.onActionNextPageOrVolume_triggered();
        view.onActionPrevPageOrVolume_triggered();
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
