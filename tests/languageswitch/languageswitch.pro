# Exercise runtime language switching against the real QuickViewer widgets.
QV_APP_SOURCE = $$clean_path($$PWD/../../apps/quickviewer)
include($$QV_APP_SOURCE/QuickViewer.pro)

TRANSLATIONS =
CONFIG -= lrelease

# Paths declared by the application project are relative to its directory.
for(source, SOURCES): LANGSWITCH_SOURCES += $$absolute_path($$source, $$QV_APP_SOURCE)
for(header, HEADERS): LANGSWITCH_HEADERS += $$absolute_path($$header, $$QV_APP_SOURCE)
for(form, FORMS): LANGSWITCH_FORMS += $$absolute_path($$form, $$QV_APP_SOURCE)
for(resource, RESOURCES): LANGSWITCH_RESOURCES += $$absolute_path($$resource, $$QV_APP_SOURCE)
for(path, INCLUDEPATH): LANGSWITCH_INCLUDES += $$absolute_path($$path, $$QV_APP_SOURCE)

SOURCES = $$LANGSWITCH_SOURCES
SOURCES -= $$QV_APP_SOURCE/src/main.cpp
SOURCES += $$PWD/tst_languageswitch.cpp
HEADERS = $$LANGSWITCH_HEADERS
FORMS = $$LANGSWITCH_FORMS
RESOURCES = $$LANGSWITCH_RESOURCES
INCLUDEPATH = $$LANGSWITCH_INCLUDES
PRECOMPILED_HEADER = $$QV_APP_SOURCE/src/pch.h
RC_ICONS = $$QV_APP_SOURCE/icons/appicon.ico

QT += testlib
TARGET = tst_languageswitch
CONFIG += console testcase
CONFIG -= plugin app_bundle
DESTDIR = ../../lib
LIBS -= -L../../lib
LIBS += -L../../lib
win32: LIBS += -lshell32
QMAKE_POST_LINK =
INSTALLS =
