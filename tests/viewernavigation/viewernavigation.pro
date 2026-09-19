include(../../QVproject.pri)
RESVG_SOURCE_ROOT = $$clean_path($$PWD/../../third_party/resvg)
include(../../qmake/third_party/resvg/resvg.pri)

QT += core gui widgets concurrent sql svgwidgets testlib

TARGET = tst_viewernavigationtest
TEMPLATE = app
CONFIG += console testcase
CONFIG -= app_bundle

VERSION = 1.2.8
DEFINES += \
    NOMINMAX \
    APP_VERSION=\\\"$$VERSION\\\" \
    APP_NAME=\\\"QuickViewerTest\\\" \
    VIEWERNAVIGATION_SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += \
    ../../apps/quickviewer/src \
    ../../apps/quickviewer/src/catalog \
    ../../apps/quickviewer/src/widgets \
    ../../apps/quickviewer/src/models \
    ../../apps/quickviewer/src/folderview \
    ../../components/i18n \
    ../../apps/quickviewer/src/qnamedpipe \
    ../../apps/quickviewer/src/qactionmanager \
    ../../third_party/resizehalf \
    ../../third_party/easyexif \
    ../../components/fileloader \
    ../../components/qzimg \
    ../../third_party/libspng/spng \
    ../../components/file-association

SOURCES += \
    tst_viewernavigationtest.cpp \
    ../../apps/quickviewer/src/imageview.cpp \
    ../../apps/quickviewer/src/models/readprogressstore.cpp \
    ../../apps/quickviewer/src/models/boundedexecutor.cpp \
    ../../apps/quickviewer/src/models/imagestring.cpp \
    ../../apps/quickviewer/src/models/loupecontroller.cpp \
    ../../apps/quickviewer/src/models/imagecontent.cpp \
    ../../apps/quickviewer/src/models/pagedisplayformatter.cpp \
    ../../apps/quickviewer/src/models/renderedpage.cpp \
    ../../apps/quickviewer/src/models/renderedpages.cpp \
    ../../apps/quickviewer/src/models/visiblepagecomposer.cpp \
    ../../apps/quickviewer/src/models/viewersession.cpp \
    ../../apps/quickviewer/src/models/prefetchplanner.cpp \
    ../../apps/quickviewer/src/models/qvapplication.cpp \
    ../../apps/quickviewer/src/models/imagemetadata.cpp \
    ../../apps/quickviewer/src/models/movie.cpp \
    ../../apps/quickviewer/src/models/shadermanager.cpp \
    ../../apps/quickviewer/src/models/shadereffect.cpp \
    ../../apps/quickviewer/src/models/svgloader.cpp \
    ../../apps/quickviewer/src/startupprofiler.cpp \
    ../../apps/quickviewer/src/models/volumecache.cpp \
    ../../apps/quickviewer/src/models/volumehandle.cpp \
    ../../apps/quickviewer/src/models/volume.cpp \
    ../../apps/quickviewer/src/models/imagedecoder.cpp \
    ../../apps/quickviewer/src/models/jpegorientation.cpp \
    ../../apps/quickviewer/src/models/volumeloader.cpp \
    ../../apps/quickviewer/src/models/volumelocation.cpp \
    ../../apps/quickviewer/src/models/storedvolumelocation.cpp \
    ../../apps/quickviewer/src/qactionmanager/keyconfigdialog.cpp \
    ../../apps/quickviewer/src/qactionmanager/mouseconfigdialog.cpp \
    ../../apps/quickviewer/src/qactionmanager/qactionmanager.cpp \
    ../../apps/quickviewer/src/qactionmanager/qmousesequence.cpp \
    ../../apps/quickviewer/src/qactionmanager/shortcutbutton.cpp \
    ../../components/i18n/languagemanager.cpp \
    ../../components/i18n/texttranslator.cpp

HEADERS += \
    ../../apps/quickviewer/src/qvenums.h \
    ../../apps/quickviewer/src/imageview.h \
    ../../apps/quickviewer/src/models/readprogressstore.h \
    ../../apps/quickviewer/src/models/boundedexecutor.h \
    ../../apps/quickviewer/src/models/imagestring.h \
    ../../apps/quickviewer/src/models/latestresultdispatcher.h \
    ../../apps/quickviewer/src/models/lrucache.h \
    ../../apps/quickviewer/src/models/loupecontroller.h \
    ../../apps/quickviewer/src/models/pagenavigator.h \
    ../../apps/quickviewer/src/models/imagecontent.h \
    ../../apps/quickviewer/src/models/pagedisplayformatter.h \
    ../../apps/quickviewer/src/models/renderedpage.h \
    ../../apps/quickviewer/src/models/visiblepagecomposer.h \
    ../../apps/quickviewer/src/models/viewersession.h \
    ../../apps/quickviewer/src/models/prefetchplanner.h \
    ../../apps/quickviewer/src/models/qvapplication.h \
    ../../apps/quickviewer/src/models/imagemetadata.h \
    ../../apps/quickviewer/src/models/movie.h \
    ../../apps/quickviewer/src/models/renderedpages.h \
    ../../apps/quickviewer/src/models/renderedpagemetrics.h \
    ../../apps/quickviewer/src/models/shadermanager.h \
    ../../apps/quickviewer/src/models/shadereffect.h \
    ../../apps/quickviewer/src/models/svgloader.h \
    ../../apps/quickviewer/src/startupprofiler.h \
    ../../apps/quickviewer/src/models/volumecache.h \
    ../../apps/quickviewer/src/models/volumehandle.h \
    ../../apps/quickviewer/src/models/visiblepages.h \
    ../../apps/quickviewer/src/models/viewerstate.h \
    ../../apps/quickviewer/src/models/volume.h \
    ../../apps/quickviewer/src/models/imagedecoder.h \
    ../../apps/quickviewer/src/models/jpegorientation.h \
    ../../apps/quickviewer/src/models/decodemetricsscope.h \
    ../../apps/quickviewer/src/models/volumeloader.h \
    ../../apps/quickviewer/src/models/volumelocation.h \
    ../../apps/quickviewer/src/models/storedvolumelocation.h \
    ../../apps/quickviewer/src/qactionmanager/keyconfigdialog.h \
    ../../apps/quickviewer/src/qactionmanager/mouseconfigdialog.h \
    ../../apps/quickviewer/src/qactionmanager/qactionmanager.h \
    ../../apps/quickviewer/src/qactionmanager/qmousesequence.h \
    ../../apps/quickviewer/src/qactionmanager/shortcutbutton.h \
    ../../components/i18n/languagemanager.h \
    ../../components/i18n/texttranslator.h

FORMS += \
    ../../apps/quickviewer/src/mainwindow.ui \
    ../../apps/quickviewer/src/qactionmanager/keyconfigdialog.ui

DESTDIR = ../../lib
LIBS += -L../../lib -leasyexif -lresizehalf -lfileloader -lunrar -lzimg -lspng

contains(DEFINES, QV_WITH_LUMINOR) {
    INCLUDEPATH += ../../components/qluminor
    win32: LIBS += -L$$PWD/../../third_party/luminor/$${LUMINOR_BIN_PATH} -lluminor -lluminor_rgba -lhalide_runtime -lqluminor
}

win32: LIBS += -luser32 -ladvapi32 -lshell32 -lShlwapi -loleaut32 -lole32 -luuid

win32 {
    # The archive fixtures need the official 7z.dll next to the test binary.
    QMAKE_POST_LINK += $$QMAKE_COPY /B $$shell_quote($$shell_path($$PWD/../../third_party/7zip/windll/$${TARGET_ARCH}/7z.dll)) $$shell_path($${DESTDIR}) $$escape_expand(\n\t)
}
