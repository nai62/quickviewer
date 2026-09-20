#include "startupfoldertextprofile.h"

#include <QtWidgets>

#include "folderitemmodel.h"
#include "startupprofiler.h"

namespace {
constexpr int HeartbeatMilliseconds = 16;
constexpr int TimeoutMilliseconds = 30000;
} // namespace

void StartupFolderTextProfile::watch(QWidget *window, ModelLookup model)
{
    // Start at reveal, including failed/empty archives which never emit an
    // initial-image completion. Observe the real post-reveal event loop.
    auto *timer = new QTimer(window);
    auto elapsed = QSharedPointer<QElapsedTimer>::create();
    elapsed->start();
    QObject::connect(timer, &QTimer::timeout, window, [window, timer, elapsed, model] {
        StartupProfiler::mark("folder-text.gui-heartbeat");
        FolderItemModel *itemModel = model ? model() : nullptr;
        if (itemModel) {
            itemModel->requestTextImages();
        }
        if (itemModel && itemModel->textImagesPending() &&
            elapsed->elapsed() < TimeoutMilliseconds) {
            return;
        }
        StartupProfiler::mark(elapsed->elapsed() >= TimeoutMilliseconds
                                  ? "folder-text.profile-timeout"
                                  : "folder-text.final-paint.begin");
        window->repaint();
        StartupProfiler::mark("folder-text.final-paint.end");
        timer->stop();
        timer->deleteLater();
        StartupProfiler::flush();
        QCoreApplication::quit();
    });
    timer->start(HeartbeatMilliseconds);
}
