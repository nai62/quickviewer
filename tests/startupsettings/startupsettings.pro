include(../../QVproject.pri)

QT += testlib core

TARGET = tst_startupsettingstest
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

INCLUDEPATH += ../../apps/quickviewer/src/models

SOURCES += \
    tst_startupsettingstest.cpp \
    ../../apps/quickviewer/src/models/startupsettings.cpp

HEADERS += \
    ../../apps/quickviewer/src/models/startupsettings.h

DESTDIR = ../../lib
