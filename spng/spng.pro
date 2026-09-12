include(../QVproject.pri)

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= app_bundle

TARGET = spng
DESTDIR = ../lib

DEFINES += SPNG_STATIC

INCLUDEPATH += \
    $$PWD/libspng/spng \
    $$PWD/zlib

SOURCES += \
    libspng/spng/spng.c \
    zlib/adler32.c \
    zlib/compress.c \
    zlib/crc32.c \
    zlib/deflate.c \
    zlib/gzclose.c \
    zlib/gzlib.c \
    zlib/gzread.c \
    zlib/gzwrite.c \
    zlib/infback.c \
    zlib/inffast.c \
    zlib/inflate.c \
    zlib/inftrees.c \
    zlib/trees.c \
    zlib/uncompr.c \
    zlib/zutil.c

HEADERS += \
    libspng/spng/spng.h \
    zlib/zlib.h \
    zlib/zconf.h
