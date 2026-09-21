#-------------------------------------------------
#
# Project created by QtCreator 2014-02-08T18:57:16
#
#-------------------------------------------------

QT       -= core

TARGET = resizehalf
TEMPLATE = lib
CONFIG += staticlib
CONFIG += warn_off
CONFIG += c++17

win32-msvc* {
    QMAKE_CXXFLAGS += /wd4819
}

RESIZEHALF_ROOT = $$clean_path($$PWD/../../../third_party/resizehalf)

SOURCES += \
    $$RESIZEHALF_ROOT/ResizeHalf.cpp

HEADERS += \
    $$RESIZEHALF_ROOT/ResizeHalf.h \
    $$RESIZEHALF_ROOT/rh_common.h \
    $$RESIZEHALF_ROOT/reduceby2_functions.h \
    $$RESIZEHALF_ROOT/bilinear_functions.h \

INCLUDEPATH += \
    $$RESIZEHALF_ROOT

DESTDIR = ../../../lib

