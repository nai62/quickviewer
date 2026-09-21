#-------------------------------------------------
#
# Project created by QtCreator 2014-02-08T18:57:16
#
#-------------------------------------------------

QT       -= core

TARGET = easyexif
TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++17

EASYEXIF_ROOT = $$clean_path($$PWD/../../../third_party/easyexif)

SOURCES += \
    $$EASYEXIF_ROOT/exif.cpp

HEADERS += \
    $$EASYEXIF_ROOT/exif.h

INCLUDEPATH += \
    $$EASYEXIF_ROOT

DESTDIR = ../../../lib
