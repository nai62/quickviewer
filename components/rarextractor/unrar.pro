#QT -= core gui

TARGET = unrar
TEMPLATE = lib
CONFIG += staticlib
CONFIG += warn_off

DEFINES += _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE UNRAR RARDLL RAR_BUILD_LIB

include(../../qmake/third_party/unrar/unrar.pri)
INCLUDEPATH += $$PWD/../../third_party
#unix {
#    DEFINES += _UNIX
#}

DESTDIR = ../../lib

HEADERS += \
    rarextractor.h \
    raraccessstrategy.h

SOURCES += \
    rarextractor.cpp \
    raraccessstrategy.cpp
