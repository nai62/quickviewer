include(../../QVproject.pri)

QT += testlib core concurrent

TARGET = tst_asynccachetest
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    tst_asynccachetest.cpp \
    ../../QuickViewer/src/models/boundedexecutor.cpp

HEADERS += \
    ../../QuickViewer/src/models/boundedexecutor.h \
    ../../QuickViewer/src/models/lrucache.h

INCLUDEPATH += ../../QuickViewer/src/models

DESTDIR = ../../lib
