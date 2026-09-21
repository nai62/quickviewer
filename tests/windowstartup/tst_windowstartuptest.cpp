#include <QSqlDatabase>
#include "foldertextcache.h"
#include <QtTest>

#include "folderwindow.h"
#include "mainwindow.h"
#include "models/filemanager.h"
#include "models/qvapplication.h"
#include "models/thumbnailmanager.h"

#define FILELOADER_DATAPATH WINDOWSTARTUP_SRCDIR "../fileloader/data/"

class StartupWindow : public ArchiveAwareMainWindow
{
public:
    QList<bool> cloakRequests;
    /** Rows in the folder list, and its first entry, when the cloak was released. */
    int panelRowsAtReveal = -1;
    QString panelEntryAtReveal;

    FolderWindow *folderWindow() const { return m_folderWindow; }
    QSplitter *panelSplitter() const
    {
        return findChild<QSplitter *>(QStringLiteral("catalogSplitter"));
    }
    ViewerSession *viewerSession() { return &m_viewerSession; }
    ImageView *imageView() const { return findChild<ImageView *>(QStringLiteral("graphicsView")); }

protected:
    bool setStartupWindowCloaked(bool cloaked) override
    {
        cloakRequests.append(cloaked);
        if (!cloaked) {
            recordPanelAtReveal();
        }
        return true;
    }

private:
    void recordPanelAtReveal()
    {
        FolderWindow *panel = folderWindow();
        if (!panel) {
            return;
        }
        QListView *view = panel->findChild<QListView *>(QStringLiteral("folderView"));
        if (!view || !view->model()) {
            return;
        }
        panelRowsAtReveal = view->model()->rowCount();
        if (panelRowsAtReveal > 0) {
            panelEntryAtReveal = QFileInfo(panel->itemPath(view->model()->index(0, 0))).fileName();
        }
    }
};

// No OS font cache or scheduling assumptions: the test decides exactly when a
// request completes, including after its originating model has been destroyed.
class DelayedFolderTextCache : public FolderTextCache
{
public:
    QList<QByteArray> requests;
    void finish(const QByteArray &key, bool success = true)
    {
        auto result = QSharedPointer<FolderTextImages>::create();
        for (int i = 0; i < FolderTextImages::ImageCount; ++i) {
            QImage image(20 + i, 16, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            result->images.append(image);
        }
        complete(key, success ? result : FolderTextResult());
    }

protected:
    void submit(const QByteArray &key) override { requests.append(key); }
};

class RecordingFolderStyle : public QProxyStyle
{
public:
    mutable QStringList paintedText;
    void drawControl(ControlElement element,
                     const QStyleOption *option,
                     QPainter *painter,
                     const QWidget *widget = nullptr) const override
    {
        if (element == CE_ItemViewItem) {
            if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
                paintedText.append(item->text);
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }
};

class WindowStartupTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        qApp->setAutoLoaded(false);
        qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::NoViewStartup);
        qApp->setShowPanelSeparateWindow(false);
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(200);
        qApp->setShowSliderBar(true);
        qApp->setDontSavingHistory(false);
        qApp->clearHistory();
        qApp->clearBookmarks();
        qApp->setMaxVolumesCache(4);
        // The suites share the settings file next to their binaries, so every
        // setting a test depends on has to be set here.
        qApp->setShowSubfolders(false);
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
    }

    void startupCloaking_data()
    {
        QTest::addColumn<bool>("beginAsFullscreen");
        QTest::addColumn<bool>("restoreWindowState");
        QTest::addColumn<int>("savedState");
        QTest::addColumn<bool>("expectedFullscreen");

        QTest::newRow("normal") << false << true << int(Qt::WindowNoState) << false;
        QTest::newRow("maximized") << false << true << int(Qt::WindowMaximized) << false;
        QTest::newRow("explicit-fullscreen") << true << false << int(Qt::WindowNoState) << true;
        QTest::newRow("restored-fullscreen") << false << true << int(Qt::WindowFullScreen) << true;
        QTest::newRow("restoration-disabled")
            << false << false << int(Qt::WindowFullScreen) << false;
    }

    void destroyingWindowKeepsUiAliveWhileSessionResets()
    {
        QPointer<StartupWindow> viewer = new StartupWindow;

        delete viewer.data();

        QVERIFY(viewer.isNull());
    }

    void startupCloaking()
    {
        QFETCH(bool, beginAsFullscreen);
        QFETCH(bool, restoreWindowState);
        QFETCH(int, savedState);
        QFETCH(bool, expectedFullscreen);

        // Produce the same geometry blob that closing the viewer saves.
        QMainWindow previous;
        previous.resize(800, 600);
        previous.setWindowState(Qt::WindowStates(savedState));
        qApp->setWindowGeometry(previous.saveGeometry());
        qApp->setWindowState(QByteArray());
        qApp->setRestoreWindowState(restoreWindowState);
        qApp->setBeginAsFullscreen(beginAsFullscreen);
        qApp->setStayOnTop(false);
        qApp->setAutoLoaded(false);
        qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::NoViewStartup);

        StartupWindow viewer;
        viewer.initializeStartup();
        QCOMPARE(viewer.isFullScreen(), expectedFullscreen);
        const QList<bool> expectedRequests = expectedFullscreen ? QList<bool>{} : QList<bool>{true};
        QCOMPARE(viewer.cloakRequests, expectedRequests);
        const QList<bool> completedRequests =
            expectedFullscreen ? QList<bool>{} : QList<bool>{true, false};
        QTRY_COMPARE(viewer.cloakRequests, completedRequests);
        QTRY_COMPARE(viewer.windowOpacity(), qreal(1.0));
        QCOMPARE(viewer.isFullScreen(), expectedFullscreen);
    }

    void folderStartupUsesLightweightPlaceholderBeforeFirstPaint()
    {
        qApp->setAutoLoaded(true);
        qApp->setLastViewPath(QStringLiteral("deferred-startup.zip"));
        qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::FolderStartup);
        qApp->setShowPanelSeparateWindow(false);
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(275);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.initializeStartup();

        QVERIFY(viewer.folderWindow() == nullptr);
        QWidget *placeholder =
            viewer.findChild<QWidget *>(QStringLiteral("startupPanelPlaceholder"));
        QVERIFY(placeholder);
        QCOMPARE(viewer.panelSplitter()->indexOf(placeholder), 0);
        QCOMPARE(viewer.panelSplitter()->sizes().at(0), 275);
    }

    /**
     * The reveal paints the frame the user sees first, so the folder panel has
     * to be part of it. Building the panel after the cloak was released left
     * that first frame with an empty panel: the window appeared with the image,
     * and the list - placeholder and all - only arrived once the fallback font
     * had been loaded.
     */
    void startupRevealsTheWindowWithItsFolderList()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("book.zip"));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), archivePath));

        qApp->setAutoLoaded(true);
        qApp->setLastViewPath(archivePath);
        qApp->setShowOptionViewOnStartup(qvEnums::OptionViewOnStartup::FolderStartup);
        qApp->setShowPanelSeparateWindow(false);

        QTemporaryDir databaseDirectory;
        QVERIFY(databaseDirectory.isValid());
        const auto cleanup = qScopeGuard([&] {
            // The manager's QSqlDatabase handle and the viewer must die before
            // removing the registered connection and its temporary directory.
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
            QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(archivePath) || QFile::remove(archivePath),
                                     5000);
        });
        StartupWindow viewer;
        viewer.resize(800, 600);
        ThumbnailManager manager(&viewer, databaseDirectory.filePath(QStringLiteral("catalog.db")));
        viewer.setThumbnailManager(&manager);

        viewer.initializeStartup();

        // The startup volume is opened from the event loop, and the window stays
        // cloaked until the image view reports the first paint of the decoded
        // image. Report that paint whenever it is outstanding, so the test does
        // not depend on the platform delivering one.
        const auto revealAfterFirstPaint = [&viewer] {
            if (viewer.viewerSession()->initialImagePaintPending()) {
                viewer.viewerSession()->notifyInitialImagePainted();
            }
            return viewer.cloakRequests;
        };
        QTRY_COMPARE(revealAfterFirstPaint(), (QList<bool>{true, false}));

        QCOMPARE(viewer.panelRowsAtReveal, 1);
        QCOMPARE(viewer.panelEntryAtReveal, QStringLiteral("book.zip"));
    }

    void folderTextProfileFinishesWithoutAnInitialImage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray oldProfile = qgetenv("QV_PROFILE_FIRST_IMAGE");
        const QByteArray oldFolderProfile = qgetenv("QV_PROFILE_FOLDER_TEXT");
        const auto restore = qScopeGuard([&] {
            if (oldProfile.isNull()) {
                qunsetenv("QV_PROFILE_FIRST_IMAGE");
            } else {
                qputenv("QV_PROFILE_FIRST_IMAGE", oldProfile);
            }
            if (oldFolderProfile.isNull()) {
                qunsetenv("QV_PROFILE_FOLDER_TEXT");
            } else {
                qputenv("QV_PROFILE_FOLDER_TEXT", oldFolderProfile);
            }
        });
        const QString path = directory.filePath("profile.tsv");
        qputenv("QV_PROFILE_FIRST_IMAGE", path.toLocal8Bit());
        qputenv("QV_PROFILE_FOLDER_TEXT", "1");
        StartupWindow viewer;
        viewer.initializeStartup();
        QTRY_VERIFY(QFile::exists(path));
        QFile profile(path);
        QVERIFY(profile.open(QIODevice::ReadOnly));
        const QByteArray data = profile.readAll();
        QVERIFY(data.contains("folder-text.final-paint.end"));
        QVERIFY(!data.contains("folder-text.profile-timeout"));
    }

    void disabledWidthSavingUsesDefaultWithoutChangingSavedWidth()
    {
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(360);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        QCOMPARE(viewer.folderWindow()->width(), qMax(200, viewer.folderWindow()->minimumWidth()));
        QCOMPARE(qApp->FolderViewWidth(), 360);

        const int displayedWidth = viewer.folderWindow()->width();
        viewer.handleSaveFolderViewWidthActionTriggered(false);
        QCOMPARE(viewer.folderWindow()->width(), displayedWidth);
        QCOMPARE(qApp->FolderViewWidth(), 360);
    }

    void enablingWidthSavingCapturesOnlyVisibleFolderView()
    {
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(360);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        viewer.panelSplitter()->setSizes({275, 515});
        QCoreApplication::processEvents();
        const int displayedWidth = viewer.folderWindow()->width();

        QCOMPARE(qApp->FolderViewWidth(), 360);
        viewer.handleSaveFolderViewWidthActionTriggered(true);
        QCOMPARE(qApp->FolderViewWidth(), displayedWidth);

        viewer.handleSaveFolderViewWidthActionTriggered(false);
        viewer.folderWindow()->hide();
        qApp->setFolderViewWidth(410);
        viewer.handleSaveFolderViewWidthActionTriggered(true);
        QCOMPARE(qApp->FolderViewWidth(), 410);
    }

    void splitterMoveAndCloseSaveActualDockedWidth()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(320);

        StartupWindow viewer;
        viewer.resize(900, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        viewer.panelSplitter()->setSizes({285, 605});
        QCoreApplication::processEvents();
        const int movedWidth = viewer.folderWindow()->width();
        QVERIFY(QMetaObject::invokeMethod(
            viewer.panelSplitter(), "splitterMoved", Q_ARG(int, movedWidth), Q_ARG(int, 1)));
        QCOMPARE(qApp->FolderViewWidth(), movedWidth);

        viewer.panelSplitter()->setSizes({345, 545});
        QCoreApplication::processEvents();
        const int finalWidth = viewer.folderWindow()->width();
        QVERIFY(finalWidth != movedWidth);
        QCOMPARE(qApp->FolderViewWidth(), movedWidth);
        viewer.close();
        QCOMPARE(qApp->FolderViewWidth(), finalWidth);
    }

    void restoredWidthIsConstrainedBySplitter()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(10000);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        QVERIFY(viewer.folderWindow()->width() < 10000);
        QVERIFY(viewer.folderWindow()->width() >= viewer.folderWindow()->minimumWidth());
        QVERIFY(viewer.panelSplitter()->sizes().constLast() >= 0);
    }

    void fullscreenWidthIsSavedOnClose()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(300);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        viewer.handleFullscreenActionTriggered();
        QCoreApplication::processEvents();
        const int fullscreenWidth = viewer.folderWindow()->width();
        viewer.close();

        QCOMPARE(qApp->FolderViewWidth(), fullscreenWidth);
    }

    void enabledWidthSavingRestoresSavedWidth()
    {
        qApp->setSaveFolderViewWidth(true);
        qApp->setFolderViewWidth(310);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QCoreApplication::processEvents();

        QCOMPARE(viewer.folderWindow()->width(), 310);
        QCOMPARE(qApp->FolderViewWidth(), 310);
    }

    void minimumWidthDoesNotDependOnFolderContents()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString longName(180, QLatin1Char('x'));
        QVERIFY(QDir(directory.path()).mkdir(longName));

        StartupWindow viewer;
        viewer.createFolderWindow(true, QString(), true);
        const int uiMinimumWidth = viewer.folderWindow()->minimumWidth();
        viewer.folderWindow()->setFolderPath(directory.path(), false);

        QCOMPARE(viewer.folderWindow()->minimumWidth(), uiMinimumWidth);
    }

    void selectingFolderOnlyEmitsOpenRequest()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("child")));

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        const QString originalPath = folder.currentPath();
        const QString childPath = QDir(originalPath).absoluteFilePath(QStringLiteral("child"));
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex child = view->model()->index(0, 0);
        QVERIFY(child.isValid());
        QCOMPARE(child.data(Qt::DisplayRole).toString(), QStringLiteral("child"));

        QSignalSpy openVolumeSpy(&folder, &FolderWindow::openVolume);
        folder.handleFolderViewItemSelected(child);

        QCOMPARE(openVolumeSpy.size(), 1);
        const OpenTarget target = openVolumeSpy.first().first().value<OpenTarget>();
        QCOMPARE(target.intent, OpenIntent::Container);
        QCOMPARE(target.location.containerPath, childPath);
        QCOMPARE(folder.currentPath(), originalPath);
    }

    void activeFolderVolumeIsExposedByModelRole()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("child")));

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex child = view->model()->index(0, 0);
        QVERIFY(child.isValid());
        QVERIFY(!child.data(FolderItemModel::CurrentVolumeRole).toBool());
        QSignalSpy openVolumeSpy(&folder, &FolderWindow::openVolume);

        folder.handleViewerSessionVolumeChanged(
            QDir(directory.path()).absoluteFilePath(QStringLiteral("child")));

        QVERIFY(child.data(FolderItemModel::CurrentVolumeRole).toBool());
        QCOMPARE(openVolumeSpy.size(), 0);
    }

    void folderViewHighlightsCurrentImageFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath(QStringLiteral("current.png"));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(imagePath));

        StartupWindow viewer;
        viewer.openPath(directory.path());
        QCOMPARE(
            QDir::cleanPath(QDir::fromNativeSeparators(viewer.viewerSession()->currentPagePath())),
            QDir::cleanPath(QDir::fromNativeSeparators(imagePath)));
        viewer.createFolderWindow(true, directory.path(), false);

        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex currentFile = view->model()->index(0, 0);
        QCOMPARE(currentFile.data().toString(), QStringLiteral("current.png"));
        QVERIFY(currentFile.data(FolderItemModel::CurrentVolumeRole).toBool());
    }

    void folderViewHighlightsTheUnwrappedSubdirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkpath(QStringLiteral("inner")));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        const QString imagePath = root.filePath(QStringLiteral("inner/current.png"));
        QVERIFY(image.save(imagePath));

        StartupWindow viewer;
        viewer.openPath(root.path());
        QCOMPARE(
            QDir::cleanPath(QDir::fromNativeSeparators(viewer.viewerSession()->currentPagePath())),
            QDir::cleanPath(QDir::fromNativeSeparators(imagePath)));
        viewer.createFolderWindow(true, root.path(), false);

        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        // The folder holds nothing but the subdirectory, so the page lives in
        // it and the panel marks that row rather than nothing at all.
        const QModelIndex inner = view->model()->index(0, 0);
        QCOMPARE(inner.data().toString(), QStringLiteral("inner"));
        QVERIFY(inner.data(FolderItemModel::CurrentVolumeRole).toBool());
    }

    void folderViewOpensTheEntryItAlreadyMarks()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkpath(QStringLiteral("inner")));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(root.filePath(QStringLiteral("inner/current.png"))));

        StartupWindow viewer;
        viewer.openPath(root.path());
        viewer.createFolderWindow(true, root.path(), false);
        FolderWindow *panel = viewer.folderWindow();
        QListView *view = panel->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);

        // The folder holds nothing but the subdirectory that has the page, so
        // the panel marks that row and makes it the view's current entry.
        const QModelIndex inner = view->model()->index(0, 0);
        QCOMPARE(inner.data().toString(), QStringLiteral("inner"));
        QVERIFY(inner.data(FolderItemModel::CurrentVolumeRole).toBool());
        QCOMPARE(view->currentIndex(), inner);
        const QRect innerRect = view->visualRect(inner);
        QVERIFY(!innerRect.isEmpty());

        QSignalSpy opened(panel, &FolderWindow::openVolume);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, innerRect.center());

        QCOMPARE(opened.size(), 1);
        const OpenTarget target = opened.first().first().value<OpenTarget>();
        QCOMPARE(target.intent, OpenIntent::Container);
        QCOMPARE(
            QDir::cleanPath(QDir::fromNativeSeparators(target.location.containerPath)),
            QDir::cleanPath(QDir::fromNativeSeparators(root.filePath(QStringLiteral("inner")))));

        // And opening it moves the viewer and the panel into that subdirectory.
        viewer.openTarget(target);
        QCOMPARE(
            QDir::cleanPath(QDir::fromNativeSeparators(panel->currentPath())),
            QDir::cleanPath(QDir::fromNativeSeparators(root.filePath(QStringLiteral("inner")))));
    }

    void loadingFolderPageBookmarkFollowsThePanel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 60, 255, 255));
            QVERIFY(image.save(directory.filePath(QStringLiteral("page-%1.bmp").arg(page))));
        }

        StartupWindow viewer;
        viewer.openPath(directory.path());
        QVERIFY(viewer.viewerSession()->selectPage(1));
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("page-1.bmp"));
        viewer.handleSaveBookmarkActionTriggered();
        QCOMPARE(qApp->Bookmarks().size(), 1);

        // The panel starts somewhere else, so loading the bookmark has to move
        // it to the page's folder and mark the page it names.
        QTemporaryDir elsewhere;
        QVERIFY(elsewhere.isValid());
        viewer.createFolderWindow(true, elsewhere.path(), false);
        FolderWindow *panel = viewer.folderWindow();
        QVERIFY(panel);
        QCOMPARE(QDir::cleanPath(QDir::fromNativeSeparators(panel->currentPath())),
                 QDir::cleanPath(QDir::fromNativeSeparators(elsewhere.path())));

        QAction action;
        action.setData(qApp->Bookmarks().first());
        viewer.handleLoadBookmarkMenuTriggered(&action);

        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("page-1.bmp"));
        QCOMPARE(QDir::cleanPath(QDir::fromNativeSeparators(panel->currentPath())),
                 QDir::cleanPath(QDir::fromNativeSeparators(directory.path())));
        QListView *view = panel->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QModelIndex bookmarkedPage;
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            const QModelIndex candidate = view->model()->index(row, 0);
            if (candidate.data().toString() == QStringLiteral("page-1.bmp")) {
                bookmarkedPage = candidate;
                break;
            }
        }
        QVERIFY(bookmarkedPage.isValid());
        QVERIFY(bookmarkedPage.data(FolderItemModel::CurrentVolumeRole).toBool());
    }

    void loadingArchivePageBookmarkFollowsThePanel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("book.7z"));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "7z/image.7z"), archivePath));

        StartupWindow viewer;
        viewer.openPath(archivePath);
        // Bookmark the page the fixture keeps in a subdirectory, so the load
        // has to resolve its name and not just its index.
        const QString entryName =
            QStringLiteral("[sample＋folder] サンプル！　～フォルダ？～/red.jpg");
        int entryIndex = -1;
        for (int page = 0; page < viewer.viewerSession()->pageCount(); ++page) {
            if (viewer.viewerSession()->selectPage(page) &&
                viewer.viewerSession()->currentPageName() == entryName) {
                entryIndex = page;
                break;
            }
        }
        QVERIFY(entryIndex >= 0);
        viewer.handleSaveBookmarkActionTriggered();
        QCOMPARE(qApp->Bookmarks().size(), 1);

        QTemporaryDir elsewhere;
        QVERIFY(elsewhere.isValid());
        viewer.createFolderWindow(true, elsewhere.path(), false);
        FolderWindow *panel = viewer.folderWindow();
        QVERIFY(panel);
        // Archives defer the folder work until the first paint, as they do on a
        // normal open, so release the placeholder directory and wait for it.
        viewer.viewerSession()->notifyInitialImagePainted();
        QTRY_VERIFY(!viewer.viewerSession()->initialImagePaintPending());
        QTRY_COMPARE(QDir::cleanPath(QDir::fromNativeSeparators(panel->currentPath())),
                     QDir::cleanPath(QDir::fromNativeSeparators(elsewhere.path())));

        QAction action;
        action.setData(qApp->Bookmarks().first());
        viewer.handleLoadBookmarkMenuTriggered(&action);

        QCOMPARE(viewer.viewerSession()->currentPageName(), entryName);
        viewer.viewerSession()->notifyInitialImagePainted();
        QTRY_VERIFY(!viewer.viewerSession()->initialImagePaintPending());
        QTRY_COMPARE(QDir::cleanPath(QDir::fromNativeSeparators(panel->currentPath())),
                     QDir::cleanPath(QDir::fromNativeSeparators(directory.path())));
        QListView *view = panel->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QModelIndex archive;
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            const QModelIndex candidate = view->model()->index(row, 0);
            if (candidate.data().toString() == QStringLiteral("book.7z")) {
                archive = candidate;
                break;
            }
        }
        QVERIFY(archive.isValid());
        QTRY_VERIFY(archive.data(FolderItemModel::CurrentVolumeRole).toBool());
    }

    void historyMenuKeepsThePathPastTheShortcutList()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(directory.filePath(QStringLiteral("page.png"))));

        const int previousMaxHistoryCount = qApp->MaxHistoryCount();
        const auto restoreMaxHistoryCount =
            qScopeGuard([&] { qApp->setMaxHistoryCount(previousMaxHistoryCount); });
        qApp->setMaxHistoryCount(37);

        // The shortcut list holds 36 entries, so the 37th entry has no shortcut
        // and shows the path alone.
        const QString oldestPath = QDir::fromNativeSeparators(directory.path());
        qApp->clearHistory();
        qApp->addHistory(oldestPath);
        for (int i = 0; i < 36; ++i) {
            qApp->addHistory(QStringLiteral("filler-%1").arg(i));
        }
        QCOMPARE(qApp->History().size(), 37);
        QCOMPARE(qApp->History().last(), oldestPath);

        StartupWindow viewer;
        viewer.initializeStartup();
        QMenu *historyMenu = viewer.findChild<QMenu *>(QStringLiteral("menuHistory"));
        QVERIFY(historyMenu);
        QTRY_COMPARE(historyMenu->actions().size(), 37);

        QAction *shortcutEntry = historyMenu->actions().at(0);
        QVERIFY(shortcutEntry->text().startsWith(QStringLiteral("&1: ")));
        QCOMPARE(shortcutEntry->data().toString(), QStringLiteral("filler-35"));

        QAction *bareEntry = historyMenu->actions().at(36);
        QCOMPARE(bareEntry->text(), oldestPath);
        QCOMPARE(bareEntry->data().toString(), oldestPath);

        viewer.handleHistoryMenuTriggered(bareEntry);
        QTRY_COMPARE(
            QDir::cleanPath(QDir::fromNativeSeparators(viewer.viewerSession()->volumePath())),
            QDir::cleanPath(oldestPath));
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("page.png"));
    }

    void folderViewGivesTheSideButtonsToTheViewer()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int page = 0; page < 3; ++page) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(page * 60, 255, 255));
            QVERIFY(image.save(directory.filePath(QStringLiteral("page-%1.bmp").arg(page))));
        }

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        viewer.openPath(directory.path());
        QCOMPARE(viewer.viewerSession()->pageCount(), 3);
        QVERIFY(viewer.viewerSession()->selectPage(2));
        QCOMPARE(viewer.viewerSession()->currentPageIndex(), 2);

        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QRect currentRect = view->visualRect(view->currentIndex());
        QVERIFY(!currentRect.isEmpty());

        QSignalSpy opened(viewer.folderWindow(), &FolderWindow::openVolume);
        // The mouse's back button steps a page, like the key that does the same:
        // it belongs to the viewer, not to the row it happens to be over.
        QMouseEvent press(QEvent::MouseButtonPress,
                          currentRect.center(),
                          view->viewport()->mapToGlobal(currentRect.center()),
                          Qt::BackButton,
                          Qt::BackButton,
                          Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &press);

        QCOMPARE(opened.size(), 0);
        QCOMPARE(viewer.viewerSession()->currentPageIndex(), 1);
    }

    void folderViewLeavesEnterToTheWindow()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkpath(QStringLiteral("inner")));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(root.filePath(QStringLiteral("inner/current.png"))));

        StartupWindow viewer;
        viewer.openPath(root.path());
        viewer.createFolderWindow(true, root.path(), false);
        FolderWindow *panel = viewer.folderWindow();
        QListView *view = panel->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        view->setFocus();

        // The panel keeps no key, Enter included. The list has an entry marked,
        // yet Enter belongs to the window, which maps it to the action it has
        // outside the panel - maximizing the window by default - rather than
        // opening that entry.
        QSignalSpy opened(panel, &FolderWindow::openVolume);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(view, &press);

        QCOMPARE(opened.size(), 0);
        QTRY_VERIFY(viewer.isMaximized());
    }

    void folderViewLeavesTheKeysItDoesNotOwnToTheWindow()
    {
        FolderWindow folder(nullptr, nullptr);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        view->setFocus();

        // The window maps these keys, so the list must not take them for its own
        // navigation: an arrow, a page key, home, end and space step the
        // viewer's pages and volumes, and Enter belongs to the window's key
        // settings like every other key.
        const QList<Qt::Key> keys{Qt::Key_Up,
                                  Qt::Key_Down,
                                  Qt::Key_Left,
                                  Qt::Key_Right,
                                  Qt::Key_PageUp,
                                  Qt::Key_PageDown,
                                  Qt::Key_Home,
                                  Qt::Key_End,
                                  Qt::Key_Space,
                                  Qt::Key_Return,
                                  Qt::Key_Enter};
        for (const Qt::Key key : keys) {
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
            QApplication::sendEvent(view, &press);
            QVERIFY2(!press.isAccepted(), qPrintable(QKeySequence(key).toString()));
        }
    }

    void folderViewContextMenuBelongsToTheRowItIsOn()
    {
        const QString previousHome = qApp->HomeFolderPath();
        const auto restoreHome =
            qScopeGuard([previousHome] { qApp->setHomeFolderPath(previousHome); });

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkdir(QStringLiteral("child")));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(root.filePath(QStringLiteral("image.png"))));

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(root.path(), false);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        // The requests below land between rows only when the list has room.
        folder.resize(400, 400);
        folder.show();
        QVERIFY(QTest::qWaitForWindowExposed(&folder));
        QVERIFY(view->viewport()->height() > 100);
        auto *setHome = folder.findChild<QAction *>(QStringLiteral("actionSetAsHomeFolder"));
        QVERIFY(setHome);
        auto *openItem = folder.findChild<QAction *>(QStringLiteral("actionOpenFolderItem"));
        auto *openInExplorer = folder.findChild<QAction *>(QStringLiteral("actionOpenInExplorer"));
        auto *copyPath = folder.findChild<QAction *>(QStringLiteral("actionCopyItemPath"));
        auto *reload = folder.findChild<QAction *>(QStringLiteral("actionReloadFolder"));
        QVERIFY(openItem);
        QVERIFY(openInExplorer);
        QVERIFY(copyPath);
        QVERIFY(reload);

        const auto rowFor = [view](const QString &name) {
            for (int row = 0; row < view->model()->rowCount(); ++row) {
                const QModelIndex index = view->model()->index(row, 0);
                if (index.data().toString() == name) {
                    return index;
                }
            }
            return QModelIndex();
        };
        const QModelIndex child = rowFor(QStringLiteral("child"));
        const QModelIndex file = rowFor(QStringLiteral("image.png"));
        QVERIFY(child.isValid());
        QVERIFY(file.isValid());

        QSignalSpy opened(&folder, &FolderWindow::openVolume);
        // Closing the menu from the event loop keeps a person out of the test,
        // and the menu is kept to say which one the request opened.
        QMenu *shownMenu = nullptr;
        const auto requestMenu = [&](const QModelIndex &index, const QPoint &pos, bool keyboard) {
            QTimer::singleShot(0, [&shownMenu] {
                if (QWidget *popup = QApplication::activePopupWidget()) {
                    shownMenu = qobject_cast<QMenu *>(popup);
                    popup->close();
                }
            });
            const QContextMenuEvent::Reason reason =
                keyboard ? QContextMenuEvent::Keyboard : QContextMenuEvent::Mouse;
            QContextMenuEvent event(reason, pos, view->viewport()->mapToGlobal(pos));
            QApplication::sendEvent(view->viewport(), &event);
        };
        const auto requestRowMenu = [&](const QModelIndex &index) {
            const QRect rect = view->visualRect(index);
            QVERIFY(!rect.isEmpty());
            shownMenu = nullptr;
            requestMenu(index, rect.center(), false);
        };

        // The menu is the folder row's, it offers opening that row, and showing
        // it does not open the row.
        requestRowMenu(child);
        QCOMPARE(opened.size(), 0);
        QVERIFY(shownMenu);
        QVERIFY(shownMenu->actions().contains(openItem));
        QVERIFY(!shownMenu->actions().contains(reload));
        QVERIFY(setHome->isEnabled());
        QVERIFY(openInExplorer->isEnabled());
        setHome->trigger();
        QCOMPARE(
            QDir::cleanPath(QDir::fromNativeSeparators(qApp->HomeFolderPath())),
            QDir::cleanPath(QDir::fromNativeSeparators(root.filePath(QStringLiteral("child")))));

        // A file row has nothing to offer that action, but it opens and copies.
        requestRowMenu(file);
        QVERIFY(!setHome->isEnabled());
        QVERIFY(openItem->isEnabled());
        QVERIFY(copyPath->isEnabled());
        openItem->trigger();
        QCOMPARE(opened.size(), 1);

        // A request over no row - the empty part below the rows - is about the
        // folder the panel shows.
        const QRect lastRow =
            view->visualRect(view->model()->index(view->model()->rowCount() - 1, 0));
        const QPoint emptyAt(4, lastRow.bottom() + 6);
        QVERIFY(emptyAt.y() < view->viewport()->height());
        shownMenu = nullptr;
        requestMenu(QModelIndex(), emptyAt, false);
        QVERIFY(shownMenu);
        QVERIFY(shownMenu->actions().contains(reload));
        QVERIFY(!shownMenu->actions().contains(openItem));
        copyPath->trigger();
        QCOMPARE(QDir::cleanPath(QDir::fromNativeSeparators(QGuiApplication::clipboard()->text())),
                 QDir::cleanPath(QDir::fromNativeSeparators(root.path())));

        // A keyboard request has no pointer over a row, so it belongs to the row
        // the list has as current even when it is aimed at another one.
        view->setCurrentIndex(child);
        shownMenu = nullptr;
        requestMenu(child, view->visualRect(file).center(), true);
        QVERIFY(shownMenu);
        QVERIFY(setHome->isEnabled());
        QVERIFY(shownMenu->actions().contains(openItem));
    }

    void folderViewOpensAPathDroppedOnIt()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath(QStringLiteral("dropped.png"));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(imagePath));

        FolderWindow folder(nullptr, nullptr);
        QSignalSpy opened(&folder, &FolderWindow::openVolume);

        // The viewer opens what was dropped, so the panel follows it instead of
        // listing a folder the viewer knows nothing about.
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(imagePath)});
        // A drop only reaches a widget that accepted the drag first.
        QDragEnterEvent enter(
            QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&folder, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&folder, &drop);

        QCOMPARE(opened.size(), 1);
        const OpenTarget target = opened.first().first().value<OpenTarget>();
        QCOMPARE(target.intent, OpenIntent::FileInContainer);
        QCOMPARE(QDir::cleanPath(QDir::fromNativeSeparators(target.location.containerPath)),
                 QDir::cleanPath(QDir::fromNativeSeparators(directory.path())));
    }

    void folderListRowsAreNoTallerThanTheirNames()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        for (int row = 0; row < 3; ++row) {
            QVERIFY(image.save(directory.filePath(QStringLiteral("image%1.png").arg(row))));
        }

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex row = view->model()->index(0, 0);
        QVERIFY(row.isValid());

        // The panel draws no icon, so a row is about its name. Comparing it with
        // the height the style lays out for an item would only hold on the style
        // that reserves room for an icon - the one this was fixed for - so the
        // row is held to the text it shows instead.
        const int textHeight = view->fontMetrics().height();
        const int rowHeight = view->visualRect(row).height();
        QVERIFY(rowHeight >= textHeight);
        QVERIFY(rowHeight <= textHeight + 8);
    }

    void explorerArgumentOpensAFolderAndSelectsAnEntry()
    {
#ifdef Q_OS_WIN
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkdir(QStringLiteral("child")));
        const QString filePath = root.filePath(QStringLiteral("image.png"));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(filePath));

        // A folder is opened, and an entry inside one is selected in the folder
        // that holds it. Explorer wants the quotes around the path alone.
        const QString folder = QDir::toNativeSeparators(QFileInfo(root.path()).canonicalFilePath());
        const QString file = QDir::toNativeSeparators(QFileInfo(filePath).canonicalFilePath());
        QCOMPARE(explorerArgument(root.path()), QLatin1Char('"') + folder + QLatin1Char('"'));
        QCOMPARE(explorerArgument(filePath),
                 QStringLiteral("/select,") + QLatin1Char('"') + file + QLatin1Char('"'));

        // Explorer would open somewhere else for a path it cannot resolve.
        QVERIFY(explorerArgument(root.filePath(QStringLiteral("gone.png"))).isEmpty());
        QVERIFY(explorerArgument(QString()).isEmpty());
#else
        QSKIP("Explorer is the file manager of Windows");
#endif
    }

    void folderViewRepaintsTheRowTheStoreGainsProgressFor()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("book.zip"));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), archivePath));
        const QString storedPath = QDir::fromNativeSeparators(archivePath);
        const ReadProgress stored{
            QFileInfo(storedPath).fileName(), storedPath, QString(), 4, 1, false};

        // Progress the store holds when the panel lists the folder is on the row
        // from the start.
        qApp->readProgressStore()->insert(storedPath, stored);
        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        FolderWindow *panel = viewer.folderWindow();
        QListView *view = panel->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QAbstractItemModel *model = view->model();
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->index(0, 0)
                     .data(FolderItemModel::ReadProgressRole)
                     .value<ReadProgress>()
                     .totalPageCount,
                 4);

        // Reading on is not a change in the model, so the row is told to repaint.
        // Before, it kept the bar it first painted until a click or a hover
        // happened to redraw it.
        QSignalSpy repainted(model, &QAbstractItemModel::dataChanged);
        qApp->readProgressStore()->insert(storedPath,
                                          {stored.volumeTitle, storedPath, QString(), 4, 3, false});

        int repaintedRow = -1;
        for (const QList<QVariant> &call : repainted) {
            const QList<int> roles = call.at(2).value<QList<int>>();
            if (roles.contains(FolderItemModel::ReadProgressRole)) {
                repaintedRow = call.at(0).value<QModelIndex>().row();
            }
        }
        QCOMPARE(repaintedRow, 0);
        QCOMPARE(model->index(0, 0)
                     .data(FolderItemModel::ReadProgressRole)
                     .value<ReadProgress>()
                     .resumePageIndex,
                 3);
    }

    void folderListPaintsProgressWithoutAPageCount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("book.zip"));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), archivePath));

        // A stored entry can carry no page count. Painting one used to divide by
        // zero and kill the process.
        const QString storedPath = QDir::fromNativeSeparators(archivePath);
        qApp->setOpenVolumeWithProgress(true);
        qApp->readProgressStore()->insertSessionOverride(
            storedPath, {QFileInfo(storedPath).fileName(), storedPath, QString(), 0, 0, false});

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QVERIFY(view->model()->rowCount() > 0);

        view->grab();
    }

    void folderTextSurvivesModelDestructionAndSharesCompletion()
    {
        DelayedFolderTextCache cache;
        const char32_t missing = 0x10ffff;
        const QString name = QStringLiteral("book") + QString::fromUcs4(&missing, 1) + ".zip";
        QList<FolderItem> items{FolderItem(name, FolderItem::Archive, QDateTime())};
        auto first = std::make_unique<FolderItemModel>(nullptr, &cache);
        first->setVolumes(&items);
        first->requestTextImages();
        QCOMPARE(cache.requests.size(), 1);
        const QByteArray key = cache.requests.first();
        FolderItemModel second(nullptr, &cache);
        second.setVolumes(&items);
        second.requestTextImages();
        QCOMPARE(cache.requests.size(), 1);
        QVERIFY(second.data(second.index(0, 0), Qt::DisplayRole).toString() != name);
        first.reset();
        cache.finish(key);
        QCOMPARE(second.data(second.index(0, 0), Qt::DisplayRole).toString(), name);
        QVERIFY(!second.textImagesPending());
        FolderItemModel reopened(nullptr, &cache);
        reopened.setVolumes(&items);
        QCOMPARE(reopened.data(reopened.index(0, 0), Qt::DisplayRole).toString(), name);
        QCOMPARE(cache.requests.size(), 1);
    }

    void folderTextIgnoresOldFolderAndStyleCompletions()
    {
        DelayedFolderTextCache cache;
        const char32_t missing = 0x10ffff;
        const QString suffix = QString::fromUcs4(&missing, 1);
        QList<FolderItem> first{FolderItem("a" + suffix, FolderItem::Archive, QDateTime())};
        QList<FolderItem> next{FolderItem("b" + suffix, FolderItem::Archive, QDateTime())};
        FolderItemModel model(nullptr, &cache);
        model.setVolumes(&first);
        model.requestTextImages();
        const QByteArray oldKey = cache.requests.last();
        model.setVolumes(&next);
        model.requestTextImages();
        const QByteArray nextKey = cache.requests.last();
        QFont font = QApplication::font();
        font.setPixelSize(24);
        model.setTextStyle(font, QApplication::palette(), 2);
        model.requestTextImages();
        const QByteArray styledKey = cache.requests.last();
        QVERIFY(oldKey != nextKey);
        QVERIFY(nextKey != styledKey);
        cache.finish(oldKey);
        cache.finish(nextKey);
        QVERIFY(model.data(model.index(0, 0), Qt::DisplayRole).toString() != next.first().name);
        cache.finish(styledKey);
        QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), next.first().name);
        const auto result = model.data(model.index(0, 0), FolderItemModel::TextImagesRole)
                                .value<FolderTextResult>();
        QVERIFY(result);
        QCOMPARE(result->images.size(), FolderTextImages::ImageCount);
    }

    void folderTextFailureRetriesOnceAndDoesNotBlockOtherNames()
    {
        DelayedFolderTextCache cache;
        const char32_t missing = 0x10ffff;
        const QString suffix = QString::fromUcs4(&missing, 1);
        QList<FolderItem> items{FolderItem("a" + suffix, FolderItem::Archive, QDateTime())};
        FolderItemModel model(nullptr, &cache);
        model.setVolumes(&items);
        model.requestTextImages();
        QCOMPARE(cache.requests.size(), 1);

        // A failure settles the pending state, and the name gets one more try.
        cache.finish(cache.requests.first(), false);
        QVERIFY(!model.textImagesPending());
        model.requestTextImages();
        QCOMPARE(cache.requests.size(), FolderTextCache::MaxAttempts);

        // Out of attempts: the name keeps its placeholder instead of asking on
        // every paint.
        cache.finish(cache.requests.last(), false);
        model.requestTextImages();
        QCOMPARE(cache.requests.size(), FolderTextCache::MaxAttempts);

        // A different name is not held back by that.
        items[0].name = "b" + suffix;
        model.setVolumes(&items);
        model.requestTextImages();
        QCOMPARE(cache.requests.size(), FolderTextCache::MaxAttempts + 1);
        cache.finish(cache.requests.last());
        QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), items.first().name);
    }

    void folderTextPaintNeverShapesTheOriginalMissingGlyph()
    {
        const bool progress = qApp->ShowReadProgress();
        qApp->setShowReadProgress(false);
        const auto restore = qScopeGuard([progress] { qApp->setShowReadProgress(progress); });
        DelayedFolderTextCache cache;
        const char32_t missing = 0x10ffff;
        const QString name = "book" + QString::fromUcs4(&missing, 1);
        QList<FolderItem> items{FolderItem(name, FolderItem::Archive, QDateTime())};
        FolderItemModel model(nullptr, &cache);
        QListView view;
        FolderItemDelegate delegate(&view);
        auto *style = new RecordingFolderStyle;
        style->setParent(&view);
        view.setStyle(style);
        view.setModel(&model);
        view.setItemDelegate(&delegate);
        model.setVolumes(&items);
        model.requestTextImages();
        view.resize(300, 150);
        view.grab();
        QVERIFY(!style->paintedText.isEmpty());
        QVERIFY(!style->paintedText.contains(name));
        QVERIFY(model.textImagesPending());
        // Completion is deliberately held until after the GUI has painted.
        // Both the regular and current/bold paints must consume the images.
        cache.finish(cache.requests.first());
        QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), name);
        for (int current : {-1, 0}) {
            model.setCurrentVolumeRow(current);
            style->paintedText.clear();
            view.grab();
            QVERIFY(!style->paintedText.isEmpty());
            for (const auto &text : style->paintedText) {
                QVERIFY(text.isEmpty());
            }
        }
        view.setItemDelegate(nullptr);
        view.setModel(nullptr);
    }

    void folderTextCacheKeyIncludesRenderingConditions()
    {
        const QFont font = QApplication::font();
        // The helper returns colourless masks, so the key carries only what
        // changes the pixels it renders: the text, the font and the scale.
        const auto base = FolderTextCache::key("name", font, 1);
        QVERIFY(base != FolderTextCache::key("name", font, 2));
        QVERIFY(base != FolderTextCache::key("other", font, 1));
        QFont bold = font;
        bold.setBold(true);
        QVERIFY(base != FolderTextCache::key("name", bold, 1));
    }

    void folderTextHelperReturnsImagesAtTheRequestedScale()
    {
        FolderTextCache cache;
        QFont font = QApplication::font();
        const QByteArray key = FolderTextCache::key(QString::fromUtf8("test摇.zip"), font, 2);
        cache.request(key);
        QTRY_VERIFY_WITH_TIMEOUT(!cache.pending(key), 15000);
        const auto result = cache.lookup(key);
        QVERIFY(result);
        QCOMPARE(result->images.size(), FolderTextImages::ImageCount);
        for (const auto &image : result->images) {
            QVERIFY(!image.isNull());
            QCOMPARE(image.devicePixelRatio(), 2.0);
        }
    }

    void folderTextRequestsOnlyTheRowsTheListShows()
    {
        DelayedFolderTextCache cache;
        const char32_t missing = 0x10ffff;
        const QString suffix = QString::fromUcs4(&missing, 1);
        QList<FolderItem> items;
        for (int row = 0; row < 40; ++row) {
            items.append(FolderItem(
                QStringLiteral("book%1").arg(row) + suffix, FolderItem::Archive, QDateTime()));
        }
        FolderItemModel model(nullptr, &cache);
        model.setVolumes(&items);
        model.setVisibleRowRange(10, 12);
        model.requestTextImages();
        // Only the rows the list shows ask for text images.
        QCOMPARE(cache.requests.size(), 3);

        model.setVisibleRowRange(20, 21);
        model.requestTextImages();
        QCOMPARE(cache.requests.size(), 5);

        // The profile settles the whole folder, still in the batches the cache
        // accepts at once; the rest follows as results arrive.
        model.requestAllTextImages();
        QCOMPARE(cache.requests.size(), FolderTextCache::MaxOutstanding);
        QVERIFY(model.textImagesPending());
    }

    void folderTextHelperLeavesWhenItIsIdle()
    {
        FolderTextCache cache;
        cache.setIdleShutdownInterval(200);
        const QFont font = QApplication::font();
        const QByteArray first = FolderTextCache::key(QString::fromUtf8("test\u6447.zip"), font, 1);
        cache.request(first);
        QTRY_VERIFY_WITH_TIMEOUT(!cache.pending(first), 15000);
        QVERIFY(cache.helperRunning());

        // Nothing is in flight: the helper leaves instead of staying resident...
        QTRY_VERIFY_WITH_TIMEOUT(!cache.helperRunning(), 15000);

        // ...and the next request starts it again.
        const QByteArray second =
            FolderTextCache::key(QString::fromUtf8("test\u6447 2.zip"), font, 1);
        cache.request(second);
        QTRY_VERIFY_WITH_TIMEOUT(!cache.pending(second), 15000);
        QVERIFY(cache.lookup(second));
    }

    void folderListReplacesGlyphsTheFontCannotDraw()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        // U+1F600 is outside the coverage of the UI fonts this runs with, so the
        // list cannot draw it before a fallback font has been loaded.
        const char32_t emoji = 0x1F600;
        const QString rareName =
            QStringLiteral("book") + QString::fromUcs4(&emoji, 1) + QStringLiteral(".zip");
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"),
                            directory.filePath(rareName)));

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const QModelIndex row = view->model()->index(0, 0);
        QVERIFY(row.isValid());
        // The list starts with a placeholder...
        QVERIFY(!row.data().toString().contains(QChar(0xD83D)));
        QVERIFY(row.data().toString() != rareName);

        view->grab();
        // ...and is completed once the fallback font has been loaded.
        QTRY_VERIFY(view->model()->index(0, 0).data().toString().contains(QChar(0xD83D)));
    }

    /**
     * A paint of the list used to start the fallback font load itself, and the
     * rows then waited about 200 ms on the font database before the panel could
     * appear. The load is queued behind the paint: the row this paint draws is
     * the placeholder, and the load only starts once the paint has returned.
     */
    void folderListPaintsWithoutWaitingForTheFallbackFont()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        // A glyph of its own, so the load this test watches cannot have been
        // warmed by another test case.
        const char32_t unicorn = 0x1F984;
        const QString rareName =
            QStringLiteral("book") + QString::fromUcs4(&unicorn, 1) + QStringLiteral(".zip");
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"),
                            directory.filePath(rareName)));

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        auto *model = qobject_cast<FolderItemModel *>(view->model());
        QVERIFY(model);
        const QModelIndex row = model->index(0, 0);
        QVERIFY(row.isValid());
        QVERIFY(!row.data().toString().contains(QString::fromUcs4(&unicorn, 1)));

        // The first frame of the list is a synchronous paint like this one.
        view->grab();

        QVERIFY2(!model->textImagesPending(),
                 "the list paint started the fallback font load it would have to wait for");
        // The load runs after the paint and completes the row.
        QTRY_VERIFY(row.data().toString().contains(QString::fromUcs4(&unicorn, 1)));
    }

    void folderListPlaceholdersNeverDescribeAnotherEntry()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const char32_t emoji = 0x1F600;
        const QString rareName =
            QStringLiteral("a") + QString::fromUcs4(&emoji, 1) + QStringLiteral(".zip");
        const QString plainName = QStringLiteral("b.zip");
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"),
                            directory.filePath(rareName)));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"),
                            directory.filePath(plainName)));

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        FolderWindow *panel = viewer.folderWindow();
        QListView *view = panel->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(panel);
        QVERIFY(view);

        // The display may hold a placeholder, but the path the panel acts on -
        // opening, the read-progress lookup, and the volume cache key - is always
        // the name on disk, and a row never shows another entry's text.
        const auto checkRows = [&] {
            QCOMPARE(view->model()->rowCount(), 2);
            for (int row = 0; row < view->model()->rowCount(); ++row) {
                const QModelIndex index = view->model()->index(row, 0);
                const QString display = index.data().toString();
                const QString real = QFileInfo(panel->itemPath(index)).fileName();
                if (real.contains(QChar(0xD83D))) {
                    // Either the name itself, or the same name with the glyph the
                    // font cannot draw replaced - never another entry's name. The
                    // fallback font may already be loaded from an earlier test, so
                    // both are valid here.
                    const bool placeholder = display.startsWith(QStringLiteral("a")) &&
                                             display.endsWith(QStringLiteral(".zip")) &&
                                             !display.contains(QChar(0xD83D));
                    QVERIFY2(display == real || placeholder,
                             qPrintable(QStringLiteral("row %1 shows %2").arg(row).arg(display)));
                } else {
                    QCOMPARE(display, real);
                }
            }
        };
        checkRows();

        // Changing the sort while the placeholder is showing must not move the
        // replacement onto another row.
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileNameDescending);
        panel->resortVolumes();
        checkRows();

        // The real names arrive once the fallback font has been loaded.
        view->grab();
        const auto rareNameIsShown = [&] {
            for (int row = 0; row < view->model()->rowCount(); ++row) {
                if (view->model()->index(row, 0).data().toString().contains(QChar(0xD83D))) {
                    return true;
                }
            }
            return false;
        };
        QTRY_VERIFY(rareNameIsShown());
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            const QModelIndex index = view->model()->index(row, 0);
            QCOMPARE(index.data().toString(), QFileInfo(panel->itemPath(index)).fileName());
        }

        // Rebuilding the list once the font has been loaded keeps the real names:
        // the load is remembered per process, so opening another entry of the same
        // folder (which re-lists it) must not fall back to the placeholder.
        panel->setFolderPath(directory.path(), false);
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            const QModelIndex index = view->model()->index(row, 0);
            QCOMPARE(index.data().toString(), QFileInfo(panel->itemPath(index)).fileName());
        }
    }

    void folderViewConsumesWheelEventsAtScrollBoundary()
    {
        FolderWindow folder(nullptr, nullptr);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        // One entry per row, top to bottom, and never wrapped.
        QCOMPARE(view->flow(), QListView::TopToBottom);
        QVERIFY(!view->isWrapping());

        QWheelEvent event(QPointF(1, 1),
                          QPointF(1, 1),
                          QPoint(),
                          QPoint(0, -120),
                          Qt::NoButton,
                          Qt::NoModifier,
                          Qt::NoScrollPhase,
                          false);
        event.ignore();
        QApplication::sendEvent(view->viewport(), &event);

        QVERIFY(event.isAccepted());
    }

    void folderWindowLeavesUnhandledFunctionKeysForParent()
    {
        FolderWindow folder(nullptr, nullptr);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier);

        QApplication::sendEvent(&folder, &event);

        QVERIFY(!event.isAccepted());
    }

    void folderViewFocusStillAllowsGlobalFunctionKeys()
    {
        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(true, QString(), true);
        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);

        // A widget has keyboard focus only while its window is active, and the
        // window system can refuse to activate a window while tests run. The
        // viewer itself asks its window which widget it keeps as its focus, so
        // the test asks the window the same question.
        view->setFocus(Qt::OtherFocusReason);
        const auto viewHoldsTheFocus = [&view, &viewer] {
            for (QWidget *focused = viewer.focusWidget(); focused;
                 focused = focused->focusWidget()) {
                if (focused == view || focused == view->viewport()) {
                    return true;
                }
            }
            return false;
        };
        QTRY_VERIFY(viewHoldsTheFocus());

        QTest::keyPress(view, Qt::Key_F4);

        // The viewer defers an action that the folder view started, so the
        // panel outlives the key event and closes on the next turn of the loop.
        QVERIFY(viewer.folderWindow() != nullptr);
        QTRY_VERIFY(viewer.folderWindow() == nullptr);
    }

    void closingTheFolderPanelWithAnOpenImageKeepsTheViewerAlive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(directory.filePath(QStringLiteral("page-0.bmp"))));

        StartupWindow viewer;
        viewer.openPath(directory.path());
        viewer.createFolderWindow(true, directory.path(), false);
        FolderWindow *folder = viewer.folderWindow();
        QVERIFY(folder);
        QListView *view = folder->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QVERIFY(view->model()->rowCount() > 0);

        // F4 and the "Show folder" action close the panel while it is listing
        // entries and while the viewer displays a volume.
        viewer.handleShowFolderActionTriggered();

        QVERIFY(viewer.folderWindow() == nullptr);
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("page-0.bmp"));
    }

    void folderViewIsDestroyedBeforeItsDelegate()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage image(16, 16, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(directory.filePath(QStringLiteral("page.bmp"))));

        auto *folder = new FolderWindow(nullptr, nullptr);
        folder->setFolderPath(directory.path(), false);
        QListView *view = folder->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QAbstractItemDelegate *delegate = view->itemDelegate();
        QVERIFY(delegate);

        // The view keeps a raw pointer to the delegate. Qt clears the model
        // pointer when the model dies, but nothing clears the delegate, so the
        // view has to be destroyed while the delegate is still alive.
        bool delegateDestroyed = false;
        bool viewDiedWithLiveDelegate = false;
        QObject::connect(delegate, &QObject::destroyed, qApp, [&delegateDestroyed] {
            delegateDestroyed = true;
        });
        QObject::connect(view, &QObject::destroyed, qApp, [&] {
            viewDiedWithLiveDelegate = !delegateDestroyed;
        });

        delete folder;

        QVERIFY2(viewDiedWithLiveDelegate, "the folder view outlived its delegate");
    }

    void historyButtonUsesClockIconAndLabel()
    {
        FolderWindow folder(nullptr, nullptr);
        QToolButton *historyButton =
            folder.findChild<QToolButton *>(QStringLiteral("historyButton"));
        QVERIFY(historyButton);
        QCOMPARE(historyButton->text(), QStringLiteral("History"));
        QVERIFY(!historyButton->icon().isNull());
        QCOMPARE(historyButton->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    }

    void historyButtonIsTheLastControlInTheButtonRow()
    {
        FolderWindow folder(nullptr, nullptr);
        QToolButton *historyButton =
            folder.findChild<QToolButton *>(QStringLiteral("historyButton"));
        QVERIFY(historyButton);
        QFrame *frame = folder.findChild<QFrame *>(QStringLiteral("frame"));
        QVERIFY(frame);
        QLayout *layout = frame->layout();
        QVERIFY(layout);
        QVERIFY(layout->count() >= 2);

        // The spacer before it keeps History at the right edge of the row.
        QVERIFY(layout->itemAt(layout->count() - 1)->widget() == historyButton);
        QVERIFY(layout->itemAt(layout->count() - 2)->spacerItem() != nullptr);
    }

    void folderViewsSingleColumnHasNoTitle()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        // A list shows one column, and it has no title.
        QCOMPARE(view->model()->columnCount(), 1);
        // The single column has no title. Qt fills the section number in for a
        // model that does not name its header, so that is all the view can
        // show even if it ever displayed one.
        QCOMPARE(view->model()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
                 QStringLiteral("1"));

        viewer.createFolderWindow(false, directory.path(), false);
        view = viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QCOMPARE(view->model()->columnCount(), 1);
    }

    void folderButtonLayoutUsesCompactMargins()
    {
        FolderWindow folder(nullptr, nullptr);
        QFrame *buttonFrame = folder.findChild<QFrame *>(QStringLiteral("frame"));
        QVERIFY(buttonFrame);
        const QMargins margins = buttonFrame->layout()->contentsMargins();
        QCOMPARE(margins, QMargins(4, 4, 4, 4));
        QCOMPARE(buttonFrame->layout()->spacing(), 2);
    }

    void menuBarSortMovesTheFolderViewAndKeepsThePage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage large(32, 48, QImage::Format_RGB32);
        large.fill(Qt::red);
        QVERIFY(large.save(directory.filePath(QStringLiteral("a.bmp"))));
        QImage small(16, 24, QImage::Format_RGB32);
        small.fill(Qt::blue);
        QVERIFY(small.save(directory.filePath(QStringLiteral("b.bmp"))));

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        FolderWindow *folder = viewer.folderWindow();
        QVERIFY(folder);
        QListView *view = folder->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        viewer.openPath(directory.path());
        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("a.bmp"));
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("a.bmp"));

        // The menu bar sort drives both the panel and the viewer.
        viewer.handleSortByFileSizeActionTriggered();

        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("b.bmp"));
        QCOMPARE(view->model()->index(1, 0).data().toString(), QStringLiteral("a.bmp"));
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("a.bmp"));
        QCOMPARE(viewer.viewerSession()->currentPageIndex(), 1);
    }

    void separateWindowFollowsTheMenuBarSort()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage large(32, 48, QImage::Format_RGB32);
        large.fill(Qt::red);
        QVERIFY(large.save(directory.filePath(QStringLiteral("a.bmp"))));
        QImage small(16, 24, QImage::Format_RGB32);
        small.fill(Qt::blue);
        QVERIFY(small.save(directory.filePath(QStringLiteral("b.bmp"))));

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        StartupWindow viewer;
        viewer.createFolderWindow(false, directory.path(), false);
        QVERIFY(viewer.folderWindow());
        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("a.bmp"));

        // The independent window follows the menu bar sort as well.
        viewer.handleSortByFileSizeActionTriggered();

        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("b.bmp"));
        QCOMPARE(view->model()->index(1, 0).data().toString(), QStringLiteral("a.bmp"));
    }

    void showingSubfoldersScansImmediatelyAndKeepsThePage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("sub")));
        const QString rootPath = directory.filePath(QStringLiteral("page-0.bmp"));
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(rootPath));
        image.fill(Qt::blue);
        QVERIFY(image.save(QDir(directory.path()).filePath(QStringLiteral("sub/page-1.bmp"))));

        qApp->setShowSubfolders(false);
        StartupWindow viewer;
        viewer.openPath(directory.path());
        QCOMPARE(viewer.viewerSession()->pageCount(), 1);

        viewer.handleShowSubfoldersActionTriggered(true);

        QVERIFY(qApp->ShowSubfolders());
        QCOMPARE(viewer.viewerSession()->pageCount(), 2);
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("page-0.bmp"));

        // Turning the option off leaves the displayed volume alone.
        viewer.handleShowSubfoldersActionTriggered(false);
        QVERIFY(!qApp->ShowSubfolders());
        QCOMPARE(viewer.viewerSession()->pageCount(), 2);
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("page-0.bmp"));
    }

    void turningSubfoldersOffAppliesToTheNextOpen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("sub")));
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(directory.filePath(QStringLiteral("page-0.bmp"))));
        image.fill(Qt::blue);
        QVERIFY(image.save(QDir(directory.path()).filePath(QStringLiteral("sub/page-1.bmp"))));

        qApp->setShowSubfolders(false);
        StartupWindow viewer;
        viewer.openPath(directory.path());
        QCOMPARE(viewer.viewerSession()->pageCount(), 1);

        viewer.handleShowSubfoldersActionTriggered(true);
        viewer.handleShowSubfoldersActionTriggered(false);
        QVERIFY(!qApp->ShowSubfolders());
        QCOMPARE(viewer.viewerSession()->pageCount(), 2);

        // Turning it off is postponed, not cancelled: the folder is read
        // without its subfolders the next time it is opened.
        viewer.openPath(directory.path());
        QCOMPARE(viewer.viewerSession()->pageCount(), 1);
    }

    void subfolderToggleKeepsTheRequestedPage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("sub")));
        QImage large(32, 48, QImage::Format_RGB32);
        large.fill(Qt::red);
        QVERIFY(large.save(directory.filePath(QStringLiteral("a-large.bmp"))));
        QImage small(16, 24, QImage::Format_RGB32);
        small.fill(Qt::blue);
        QVERIFY(small.save(QDir(directory.path()).filePath(QStringLiteral("sub/b-small.bmp"))));

        qApp->setShowSubfolders(false);
        qApp->setOpenVolumeWithProgress(false);
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileSize);
        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        FolderWindow *folder = viewer.folderWindow();
        QVERIFY(folder);
        QListView *view = folder->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        viewer.openPath(directory.path());
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("a-large.bmp"));

        // Opening the subfolder from the panel shows the image in it.
        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("sub"));
        folder->handleFolderViewItemSelected(view->model()->index(0, 0));
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("b-small.bmp"));

        // Turning the option on and going back to the root makes the volume
        // span both folders, so the file names and the page order differ.
        viewer.handleShowSubfoldersActionTriggered(true);
        folder->handleParentButtonClicked();

        int imageRow = -1;
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            if (view->model()->index(row, 0).data().toString() == QStringLiteral("a-large.bmp")) {
                imageRow = row;
                break;
            }
        }
        QVERIFY(imageRow >= 0);
        folder->handleFolderViewItemSelected(view->model()->index(imageRow, 0));

        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("a-large.bmp"));
    }

    void subfolderToggleKeepsTheRememberedPage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("b")));
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(directory.filePath(QStringLiteral("a.bmp"))));
        image.fill(Qt::green);
        QVERIFY(image.save(directory.filePath(QStringLiteral("c.bmp"))));
        image.fill(Qt::blue);
        QVERIFY(image.save(directory.filePath(QStringLiteral("d.bmp"))));
        image.fill(Qt::yellow);
        QVERIFY(image.save(QDir(directory.path()).filePath(QStringLiteral("b/page.bmp"))));

        qApp->setShowSubfolders(false);
        qApp->setOpenVolumeWithProgress(true);
        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        FolderWindow *folder = viewer.folderWindow();
        QVERIFY(folder);
        QListView *view = folder->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        viewer.openPath(directory.path());
        QCOMPARE(viewer.viewerSession()->pageCount(), 3);

        // The reader stops on a page that sits at another index once the
        // subfolder is listed: "a.bmp", "c.bmp", "d.bmp" becomes "a.bmp",
        // "b/page.bmp", "c.bmp", "d.bmp".
        QVERIFY(viewer.viewerSession()->selectPage(1));
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("c.bmp"));

        int folderRow = -1;
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            if (view->model()->index(row, 0).data().toString() == QStringLiteral("b")) {
                folderRow = row;
                break;
            }
        }
        QVERIFY(folderRow >= 0);
        folder->handleFolderViewItemSelected(view->model()->index(folderRow, 0));
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("page.bmp"));

        viewer.handleShowSubfoldersActionTriggered(true);
        folder->handleParentButtonClicked();

        QCOMPARE(viewer.viewerSession()->pageCount(), 4);
        QCOMPARE(viewer.viewerSession()->currentPageName(), QStringLiteral("c.bmp"));
    }

    void folderViewHighlightsTheFolderOfASubfolderPage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("sub")));
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(QDir(directory.path()).filePath(QStringLiteral("sub/page-0.bmp"))));

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QCOMPARE(view->model()->rowCount(), 1);

        folder.handleViewerSessionVolumeChanged(
            QDir(directory.path()).filePath(QStringLiteral("sub/page-0.bmp")));

        QCOMPARE(view->currentIndex().data().toString(), QStringLiteral("sub"));
        // The folder that leads to the page is marked like a displayed file.
        QVERIFY(view->currentIndex().data(FolderItemModel::CurrentVolumeRole).toBool());

        // Rebuilding the list keeps the mark.
        folder.setFolderPath(directory.path(), false);
        QCOMPARE(view->currentIndex().data().toString(), QStringLiteral("sub"));
        QVERIFY(view->currentIndex().data(FolderItemModel::CurrentVolumeRole).toBool());
    }

    void openingFileInShownFolderDoesNotRereadIt()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString firstPath = directory.filePath(QStringLiteral("page-0.bmp"));
        QImage image(16, 24, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(firstPath));

        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        FolderWindow *folder = viewer.folderWindow();
        QVERIFY(folder);
        QListView *view = folder->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QCOMPARE(view->model()->rowCount(), 1);

        // A file is added and another file of the same folder is opened.
        image.fill(Qt::blue);
        QVERIFY(image.save(directory.filePath(QStringLiteral("page-1.bmp"))));
        viewer.openPath(firstPath);

        // The panel keeps its list until it is reloaded.
        QCOMPARE(view->model()->rowCount(), 1);
        folder->handleReloadButtonClicked();
        QCOMPARE(view->model()->rowCount(), 2);
    }

    void reloadButtonRequestsAContainerReload()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QSignalSpy reloadSpy(&folder, &FolderWindow::reloadRequested);

        folder.handleReloadButtonClicked();

        QCOMPARE(reloadSpy.size(), 1);
        QCOMPARE(reloadSpy.first().first().toString(), folder.currentPath());
    }

    void nameSortKeepsArchivesAndImagesInOneFileGroup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("middle-folder")));

        QFile archive(directory.filePath(QStringLiteral("z-last.zip")));
        QVERIFY(archive.open(QIODevice::WriteOnly));
        archive.close();
        QFile image(directory.filePath(QStringLiteral("a-first.png")));
        QVERIFY(image.open(QIODevice::WriteOnly));
        image.close();

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);

        QCOMPARE(view->model()->rowCount(), 3);
        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("middle-folder"));
        QCOMPARE(view->model()->index(1, 0).data().toString(), QStringLiteral("a-first.png"));
        QCOMPARE(view->model()->index(2, 0).data().toString(), QStringLiteral("z-last.zip"));
    }

    void foldersStayFirstForEveryImageSort()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("z-folder")));

        QFile small(directory.filePath(QStringLiteral("a-small.bmp")));
        QVERIFY(small.open(QIODevice::WriteOnly));
        QCOMPARE(small.write(QByteArray(4, 'a')), qint64(4));
        small.close();
        QFile large(directory.filePath(QStringLiteral("b-large.bmp")));
        QVERIFY(large.open(QIODevice::WriteOnly));
        QCOMPARE(large.write(QByteArray(4096, 'b')), qint64(4096));
        large.close();

        const QList<qvEnums::ImageSortBy> sortModes{
            qvEnums::ImageSortBy::SortByFileName,
            qvEnums::ImageSortBy::SortByFileNameDescending,
            qvEnums::ImageSortBy::SortByFileSize,
            qvEnums::ImageSortBy::SortByFileSizeDescending,
            qvEnums::ImageSortBy::SortByModifiedTime,
            qvEnums::ImageSortBy::SortByModifiedTimeDescending,
        };
        for (const qvEnums::ImageSortBy sortBy : sortModes) {
            qApp->setImageSortBy(sortBy);
            FolderWindow folder(nullptr, nullptr);
            folder.setFolderPath(directory.path(), false);
            QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
            QVERIFY(view);

            QCOMPARE(view->model()->rowCount(), 3);
            QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("z-folder"));

            if (sortBy == qvEnums::ImageSortBy::SortByFileSize) {
                QCOMPARE(view->model()->index(1, 0).data().toString(),
                         QStringLiteral("a-small.bmp"));
                QCOMPARE(view->model()->index(2, 0).data().toString(),
                         QStringLiteral("b-large.bmp"));
            }
            if (sortBy == qvEnums::ImageSortBy::SortByFileSizeDescending) {
                QCOMPARE(view->model()->index(1, 0).data().toString(),
                         QStringLiteral("b-large.bmp"));
                QCOMPARE(view->model()->index(2, 0).data().toString(),
                         QStringLiteral("a-small.bmp"));
            }
        }
    }

    void folderViewKeepsTheViewerPageOrder()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (const QString &name : {QStringLiteral("page2.bmp"), QStringLiteral("page10.bmp")}) {
            QImage image(16, 24, QImage::Format_RGB32);
            image.fill(Qt::white);
            QVERIFY(image.save(directory.filePath(name)));
        }

        qApp->setImageSortBy(qvEnums::ImageSortBy::SortByFileName);
        FolderWindow folder(nullptr, nullptr);
        folder.setFolderPath(directory.path(), false);
        QListView *view = folder.findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QCOMPARE(view->model()->rowCount(), 2);
        QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("page2.bmp"));
        QCOMPARE(view->model()->index(1, 0).data().toString(), QStringLiteral("page10.bmp"));

        ViewerSession session(nullptr);
        QVERIFY(session.openContainer(directory.path()));
        QCOMPARE(session.pageCount(), 2);
        QCOMPARE(session.currentPageName(), QStringLiteral("page2.bmp"));
        QVERIFY(session.selectPage(1));
        QCOMPARE(session.currentPageName(), QStringLiteral("page10.bmp"));
    }

    void separateWindowUsesSharedSavedWidthOnEnableAndExit()
    {
        qApp->setSaveFolderViewWidth(false);
        qApp->setFolderViewWidth(330);

        StartupWindow viewer;
        viewer.resize(800, 600);
        viewer.show();
        viewer.createFolderWindow(false, QString(), true);
        QCoreApplication::processEvents();
        viewer.folderWindow()->resize(420, viewer.folderWindow()->height());
        QCoreApplication::processEvents();

        QCOMPARE(qApp->FolderViewWidth(), 330);
        viewer.handleSaveFolderViewWidthActionTriggered(true);
        QCOMPARE(qApp->FolderViewWidth(), viewer.folderWindow()->width());

        viewer.folderWindow()->resize(460, viewer.folderWindow()->height());
        QCoreApplication::processEvents();
        QCOMPARE(qApp->FolderViewWidth(), viewer.folderWindow()->width());
        viewer.close();
        QCOMPARE(qApp->FolderViewWidth(), 460);
        viewer.handleFolderWindowClosed();
    }

    void passwordProtectedArchiveShowsMessageInImageViewWithoutFolderFallback()
    {
        StartupWindow viewer;
        const QString encryptedPath = QString(FILELOADER_DATAPATH "7z/password.7z");
        const QString validArchivePath = QString(FILELOADER_DATAPATH "deflate-utf8.zip");
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QVERIFY(pageSlider);

        viewer.openPath(validArchivePath);
        QVERIFY(pageSlider->isEnabled());

        viewer.openPath(encryptedPath);

        QCOMPARE(viewer.imageView()->displayedMessage(),
                 QStringLiteral("Cannot Open Archive\n"
                                "This archive is password-protected.\n\n"
                                "%1")
                     .arg(QDir::toNativeSeparators(encryptedPath)));
        QFrame *pageFrame = viewer.findChild<QFrame *>(QStringLiteral("pageFrame"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageFrame);
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageFrame->isHidden());
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("Unavailable"));
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            QVERIFY(qobject_cast<QMessageBox *>(widget) == nullptr);
        }
        QVERIFY(viewer.folderWindow() == nullptr);
        QVERIFY(!qApp->History().contains(encryptedPath));

        viewer.openPath(validArchivePath);
        QVERIFY(viewer.imageView()->displayedMessage().isEmpty());
        QVERIFY(pageSlider->isEnabled());
    }

    void archiveWithoutImagesIsActiveAndShowsCentralError()
    {
        StartupWindow viewer;
        const QString archivePath = QString(FILELOADER_DATAPATH "7z/text.7z");
        viewer.createFolderWindow(true, QFileInfo(archivePath).absolutePath(), false);

        viewer.openPath(archivePath);

        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        QModelIndex archiveIndex;
        for (int row = 0; row < view->model()->rowCount(); ++row) {
            const QModelIndex candidate = view->model()->index(row, 0);
            if (candidate.data().toString() == QFileInfo(archivePath).fileName()) {
                archiveIndex = candidate;
                break;
            }
        }
        QVERIFY(archiveIndex.isValid());
        QVERIFY(archiveIndex.data(FolderItemModel::CurrentVolumeRole).toBool());
        QCOMPARE(viewer.imageView()->displayedMessage(),
                 QStringLiteral("No Viewable Images\n"
                                "No supported images were found in this archive.\n\n"
                                "%1")
                     .arg(QDir::toNativeSeparators(archivePath)));
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("No images"));
        QLabel *statusLabel = viewer.findChild<QLabel *>(QStringLiteral("statusLabel"));
        QVERIFY(statusLabel);
        QVERIFY(statusLabel->text().isEmpty());
    }

    void archiveDefersFolderViewUpdatesUntilFirstPaint()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("book.zip"));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), archivePath));
        QVERIFY(
            QFile::setPermissions(archivePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner));

        const auto cleanup = qScopeGuard([&] {
            QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(archivePath) || QFile::remove(archivePath),
                                     5000);
        });
        StartupWindow viewer;
        viewer.createFolderWindow(true, directory.path(), false);
        QListView *view =
            viewer.folderWindow()->findChild<QListView *>(QStringLiteral("folderView"));
        QVERIFY(view);
        const auto archiveIsCurrent = [view] {
            for (int row = 0; row < view->model()->rowCount(); ++row) {
                const QModelIndex index = view->model()->index(row, 0);
                if (index.data().toString() == QStringLiteral("book.zip")) {
                    return index.data(FolderItemModel::CurrentVolumeRole).toBool();
                }
            }
            return false;
        };
        QVERIFY(!archiveIsCurrent());

        viewer.openPath(archivePath);

        QVERIFY(viewer.viewerSession()->initialImagePaintPending());
        QVERIFY(!archiveIsCurrent());

        viewer.viewerSession()->notifyInitialImagePainted();

        QTRY_VERIFY(!viewer.viewerSession()->initialImagePaintPending());
        QTRY_VERIFY(archiveIsCurrent());
    }

    void emptyFolderShowsSpecificMessageAndKeepsPageBar()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        StartupWindow viewer;

        viewer.openPath(directory.path());

        QCOMPARE(viewer.imageView()->displayedMessage(),
                 QStringLiteral("No Viewable Images\n"
                                "No supported images were found in this folder.\n\n"
                                "%1")
                     .arg(QDir::toNativeSeparators(directory.path())));
        QFrame *pageFrame = viewer.findChild<QFrame *>(QStringLiteral("pageFrame"));
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageFrame);
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageFrame->isHidden());
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("No images"));
    }

    void brokenImageShowsPathButKeepsVolumeNavigation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString imagePath = directory.filePath(QStringLiteral("broken.png"));
        QFile image(imagePath);
        QVERIFY(image.open(QIODevice::WriteOnly));
        QCOMPARE(image.write("not an image"), qint64(12));
        image.close();
        StartupWindow viewer;

        viewer.openPath(imagePath);

        const QString expected = QStringLiteral("Cannot Display Image\n"
                                                "The image could not be decoded.\n\n"
                                                "%1")
                                     .arg(QDir::toNativeSeparators(imagePath));
        QTRY_COMPARE(viewer.imageView()->displayedMessage(), expected);
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QTRY_VERIFY(pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("(1/1)"));
    }

    void corruptArchiveShowsPathAndDisablesPageBar()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("broken.zip"));
        QFile archive(archivePath);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        QCOMPARE(archive.write("not an archive"), qint64(14));
        archive.close();
        StartupWindow viewer;

        viewer.openPath(archivePath);

        const QString message = viewer.imageView()->displayedMessage();
        QVERIFY(message.startsWith(QStringLiteral("Cannot Open Archive\n")));
        QVERIFY(message.contains(QDir::toNativeSeparators(archivePath)));
        QSlider *pageSlider = viewer.findChild<QSlider *>(QStringLiteral("pageSlider"));
        QLabel *pageLabel = viewer.findChild<QLabel *>(QStringLiteral("pageLabel"));
        QVERIFY(pageSlider);
        QVERIFY(pageLabel);
        QVERIFY(!pageSlider->isEnabled());
        QCOMPARE(pageLabel->text(), QStringLiteral("Unavailable"));
    }

    void backgroundPasswordFailureWaitsForForegroundAttempt()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString firstPath = directory.filePath("001.zip");
        const QString secondPath = directory.filePath("002.zip");
        const QString encryptedPath = directory.filePath("003.zip");
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), firstPath));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "deflate-utf8.zip"), secondPath));
        QVERIFY(QFile::copy(QString(FILELOADER_DATAPATH "zip/encrypted.zip"), encryptedPath));

        const auto cleanup = qScopeGuard([&] {
            for (const auto &path : {firstPath, secondPath, encryptedPath}) {
                QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(path) || QFile::remove(path), 5000);
            }
        });
        StartupWindow viewer;

        viewer.openPath(firstPath);
        QVERIFY(viewer.viewerSession()->nextVolume());
        QVERIFY(viewer.imageView()->displayedMessage().isEmpty());

        QVERIFY(!viewer.viewerSession()->nextVolume());

        QVERIFY(!viewer.imageView()->displayedMessage().isEmpty());
        QVERIFY(!qApp->History().contains(encryptedPath));
    }
};

int main(int argc, char **argv)
{
    if (isFolderTextHelper(argc, argv)) {
        return runFolderTextHelper(argc, argv);
    }

    QStandardPaths::setTestModeEnabled(true);
    // Keep QtTest arguments out of the application's startup file loader.
    int applicationArgc = 1;
    char *applicationArgv[] = {argv[0], nullptr};
    QVApplication application(applicationArgc, applicationArgv);
    qRegisterMetaType<OpenTarget>("OpenTarget");
    WindowStartupTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_windowstartuptest.moc"
