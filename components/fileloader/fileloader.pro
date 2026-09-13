#-------------------------------------------------
#
# Project created by QtCreator 2014-02-08T18:57:16
#
#-------------------------------------------------

QT       += core gui concurrent

TARGET = fileloader
TEMPLATE = lib

win32-msvc* {
    CONFIG += staticlib
    QMAKE_CXXFLAGS += /wd4819
}

*clang* || *g++* {
    CONFIG += c++17
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
INCLUDEPATH += \
        ../../third_party/7zip/7zip \
        ../../third_party/7zip/7zip/CPP \
        ../../third_party/lib7zip/src \

SOURCES += \
    $$PWD/fileloader.cpp \
    $$PWD/fileloader7zarchive.cpp \
    $$PWD/fileloaderdirectory.cpp \
    $$PWD/fileloadersubdirectory.cpp \
    $$PWD/fileloaderrararchive.cpp \

HEADERS += \
    $$PWD/fileloader.h \
    $$PWD/fileloader7zarchive.h \
    $$PWD/fileloaderdirectory.h \
    $$PWD/fileloadersubdirectory.h \
    $$PWD/fileloaderrararchive.h \

DESTDIR = ../../lib
LIBS += -L../../lib

DEFINES += UNRAR RARDLL
INCLUDEPATH += ../rarextractor
LIBS += -lunrar

#DEFINES += QT7Z_STATIC
INCLUDEPATH += ../qt7z
LIBS += -lQt7z
