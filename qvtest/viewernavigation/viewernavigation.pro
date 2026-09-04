include(../../QVproject.pri)

QT += core gui widgets concurrent sql svgwidgets testlib

TARGET = tst_viewernavigationtest
TEMPLATE = app
CONFIG += console testcase
CONFIG -= app_bundle

VERSION = 1.2.8
DEFINES += \
    NOMINMAX \
    APP_VERSION=\\\"$$VERSION\\\" \
    APP_NAME=\\\"QuickViewerTest\\\"

INCLUDEPATH += \
    ../../QuickViewer/src \
    ../../QuickViewer/src/catalog \
    ../../QuickViewer/src/widgets \
    ../../QuickViewer/src/models \
    ../../QuickViewer/src/folderview \
    ../../QuickViewer/src/qfullscreenframe \
    ../../QuickViewer/src/qlanguageselector \
    ../../QuickViewer/src/qnamedpipe \
    ../../QuickViewer/src/qactionmanager \
    ../../ResizeHalf/ResizeHalf \
    ../../easyexif/easyexif \
    ../../fileloader \
    ../../zimg \
    ../../qsvgrenderer/svg-native-viewer/svgnative/include \
    ../../AssociateFilesWithQuickViewer

SOURCES += \
    tst_viewernavigationtest.cpp \
    ../../QuickViewer/src/imageview.cpp \
    ../../QuickViewer/src/models/bookprogressmanager.cpp \
    ../../QuickViewer/src/models/boundedexecutor.cpp \
    ../../QuickViewer/src/models/imagestring.cpp \
    ../../QuickViewer/src/models/loupecontroller.cpp \
    ../../QuickViewer/src/models/pagecontent.cpp \
    ../../QuickViewer/src/models/renderedpages.cpp \
    ../../QuickViewer/src/models/viewersession.cpp \
    ../../QuickViewer/src/models/prefetchplanner.cpp \
    ../../QuickViewer/src/models/qvapplication.cpp \
    ../../QuickViewer/src/models/qvimagemetadata.cpp \
    ../../QuickViewer/src/models/qvmovie.cpp \
    ../../QuickViewer/src/models/shadermanager.cpp \
    ../../QuickViewer/src/models/volumehandle.cpp \
    ../../QuickViewer/src/models/volume.cpp \
    ../../QuickViewer/src/models/volumeloader.cpp \
    ../../QuickViewer/src/qactionmanager/keyconfigdialog.cpp \
    ../../QuickViewer/src/qactionmanager/mouseconfigdialog.cpp \
    ../../QuickViewer/src/qactionmanager/qactionmanager.cpp \
    ../../QuickViewer/src/qactionmanager/qmousesequence.cpp \
    ../../QuickViewer/src/qactionmanager/shortcutbutton.cpp \
    ../../QuickViewer/src/qlanguageselector/qlanguageselector.cpp \
    ../../QuickViewer/src/qlanguageselector/qtexttranslator.cpp

HEADERS += \
    ../../QuickViewer/src/qv_init.h \
    ../../QuickViewer/src/imageview.h \
    ../../QuickViewer/src/models/bookprogressmanager.h \
    ../../QuickViewer/src/models/boundedexecutor.h \
    ../../QuickViewer/src/models/imagestring.h \
    ../../QuickViewer/src/models/latestresultdispatcher.h \
    ../../QuickViewer/src/models/lrucache.h \
    ../../QuickViewer/src/models/loupecontroller.h \
    ../../QuickViewer/src/models/pagecontent.h \
    ../../QuickViewer/src/models/viewersession.h \
    ../../QuickViewer/src/models/prefetchplanner.h \
    ../../QuickViewer/src/models/qvapplication.h \
    ../../QuickViewer/src/models/qvimagemetadata.h \
    ../../QuickViewer/src/models/qvmovie.h \
    ../../QuickViewer/src/models/renderedpages.h \
    ../../QuickViewer/src/models/renderedpagemetrics.h \
    ../../QuickViewer/src/models/shadermanager.h \
    ../../QuickViewer/src/models/volumehandle.h \
    ../../QuickViewer/src/models/visiblepages.h \
    ../../QuickViewer/src/models/viewerstate.h \
    ../../QuickViewer/src/models/volume.h \
    ../../QuickViewer/src/models/volumeloader.h \
    ../../QuickViewer/src/qactionmanager/keyconfigdialog.h \
    ../../QuickViewer/src/qactionmanager/mouseconfigdialog.h \
    ../../QuickViewer/src/qactionmanager/qactionmanager.h \
    ../../QuickViewer/src/qactionmanager/qmousesequence.h \
    ../../QuickViewer/src/qactionmanager/shortcutbutton.h \
    ../../QuickViewer/src/qlanguageselector/qlanguageselector.h \
    ../../QuickViewer/src/qlanguageselector/qtexttranslator.h

FORMS += \
    ../../QuickViewer/src/mainwindow.ui \
    ../../QuickViewer/src/qactionmanager/keyconfigdialog.ui

DESTDIR = ../../lib
LIBS += -L../../lib -leasyexif -lresizehalf -lfileloader -lQt7z -lunrar -lzimg

contains(DEFINES, QV_WITH_LUMINOR) {
    INCLUDEPATH += ../../luminor
    win32: LIBS += -L$$PWD/../../luminor/$${LUMINOR_BIN_PATH} -lluminor -lluminor_rgba -lhalide_runtime -lqluminor
}

win32 {
    LIBS += -luser32 -ladvapi32 -lshell32 -lShlwapi -loleaut32 -lole32 -luuid -lQSVGNative0
}
