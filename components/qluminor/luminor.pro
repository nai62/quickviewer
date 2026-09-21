include("../../QVproject.pri")
QT       += core gui

CONFIG += c++17

TARGET = qluminor
TEMPLATE = lib
CONFIG += staticlib

LUMINOR_ROOT = $$clean_path($$PWD/../../third_party/luminor)
LUMINOR_BIN_DIR = $$LUMINOR_ROOT/$${LUMINOR_BIN_PATH}

win32 {
LIBS += -L$${LUMINOR_BIN_DIR} -lluminor -lluminor_rgba -lhalide_runtime
}

unix {
#OBJECTS_DIR += $${LUMINOR_BIN_DIR}
#OBJECTS += $${LUMINOR_BIN_DIR}/luminor.o $${LUMINOR_BIN_DIR}/luminor_rgba.o
LIBS += -L$${LUMINOR_BIN_DIR} -lhalide_runtime  $${LUMINOR_BIN_DIR}/luminor.o $${LUMINOR_BIN_DIR}/luminor_rgba.o
}
DESTDIR = ../../lib

INCLUDEPATH += $${LUMINOR_BIN_DIR} $${LUMINOR_ROOT}/luminor
HEADERS += \
    $${LUMINOR_BIN_DIR}/luminor.h \
    $${LUMINOR_BIN_DIR}/luminor_rgba.h \
    $${LUMINOR_ROOT}/luminor/HalideBuffer.h \
    $${LUMINOR_ROOT}/luminor/HalideRuntime.h \
    qluminor.h





# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    qluminor.cpp
