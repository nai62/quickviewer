QT += core
QT -= gui

TARGET = rar-benchmark
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle

INCLUDEPATH += ../../components/rarextractor
LIBS += -L../../lib -lunrar

win32 {
    LIBS += -ladvapi32 -lshell32
}

SOURCES += \
    main.cpp

DESTDIR = ../../bin
