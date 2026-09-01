#include <QtTest>

#include <type_traits>

#include "imageview.h"
#include "models/pagemanager.h"
#include "models/qvapplication.h"
#include "models/volumehandle.h"

class EmptyFileLoader final : public IFileLoader
{
public:
    explicit EmptyFileLoader(QObject *parent = nullptr) : IFileLoader(parent) {}

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

    ImageRetouch brightness() const override { return {}; }

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
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));

        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

        QVERIFY(manager.loadVolumeWithFile(imagePath));
        QCOMPARE(manager.stateKind(), ViewerStateKind::Loading);
        QVERIFY(manager.initialImagePaintPending());

        QTRY_COMPARE(manager.stateKind(), ViewerStateKind::StandalonePreview);
        QCOMPARE(QFileInfo(manager.currentPageName()).fileName(),
                 QString("preview.bmp"));
        QVERIFY(manager.initialImagePaintPending());

        // Navigation while the parent folder is not ready must remain a no-op.
        QVERIFY(!manager.nextPage());
        QVERIFY(!manager.prevPage());
        QVERIFY(!manager.nextVolume());
        QVERIFY(!manager.prevVolume());

        manager.notifyInitialImagePainted();
        QTRY_COMPARE(manager.stateKind(), ViewerStateKind::VolumeReady);
        QVERIFY(!manager.initialImagePaintPending());
    }

    void visiblePagesAreReadOnlySnapshots()
    {
        PageManager manager(nullptr);
        int notificationCount = 0;
        VisiblePages latest;
        connect(&manager, &PageManager::visiblePagesChanged,
                this, [&](VisiblePages pages) {
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

    void pageItemUsesOnlyItsRenderContext()
    {
        QGraphicsScene scene;
        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::red);
        StubPageRenderContext context;
        PageItem page(nullptr, &scene,
                      ImageContent(image, "page.bmp", image.size(), {}, 0),
                      &context);

        page.setPageLayoutFitting(QRect(0, 0, 100, 100),
                                  PageItem::PageCenter,
                                  qvEnums::FitToRect, 1.0);
        QCOMPARE(context.pixelRatioRequests, 1);

        PageItem pageWithoutContext(nullptr, &scene,
                      ImageContent(image, "preview.bmp", image.size(), {}, 0));
        pageWithoutContext.setPageLayoutFitting(QRect(0, 0, 100, 100),
                                  PageItem::PageCenter,
                                  qvEnums::FitToRect, 1.0);
    }

    void emptyVolumeManagerOperationsAreSafe()
    {
        EmptyFileLoader loader;
        VolumeManager volume(nullptr, &loader, nullptr);

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
        volume.moveToThread(nullptr);
    }

    void volumeHandleDestroysOnOwnerThread()
    {
        auto *loader = new EmptyFileLoader;
        auto *volume = new VolumeManager(nullptr, loader, nullptr);
        QThread *destructionThread = nullptr;
        QObject::connect(volume, &QObject::destroyed, this,
                         [&destructionThread] {
            destructionThread = QThread::currentThread();
        }, Qt::DirectConnection);

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
            nullptr, new EmptyFileLoader, nullptr);
        bool destroyed = false;
        QObject::connect(volume, &QObject::destroyed, this,
                         [&destroyed] { destroyed = true; });

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

    void renderedPageItemsHaveSingleOwnership()
    {
        static_assert(!std::is_copy_constructible_v<PageItem>);
        static_assert(!std::is_copy_assignable_v<PageItem>);

        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

        // The legacy return value reports whether the accepted image is wide,
        // so use a wide image to distinguish acceptance from rejection.
        const QImage image(8, 4, QImage::Format_ARGB32);
        QVERIFY(view.on_addImage_triggered(
                    ImageContent(image, "first.png", image.size(), {}, 0), true));
        QCOMPARE(view.renderedPageCount(), 1);
        QVERIFY(view.renderedPageAt(0));

        QVERIFY(view.on_addImage_triggered(
                    ImageContent(image, "second.png", image.size(), {}, 0), true));
        QCOMPARE(view.renderedPageCount(), 2);
        QVERIFY(view.renderedPageAt(1));
        QCOMPARE(view.renderedPageAt(0)->Ic.Path, QString("first.png"));
        QCOMPARE(view.renderedPageAt(1)->Ic.Path, QString("second.png"));
        QVERIFY(!view.on_addImage_triggered(
                    ImageContent(image, "third.png", image.size(), {}, 0), true));

        view.on_clearImages_triggered();
        QCOMPARE(view.renderedPageCount(), 0);
        QVERIFY(!view.renderedPageAt(0));

        QVERIFY(view.on_addImage_triggered(
                    ImageContent(image, "replacement.png", image.size(), {}, 0), false));
        QCOMPARE(view.renderedPageCount(), 1);
        QVERIFY(view.on_addImage_triggered(
                    ImageContent(image, "prepended.png", image.size(), {}, 0), false));
        QCOMPARE(view.renderedPageCount(), 2);
        QCOMPARE(view.renderedPageAt(0)->Ic.Path, QString("prepended.png"));
        QCOMPARE(view.renderedPageAt(1)->Ic.Path, QString("replacement.png"));
        QVERIFY(!view.renderedPageAt(-1));
        QVERIFY(!view.renderedPageAt(2));
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
                     "504b0506000000000000000000000000000000000000")), qint64(22));
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
