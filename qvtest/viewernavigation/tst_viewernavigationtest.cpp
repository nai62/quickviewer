#include <QtTest>

#include "imageview.h"
#include "models/pagemanager.h"
#include "models/qvapplication.h"

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

    void emptyDirectoryNavigationIsSafe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        PageManager manager(nullptr);
        ImageView view;
        view.setPageManager(&manager);

        QVERIFY(!manager.loadVolume(directory.path()));
        QCOMPARE(manager.size(), 0);
        QVERIFY(!manager.firstPage());
        QVERIFY(!manager.lastPage());
        view.onActionNextPageOrVolume_triggered();
        view.onActionPrevPageOrVolume_triggered();
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
