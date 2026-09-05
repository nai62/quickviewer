include("QVproject.pri")

TEMPLATE = subdirs
SUBDIRS = \
    Qt7z/Qt7z \
    ResizeHalf \
    easyexif \
    unrar \
    fileloader \
    zimg \
    qsvgrenderer \
    QuickViewer \
    qvtest \
    prefetchplannertest \
    latestresultdispatchertest \
    asynccachetest \
    viewernavigationtest \
    windowstartuptest

prefetchplannertest.subdir = qvtest/prefetchplanner
latestresultdispatchertest.subdir = qvtest/latestresultdispatcher
asynccachetest.subdir = qvtest/asynccache
viewernavigationtest.subdir = qvtest/viewernavigation
windowstartuptest.subdir = qvtest/windowstartup

fileloader.depends = Qt7z/Qt7z unrar
QuickViewer.depends = ResizeHalf easyexif fileloader zimg qsvgrenderer
qvtest.depends = fileloader
viewernavigationtest.depends = ResizeHalf easyexif fileloader zimg qsvgrenderer
windowstartuptest.depends = ResizeHalf easyexif fileloader zimg qsvgrenderer

contains(DEFINES, QV_WITH_LUMINOR) {
    SUBDIRS += luminor
    QuickViewer.depends += luminor
    windowstartuptest.depends += luminor
}

win32 {
    SUBDIRS += AssociateFilesWithQuickViewer
}

unix {
#    SUBDIRS += Qt7z/lib7z/lib7z.pro
#    fileloader.depends += Qt7z/lib7z/lib7z.pro
}


CODECFORSRC = UTF-8

TRANSLATIONS = \
    QuickViewer/translations/quickviewer_en.ts \
    QuickViewer/translations/quickviewer_ja.ts \
    QuickViewer/translations/quickviewer_es.ts \
    QuickViewer/translations/quickviewer_zh.ts \
    QuickViewer/translations/quickviewer_el.ts \
    QuickViewer/translations/quickviewer_fr.ts \
    QuickViewer/translations/quickviewer_ru.ts \
    QuickViewer/translations/quickviewer_ar.ts \

DISTFILES += \
    QuickViewer/translations/quickviewer_ja.qm \
