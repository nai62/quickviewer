include(../../QVproject.pri)

QT += testlib core

TARGET = tst_storagelocationtest
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

INCLUDEPATH += ../../apps/quickviewer/src/models

SOURCES += \
    tst_storagelocationtest.cpp \
    ../../apps/quickviewer/src/models/storagelocation.cpp

HEADERS += \
    ../../apps/quickviewer/src/models/storagelocation.h

DESTDIR = ../../lib
