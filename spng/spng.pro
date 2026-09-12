include(../QVproject.pri)

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= app_bundle

TARGET = spng
DESTDIR = ../lib

DEFINES += SPNG_STATIC

SPNG_COMPRESSION_BACKEND = zlib

INCLUDEPATH += \
    $$PWD/libspng/spng

SOURCES += \
    libspng/spng/spng.c

HEADERS += \
    libspng/spng/spng.h

equals(SPNG_COMPRESSION_BACKEND, miniz) {
    DEFINES += \
        SPNG_USE_MINIZ \
        MINIZ_NO_ARCHIVE_APIS \
        MINIZ_NO_STDIO

    INCLUDEPATH += $$PWD/miniz

    SOURCES += \
        miniz/miniz.c \
        miniz/miniz_tdef.c \
        miniz/miniz_tinfl.c

    HEADERS += \
        miniz_export.h \
        miniz/miniz.h \
        miniz/miniz_common.h \
        miniz/miniz_tdef.h \
        miniz/miniz_tinfl.h
} else {
    INCLUDEPATH += $$PWD/zlib

    SOURCES += \
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
}
