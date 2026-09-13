include(../../QVproject.pri)

RESVG_SOURCE_ROOT = $$clean_path($$PWD/../../resvg/resvg)
include(../../resvg/resvg.pri)

QT += testlib core gui svg

TARGET = tst_svgloadertest
TEMPLATE = app
CONFIG += console testcase
CONFIG -= app_bundle

INCLUDEPATH += \
    ../../QuickViewer/src \
    ../../QuickViewer/src/models

SOURCES += \
    tst_svgloadertest.cpp \
    ../../QuickViewer/src/models/svgloader.cpp

HEADERS += \
    ../../QuickViewer/src/qv_init.h \
    ../../QuickViewer/src/models/svgloader.h

DESTDIR = ../../lib
