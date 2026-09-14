include("QVproject.pri")

TEMPLATE = subdirs
SUBDIRS = \
    ResizeHalf \
    easyexif \
    unrar \
    fileloader \
    zimg \
    spng \
    QuickViewer

CONFIG(debug, debug|release) {
    SUBDIRS += \
        qvtest \
        prefetchplannertest \
        latestresultdispatchertest \
        asynccachetest \
        svgloadertest \
        viewernavigationtest \
        windowstartuptest
}

ResizeHalf.file = qmake/third_party/resizehalf/ResizeHalf.pro
easyexif.file = qmake/third_party/easyexif/easyexif.pro
unrar.file = components/rarextractor/unrar.pro
fileloader.file = components/fileloader/fileloader.pro
zimg.file = components/qzimg/zimg.pro
spng.file = qmake/third_party/spng/spng.pro
QuickViewer.file = apps/quickviewer/QuickViewer.pro
qvtest.file = tests/fileloader/qvtest.pro
prefetchplannertest.file = tests/prefetchplanner/prefetchplanner.pro
latestresultdispatchertest.file = tests/latestresultdispatcher/latestresultdispatcher.pro
asynccachetest.file = tests/asynccache/asynccache.pro
svgloadertest.file = tests/svgloader/svgloader.pro
viewernavigationtest.file = tests/viewernavigation/viewernavigation.pro
windowstartuptest.file = tests/windowstartup/windowstartup.pro

fileloader.depends = unrar
QuickViewer.depends = ResizeHalf easyexif fileloader zimg spng
qvtest.depends = fileloader
viewernavigationtest.depends = ResizeHalf easyexif fileloader zimg spng
windowstartuptest.depends = ResizeHalf easyexif fileloader zimg spng

contains(DEFINES, QV_WITH_LUMINOR) {
    SUBDIRS += luminor
    luminor.file = components/qluminor/luminor.pro
    QuickViewer.depends += luminor
    windowstartuptest.depends += luminor
}

win32 {
    SUBDIRS += AssociateFilesWithQuickViewer
    AssociateFilesWithQuickViewer.file = apps/associate-files/AssociateFilesWithQuickViewer.pro
}


CODECFORSRC = UTF-8

TRANSLATIONS = \
    apps/quickviewer/translations/quickviewer_ja.ts \
    apps/quickviewer/translations/quickviewer_es.ts \
    apps/quickviewer/translations/quickviewer_zh.ts \
    apps/quickviewer/translations/quickviewer_el.ts \
    apps/quickviewer/translations/quickviewer_fr.ts \
    apps/quickviewer/translations/quickviewer_ru.ts \
    apps/quickviewer/translations/quickviewer_ar.ts \

DISTFILES += \
    apps/quickviewer/translations/quickviewer_ja.qm \
