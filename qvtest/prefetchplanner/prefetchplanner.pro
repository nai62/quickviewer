include(../../QVproject.pri)

QT += testlib core

TARGET = tst_prefetchplannertest
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

INCLUDEPATH += ../../QuickViewer/src/models

SOURCES += \
    tst_prefetchplannertest.cpp \
    ../../QuickViewer/src/models/prefetchplanner.cpp

HEADERS += \
    ../../QuickViewer/src/models/prefetchplanner.h

DESTDIR = ../../lib
