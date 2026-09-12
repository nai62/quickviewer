include(../QVproject.pri)

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= app_bundle

TARGET = spng
DESTDIR = ../lib

DEFINES += \
    SPNG_STATIC \
    SPNG_USE_MINIZ \
    MINIZ_NO_ARCHIVE_APIS \
    MINIZ_NO_STDIO

INCLUDEPATH += \
    $$PWD \
    $$PWD/libspng/spng \
    $$PWD/miniz

SOURCES += \
    libspng/spng/spng.c \
    miniz/miniz.c \
    miniz/miniz_tdef.c \
    miniz/miniz_tinfl.c

HEADERS += \
    miniz_export.h \
    libspng/spng/spng.h \
    miniz/miniz.h \
    miniz/miniz_common.h \
    miniz/miniz_tdef.h \
    miniz/miniz_tinfl.h
