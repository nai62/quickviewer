#include "foldertextcache.h"
#include <QtCore>
#include <QtWidgets>

#include "benchmark/imagebenchmarkrunner.h"

#include "qvapplication.h"
#include "thumbnailmanager.h"
#include "qnamedpipe.h"
#include "startupprofiler.h"
#include "volume.h"

#if defined(Q_OS_WIN)
#    include "mainwindowforwindows.h"
#else
#    include "mainwindow.h"
#endif

#ifdef Q_OS_WIN
#    include <Windows.h>
#endif

namespace {
class ImageLoadingShutdownGuard
{
public:
    ~ImageLoadingShutdownGuard() { Volume::shutdownImageLoading(); }
};

/**
 * Reports the first paint of the empty-window benchmark child. That child stops
 * there, so its profile shows how much of a first paint belongs to Qt and
 * Windows rather than to QuickViewer's startup work.
 */
class EmptyWindowPaintReporter : public QWidget
{
public:
    explicit EmptyWindowPaintReporter(QWidget *parent = nullptr)
        : QWidget(parent)
    {
    }

    bool hasReportedFirstPaint() const { return m_reported; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_reported) {
            return;
        }
        m_reported = true;
        StartupProfiler::mark("first-image-painted");
        StartupProfiler::flush();
    }

private:
    bool m_reported = false;
};

// A child that cannot paint must not hold the benchmark for its full timeout.
constexpr int EmptyWindowDeadlineMilliseconds = 15000;

/**
 * Startup measured by the empty-window suite: a plain QApplication and one bare
 * window, with the same winId, show, and event steps the real startup performs.
 */
int runEmptyWindowChild(int argc, char **argv)
{
    StartupProfiler::mark("application.construct.begin");
    QApplication app(argc, argv);
    StartupProfiler::mark("application.construct.end");
    StartupProfiler::mark("application.constructed");

    EmptyWindowPaintReporter window;
    window.resize(800, 600);
    // The first winId() call creates the native window, in this child as in the
    // QuickViewer startup that cloaks the window before showing it.
    StartupProfiler::mark("startup.cloak.before-winid");
    (void)window.winId();
    StartupProfiler::mark("startup.cloak.after-winid");

    StartupProfiler::mark("startup.show.begin");
    window.show();
    StartupProfiler::mark("startup.show.end");
    StartupProfiler::mark("startup.window-shown");

    StartupProfiler::mark("startup.process-events.begin");
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    StartupProfiler::mark("startup.process-events.end");
    StartupProfiler::mark("startup.initial-events-processed");

    // Stopping from inside a paint event does not end the event loop, and the
    // paint can arrive before this loop starts, so the first paint is polled
    // from a timer that runs inside the loop.
    QTimer firstPaintPoll;
    QObject::connect(&firstPaintPoll, &QTimer::timeout, &app, [&window, &app] {
        if (window.hasReportedFirstPaint()) {
            app.exit(0);
        }
    });
    firstPaintPoll.start(10);
    QTimer::singleShot(EmptyWindowDeadlineMilliseconds, &app, [] {
        StartupProfiler::mark("empty-window.deadline");
        StartupProfiler::flush();
        QCoreApplication::exit(2);
    });
    const int result = app.exec();
    StartupProfiler::flush();
    return result;
}
} // namespace

int main(int argc, char *argv[])
{
    if (isFolderTextHelper(argc, argv)) {
        return runFolderTextHelper(argc, argv);
    }

    StartupProfiler::start();
#ifdef Q_OS_WIN
    {
        // Activate the direct2d QPA plugin when 'UseDirect2D' of quickviewer.ini is true.
        // Since initialization of QPA is processed in the constructor of QGuiApplication,
        // it must be set to the QT_QPA_PLATFORM environment variable before that.
#    ifdef QV_PORTABLE
        QString inipath = QDir::toNativeSeparators(QFileInfo(argv[0]).path());
        inipath += QStringLiteral("\\") + QVApplication::settingsSubPath();
#    else
        QString inipath = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation))
                              .filePath(QVApplication::settingsSubPath());
#    endif
        std::wstring winipath = inipath.toStdWString();
        WCHAR value[128];
        qDebug() << ::GetPrivateProfileString(
            L"View", L"UseDirect2D", L"false", value, sizeof(value) - 1, winipath.c_str());
        if (::lstrcmp(value, L"true") == 0) {
            qputenv("QT_QPA_PLATFORM", "direct2d");
        }
    }
#elif defined(Q_OS_MAC)
    {
        // hide icons on application menu.
        QApplication::instance()->setAttribute(Qt::AA_DontShowIconsInMenus, true);
    }
#endif

    if (ImageBenchmarkRunner::isEmptyWindowChildRequested()) {
        return runEmptyWindowChild(argc, argv);
    }

    StartupProfiler::mark("application.construct.begin");
    QVApplication app(argc, argv);
    StartupProfiler::mark("application.construct.end");
    ImageLoadingShutdownGuard imageLoadingShutdownGuard;
    ImageBenchmarkRunner::applyStartupOverrides();
    if (ImageBenchmarkRunner::isRequested(app.arguments())) {
        return ImageBenchmarkRunner::run(app.arguments());
    }

    StartupProfiler::mark("application.constructed");
    app.setEffectEnabled(Qt::UI_AnimateCombo, false);
    app.myInstallTranslator();
    int result = 0;
    {
        QNamedPipe pipe(app.applicationName(), qApp->ProhibitMultipleRunning());
        if (!pipe.isServerMode()) {
            qDebug() << app.arguments();
            if (app.arguments().length() > 1) {
                pipe.send(app.arguments()[1].toUtf8());
            } else {
                pipe.send("b");
            }
            return 0;
        }

        StartupProfiler::mark("mainwindow.construct.begin");
#ifdef Q_OS_WIN
        MainWindowForWindows w;
#else
        ArchiveAwareMainWindow w;
#endif
        StartupProfiler::mark("mainwindow.construct.end");
        StartupProfiler::mark("mainwindow.constructed");
        w.initializeStartup();
        StartupProfiler::mark("startup.initialized");
        QString dbpath = app.CatalogDatabasePath();
        ThumbnailManager manager(&w, dbpath);
        StartupProfiler::mark("thumbnail-manager.constructed");
        w.setThumbnailManager(&manager);
        StartupProfiler::mark("thumbnail-manager.attached");
        w.connect(&pipe, &QNamedPipe::received, &w, [&](QByteArray bytes) {
            if (bytes.size() == 1) {
                w.setWindowTop(false);
            } else if (bytes.size() > 0) {
                auto string = QString::fromUtf8(bytes);
                w.loadVolumeWithAssoc(string);
            }
        });
        StartupProfiler::mark("event-loop.begin");
        result = app.exec();
        Volume::shutdownImageLoading();
    }
    return result;
}
