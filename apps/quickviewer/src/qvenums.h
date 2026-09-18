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

enum ShaderEffect {
    UnPrepared,
    CpuBicubic,
    CpuSpline16,
    CpuSpline36,
    CpuLanczos3,
    CpuLanczos4,

    UsingFixedShader,
    NearestNeighbor,
    Bilinear,

    UsingCpuResizer,
    BilinearAndCpuBicubic,
    BilinearAndCpuSpline16,
    BilinearAndCpuSpline36,
    BilinearAndCpuLanczos,

    UsingSomeShader,
#ifndef QV_WITHOUT_OPENGL
    Bicubic,
    Lanczos
#endif
};
Q_ENUM_NS(ShaderEffect)

enum CatalogViewMode {
    List,
    Icon,
    IconNoText,
};
Q_ENUM_NS(CatalogViewMode)

enum ToolbarIconSize {
    NormalIcon = 24,
    Large2Icon = 40,
};
Q_ENUM_NS(ToolbarIconSize)

enum FitMode {
    NoFitting,
    FitToRect,
    FitToWidth
};
Q_ENUM_NS(FitMode)

enum SvgLoaderBackend {
    Resvg,
    QtSvg,
};
Q_ENUM_NS(SvgLoaderBackend)

enum OptionViewOnStartup {
    NoViewStartup,
    FolderStartup,
    CatalogStartup,
    RetouchStartup,
    //        ExifStartup,
};
Q_ENUM_NS(OptionViewOnStartup)

enum ImageSortBy {
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
