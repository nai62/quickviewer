include(../../QVproject.pri)

QT += testlib core concurrent

TARGET = tst_latestresultdispatchertest
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += tst_latestresultdispatchertest.cpp

INCLUDEPATH += ../../apps/quickviewer/src/models

DESTDIR = ../../lib
