include("QVproject.pri")

TEMPLATE = subdirs
SUBDIRS = \
    Qt7z \
    ResizeHalf \
    easyexif \
    unrar \
    fileloader \
    zimg \
    spng \
    QuickViewer \
    qvtest \
    prefetchplannertest \
    latestresultdispatchertest \
    asynccachetest \
    svgloadertest \
    viewernavigationtest \
    windowstartuptest

Qt7z.subdir = components/qt7z/Qt7z.pro
ResizeHalf.subdir = qmake/third_party/resizehalf/ResizeHalf.pro
easyexif.subdir = qmake/third_party/easyexif/easyexif.pro
unrar.subdir = components/rarextractor/unrar.pro
fileloader.subdir = components/fileloader/fileloader.pro
zimg.subdir = components/qzimg/zimg.pro
spng.subdir = qmake/third_party/spng/spng.pro
QuickViewer.subdir = apps/quickviewer/QuickViewer.pro
qvtest.subdir = tests/fileloader/qvtest.pro
prefetchplannertest.subdir = tests/prefetchplanner/prefetchplanner.pro
latestresultdispatchertest.subdir = tests/latestresultdispatcher/latestresultdispatcher.pro
asynccachetest.subdir = tests/asynccache/asynccache.pro
svgloadertest.subdir = tests/svgloader/svgloader.pro
viewernavigationtest.subdir = tests/viewernavigation/viewernavigation.pro
windowstartuptest.subdir = tests/windowstartup/windowstartup.pro

fileloader.depends = Qt7z unrar
QuickViewer.depends = ResizeHalf easyexif fileloader zimg spng
qvtest.depends = fileloader
viewernavigationtest.depends = ResizeHalf easyexif fileloader zimg spng
windowstartuptest.depends = ResizeHalf easyexif fileloader zimg spng

contains(DEFINES, QV_WITH_LUMINOR) {
    SUBDIRS += luminor
    luminor.subdir = components/qluminor/luminor.pro
    QuickViewer.depends += luminor
    windowstartuptest.depends += luminor
}

win32 {
    SUBDIRS += AssociateFilesWithQuickViewer
    AssociateFilesWithQuickViewer.subdir = apps/associate-files/AssociateFilesWithQuickViewer.pro
}

unix {
#    SUBDIRS += qmake/third_party/lib7z/lib7z.pro
#    fileloader.depends += qmake/third_party/lib7z/lib7z.pro
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
