include(../../QVproject.pri)

QT += testlib core

TARGET = tst_prefetchplannertest
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

INCLUDEPATH += ../../apps/quickviewer/src/models

SOURCES += \
    tst_prefetchplannertest.cpp \
    ../../apps/quickviewer/src/models/prefetchplanner.cpp

HEADERS += \
    ../../apps/quickviewer/src/models/prefetchplanner.h

DESTDIR = ../../lib
