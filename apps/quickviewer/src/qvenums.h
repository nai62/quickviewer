#ifndef QVENUMS_H
#define QVENUMS_H

#include <QtCore>

class qvEnums : public QObject
{
    Q_OBJECT
public:
    qvEnums(QObject *parent)
        : QObject(parent)
    {}

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
    Q_ENUM(ShaderEffect)

    enum CatalogViewMode {
        List,
        Icon,
        IconNoText,
    };
    Q_ENUM(CatalogViewMode)

    enum FolderViewSort {
        OrderByName,
        OrderByUpdatedAt,
    };
    Q_ENUM(FolderViewSort)

    enum ToolbarIconSize {
        NormalIcon = 24,
        LargeIcon = 32,
        Large2Icon = 40,
    };
    Q_ENUM(ToolbarIconSize)

    enum FitMode {
        NoFitting,
        FitToRect,
        FitToWidth
    };
    Q_ENUM(FitMode)

    enum SvgLoaderBackend {
        Resvg,
        QtSvg,
    };
    Q_ENUM(SvgLoaderBackend)

    enum OptionViewOnStartup {
        NoViewStartup,
        FolderStartup,
        CatalogStartup,
        RetouchStartup,
        //        ExifStartup,
    };
    Q_ENUM(OptionViewOnStartup)

    enum ImageSortBy {
        SortByFileName,
        SortByFileNameDescending,
        SortByFileSize,
        SortByFileSizeDescending,
        SortByModifiedTime,
        SortByModifiedTimeDescending,
    };
    Q_ENUM(ImageSortBy)
};

#endif // QVENUMS_H
