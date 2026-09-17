#ifndef VIEWERLOADSTATUS_H
#define VIEWERLOADSTATUS_H

#include <QtCore>

enum class LoadTargetKind {
    Unknown,
    ImageFile,
    Folder,
    Archive,
};

enum class LoadFailureReason {
    None,
    NoViewableImages,
    NotFound,
    PermissionDenied,
    DecodeFailed,
    PasswordProtected,
    UnsupportedFormat,
    CorruptData,
    IoError,
};

enum class ViewerLoadPhase {
    Empty,
    Loading,
    Ready,
    Failed,
};

struct ViewerLoadStatus
{
    ViewerLoadPhase phase = ViewerLoadPhase::Empty;
    LoadTargetKind targetKind = LoadTargetKind::Unknown;
    LoadFailureReason failureReason = LoadFailureReason::None;
    QString requestedPath;
    QString failurePath;
};

#endif // VIEWERLOADSTATUS_H
