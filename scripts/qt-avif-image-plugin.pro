# Build with the same Qt kit and configuration as QuickViewer.
isEmpty(AVIF_SOURCE): AVIF_SOURCE = $$_PRO_FILE_PWD_/../../../qt-avif-image-plugin
AVIF_SOURCE = $$absolute_path($$AVIF_SOURCE, $$OUT_PWD)
isEmpty(AVIF_DEPS): AVIF_DEPS = $$AVIF_SOURCE/3rdparty/install
AVIF_DEPS = $$absolute_path($$AVIF_DEPS, $$OUT_PWD)

TARGET = qavif6
TEMPLATE = lib
DESTDIR = .
QT += gui
CONFIG += plugin c++17 skip_target_version_ext warn_on
CONFIG -= debug_and_release debug_and_release_target

HEADERS = $$AVIF_SOURCE/src/qavifhandler_p.h $$AVIF_SOURCE/src/util_p.h
SOURCES = $$AVIF_SOURCE/src/main.cpp $$AVIF_SOURCE/src/qavifhandler.cpp
OTHER_FILES = $$AVIF_SOURCE/src/avif.json

win32 {
    INCLUDEPATH += $$AVIF_DEPS/include
    LIBS += $$AVIF_DEPS/lib/avif.lib
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += libavif
}
