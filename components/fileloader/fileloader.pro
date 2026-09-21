#-------------------------------------------------
#
# Project created by QtCreator 2014-02-08T18:57:16
#
#-------------------------------------------------

QT       += core gui concurrent

TARGET = fileloader
TEMPLATE = lib
CONFIG += c++17

win32-msvc* {
    CONFIG += staticlib
    QMAKE_CXXFLAGS += /wd4819
}

*clang* || *g++* {
    win32: CONFIG += staticlib
    QMAKE_LFLAGS += -Wl,-rpath,../lib
    DEFINES += NTDDI_VERSION=NTDDI_VISTA
}

win32 {
    LIBS += -luser32 -ladvapi32 -lShlwapi -loleaut32 -lole32
}
unix {
    DEFINES += _UNIX
}

include(../../qmake/third_party/lib7zip/lib7zip.pri)

SOURCES += \
    $$PWD/fileloader.cpp \
    $$PWD/imageformat.cpp \
    $$PWD/fileloader7zarchive.cpp \
    $$PWD/fileloaderdirectory.cpp \
    $$PWD/fileloadersubdirectory.cpp \
    $$PWD/fileloaderrararchive.cpp \

HEADERS += \
    $$PWD/fileloader.h \
    $$PWD/imageformat.h \
    $$PWD/fileloader7zarchive.h \
    $$PWD/fileloaderdirectory.h \
    $$PWD/fileloadersubdirectory.h \
    $$PWD/fileloaderrararchive.h \

DESTDIR = ../../lib
LIBS += -L../../lib

DEFINES += UNRAR RARDLL
INCLUDEPATH += ../rarextractor
LIBS += -lunrar
