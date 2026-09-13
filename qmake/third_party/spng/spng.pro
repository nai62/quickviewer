include(../../../QVproject.pri)

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= app_bundle

TARGET = spng
DESTDIR = ../../../lib

DEFINES += SPNG_STATIC

LIBSPNG_ROOT = $$clean_path($$PWD/../../../third_party/libspng)
ZLIB_ROOT = $$clean_path($$PWD/../../../third_party/zlib)

INCLUDEPATH += \
    $$LIBSPNG_ROOT/spng \
    $$ZLIB_ROOT

SOURCES += \
    $$LIBSPNG_ROOT/spng/spng.c \
    $$ZLIB_ROOT/adler32.c \
    $$ZLIB_ROOT/compress.c \
    $$ZLIB_ROOT/crc32.c \
    $$ZLIB_ROOT/deflate.c \
    $$ZLIB_ROOT/gzclose.c \
    $$ZLIB_ROOT/gzlib.c \
    $$ZLIB_ROOT/gzread.c \
    $$ZLIB_ROOT/gzwrite.c \
    $$ZLIB_ROOT/infback.c \
    $$ZLIB_ROOT/inffast.c \
    $$ZLIB_ROOT/inflate.c \
    $$ZLIB_ROOT/inftrees.c \
    $$ZLIB_ROOT/trees.c \
    $$ZLIB_ROOT/uncompr.c \
    $$ZLIB_ROOT/zutil.c

HEADERS += \
    $$LIBSPNG_ROOT/spng/spng.h \
    $$ZLIB_ROOT/zlib.h \
    $$ZLIB_ROOT/zconf.h
