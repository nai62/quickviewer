# Exercise the catalog database and its builder without the catalog windows.
QV_APP_SOURCE = $$clean_path($$PWD/../../apps/quickviewer)
include($$QV_APP_SOURCE/QuickViewer.pro)

TRANSLATIONS =
CONFIG -= lrelease

# Paths declared by the application project are relative to its directory.
for(source, SOURCES): CATALOGDB_SOURCES += $$absolute_path($$source, $$QV_APP_SOURCE)
for(header, HEADERS): CATALOGDB_HEADERS += $$absolute_path($$header, $$QV_APP_SOURCE)
for(form, FORMS): CATALOGDB_FORMS += $$absolute_path($$form, $$QV_APP_SOURCE)
for(resource, RESOURCES): CATALOGDB_RESOURCES += $$absolute_path($$resource, $$QV_APP_SOURCE)
for(path, INCLUDEPATH): CATALOGDB_INCLUDES += $$absolute_path($$path, $$QV_APP_SOURCE)

SOURCES = $$CATALOGDB_SOURCES
SOURCES -= $$QV_APP_SOURCE/src/main.cpp
SOURCES += $$PWD/tst_catalogdatabasetest.cpp
HEADERS = $$CATALOGDB_HEADERS
FORMS = $$CATALOGDB_FORMS
RESOURCES = $$CATALOGDB_RESOURCES
INCLUDEPATH = $$CATALOGDB_INCLUDES
PRECOMPILED_HEADER = $$QV_APP_SOURCE/src/pch.h
RC_ICONS = $$QV_APP_SOURCE/icons/appicon.ico

QT += testlib
TARGET = tst_catalogdatabasetest
CONFIG += console testcase
CONFIG -= plugin app_bundle
DESTDIR = ../../lib
LIBS -= -L../../lib
LIBS += -L../../lib
win32: LIBS += -lshell32
QMAKE_POST_LINK =
INSTALLS =

DEFINES += CATALOGDATABASE_SRCDIR=\\\"$$PWD/\\\"
