include(../../QVproject.pri)

QT += testlib core concurrent

TARGET = tst_asynccachetest
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    tst_asynccachetest.cpp \
    ../../apps/quickviewer/src/models/boundedexecutor.cpp

HEADERS += \
    ../../apps/quickviewer/src/models/boundedexecutor.h \
    ../../apps/quickviewer/src/models/lrucache.h

INCLUDEPATH += ../../apps/quickviewer/src/models

DESTDIR = ../../lib
