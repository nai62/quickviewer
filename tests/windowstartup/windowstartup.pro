# Exercise the real MainWindow startup path, including geometry restoration.
QV_APP_SOURCE = $$clean_path($$PWD/../../QuickViewer)
include($$QV_APP_SOURCE/QuickViewer.pro)

# Paths declared by the application project are relative to its directory.
for(source, SOURCES): STARTUP_SOURCES += $$absolute_path($$source, $$QV_APP_SOURCE)
for(header, HEADERS): STARTUP_HEADERS += $$absolute_path($$header, $$QV_APP_SOURCE)
for(form, FORMS): STARTUP_FORMS += $$absolute_path($$form, $$QV_APP_SOURCE)
for(resource, RESOURCES): STARTUP_RESOURCES += $$absolute_path($$resource, $$QV_APP_SOURCE)
for(path, INCLUDEPATH): STARTUP_INCLUDES += $$absolute_path($$path, $$QV_APP_SOURCE)

SOURCES = $$STARTUP_SOURCES
SOURCES -= $$QV_APP_SOURCE/src/main.cpp
SOURCES += $$PWD/tst_windowstartuptest.cpp
HEADERS = $$STARTUP_HEADERS
FORMS = $$STARTUP_FORMS
RESOURCES = $$STARTUP_RESOURCES
INCLUDEPATH = $$STARTUP_INCLUDES
PRECOMPILED_HEADER = $$QV_APP_SOURCE/src/pch.h
RC_ICONS = $$QV_APP_SOURCE/icons/appicon.ico

QT += testlib
TARGET = tst_windowstartuptest
CONFIG += console testcase
CONFIG -= plugin app_bundle
DESTDIR = ../../lib
LIBS -= -L../lib
LIBS += -L../../lib
win32: LIBS += -lshell32
QMAKE_POST_LINK =
INSTALLS =
