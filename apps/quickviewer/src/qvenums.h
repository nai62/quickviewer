#ifndef QVENUMS_H
#define QVENUMS_H

#include <QtCore>

/**
 * Enumerations shared by the application. The namespace carries the meta
 * object so that the values can be stored in and read from the settings by
 * name.
 */
namespace qvEnums {
Q_NAMESPACE

enum class ShaderEffect {
    UnPrepared,
    CpuBicubic,
    CpuSpline16,
    CpuSpline36,
    CpuLanczos3,
    CpuLanczos4,

    NearestNeighbor,
    Bilinear,
    Bicubic,
    Lanczos
};
Q_ENUM_NS(ShaderEffect)

enum class CatalogViewMode {
    List,
    Icon,
    IconNoText,
};
Q_ENUM_NS(CatalogViewMode)

enum class FolderViewSort {
    OrderByName,
    OrderByUpdatedAt,
};
Q_ENUM_NS(FolderViewSort)

enum class ToolbarIconSize {
    NormalIcon = 24,
    LargeIcon = 32,
    Large2Icon = 40,
};
Q_ENUM_NS(ToolbarIconSize)

enum class FitMode {
    NoFitting,
    FitToRect,
    FitToWidth
};
Q_ENUM_NS(FitMode)

enum class SvgLoaderBackend {
    Resvg,
    QtSvg,
};
Q_ENUM_NS(SvgLoaderBackend)

enum class OptionViewOnStartup {
    NoViewStartup,
    FolderStartup,
    CatalogStartup,
    RetouchStartup,
    //        ExifStartup,
};
Q_ENUM_NS(OptionViewOnStartup)

enum class ImageSortBy {
    SortByFileName,
    SortByFileNameDescending,
    SortByFileSize,
    SortByFileSizeDescending,
    SortByModifiedTime,
    SortByModifiedTimeDescending,
};
Q_ENUM_NS(ImageSortBy)

} // namespace qvEnums

#endif // QVENUMS_H
