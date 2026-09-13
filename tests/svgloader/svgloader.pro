include(../../QVproject.pri)

RESVG_SOURCE_ROOT = $$clean_path($$PWD/../../third_party/resvg)
include(../../qmake/third_party/resvg/resvg.pri)

QT += testlib core gui svg

TARGET = tst_svgloadertest
TEMPLATE = app
CONFIG += console testcase
CONFIG -= app_bundle

INCLUDEPATH += \
    ../../apps/quickviewer/src \
    ../../apps/quickviewer/src/models

SOURCES += \
    tst_svgloadertest.cpp \
    ../../apps/quickviewer/src/models/svgloader.cpp

HEADERS += \
    ../../apps/quickviewer/src/qv_init.h \
    ../../apps/quickviewer/src/models/svgloader.h

DESTDIR = ../../lib
