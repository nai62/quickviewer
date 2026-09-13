#ifndef VIEWERSTATE_H
#define VIEWERSTATE_H

#include <optional>
#include <variant>

#include <QtGlobal>

#include "volumehandle.h"

struct EmptyViewerState
{
};

struct LoadingViewerState
{
    quint64 generation = 0;
};

struct StandalonePreviewViewerState
{
    quint64 generation = 0;
    bool imageReadyForPaint = false;
    bool paintCompletionQueued = false;
    bool folderScanStarted = false;
};

struct InitialPaintDeferral
{
    quint64 generation = 0;
    bool completionQueued = false;
};

struct VolumeReadyViewerState
{
    VolumeHandle volume;
    std::optional<InitialPaintDeferral> initialPaintDeferral;
};

struct FailedViewerState
{
};

using ViewerState = std::variant<EmptyViewerState,
                                 LoadingViewerState,
                                 StandalonePreviewViewerState,
                                 VolumeReadyViewerState,
                                 FailedViewerState>;

enum class ViewerStateKind {
    Empty,
    Loading,
    StandalonePreview,
    VolumeReady,
    Failed
};

inline ViewerStateKind viewerStateKind(const ViewerState &state)
{
    if (std::holds_alternative<LoadingViewerState>(state)) {
        return ViewerStateKind::Loading;
    }
    if (std::holds_alternative<StandalonePreviewViewerState>(state)) {
        return ViewerStateKind::StandalonePreview;
    }
    if (std::holds_alternative<VolumeReadyViewerState>(state)) {
        return ViewerStateKind::VolumeReady;
    }
    if (std::holds_alternative<FailedViewerState>(state)) {
        return ViewerStateKind::Failed;
    }
    return ViewerStateKind::Empty;
}

#endif // VIEWERSTATE_H
