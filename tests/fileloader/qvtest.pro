#-------------------------------------------------
#
# Project created by QtCreator 2017-09-10T02:57:37
#
#-------------------------------------------------

include(../../QVproject.pri)

QT       += testlib core gui concurrent

TARGET = tst_fileloadertest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        tst_fileloadertest.cpp

DEFINES += SRCDIR=\\\"$$PWD/\\\"

DESTDIR = ../../lib

win32 {
    LIBS += -luser32 -ladvapi32 -lShlwapi -loleaut32 -lole32 -lshell32
    QMAKE_POST_LINK += $$QMAKE_COPY /B $$shell_quote($$shell_path($$PWD/../../third_party/7zip/windll/$${TARGET_ARCH}/7z.dll)) $$shell_path($${DESTDIR}) $$escape_expand(\n\t)
}
unix {
    DEFINES += _UNIX
}

LIBS += -L../../lib -lunrar -lfileloader
INCLUDEPATH += ../../components/fileloader ../../components/rarextractor

OTHER_FILES += \
    data/deflate-mbcs.zip \
    data/deflate-utf8.zip \
    data/deflate64-mbcs.zip \
    data/deflate64-utf8.zip \
    data/7z/image.7z \
    data/7z/mixed.7z \
    data/7z/password.7z \
    data/7z/password-filename.7z \
    data/7z/text.7z \
    data/zip/encrypted.zip \
    data/zip/zero-byte.zip
