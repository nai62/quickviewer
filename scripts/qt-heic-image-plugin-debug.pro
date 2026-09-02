isEmpty(HEIF_SOURCE) {
    HEIF_SOURCE = $$_PRO_FILE_PWD_/../../../qt-heic-image-plugin
}

TARGET = kimg_heif6

INCLUDEPATH += $$HEIF_SOURCE/3rdparty/install/include

HEADERS = \
    $$HEIF_SOURCE/src/heif_p.h \
    $$HEIF_SOURCE/src/util_p.h
SOURCES = $$HEIF_SOURCE/src/heif.cpp
OTHER_FILES = $$HEIF_SOURCE/src/heif.json

LIBS += $$HEIF_SOURCE/3rdparty/install/lib/heif.lib

TEMPLATE = lib

CONFIG += debug skip_target_version_ext c++17 warn_on plugin
CONFIG -= separate_debug_info release debug_and_release force_debug_info

QMAKE_TARGET_COMPANY = "Daniel Novomesky"
QMAKE_TARGET_PRODUCT = "qt-heic-image-plugin"
QMAKE_TARGET_DESCRIPTION = "Qt plug-in to allow Qt applications to read HEIF/HEIC images."
QMAKE_TARGET_COPYRIGHT = "Copyright (C) 2020-2026 Daniel Novomesky"
QMAKE_TARGET_COMMENTS = "Debug build for QuickViewer tests"
