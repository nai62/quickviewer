#-------------------------------------------------
#
# Project created by QtCreator 2017-07-21T04:47:02
#
#-------------------------------------------------

include(../../qmake/windows-target.pri)

QT       += core gui widgets

TARGET = AssociateFilesWithQuickViewer
TEMPLATE = app
CONFIG += c++17
#CONFIG += console

QMAKE_TARGET_COMPANY = KATO Kanryu(k.kanryu@gmail.com)
QMAKE_TARGET_PRODUCT = AssociateFilesWithQuickViewer
QMAKE_TARGET_DESCRIPTION = Associate Files Tool for QuickViewer
QMAKE_TARGET_COPYRIGHT = (C) 2017 KATO Kanryu

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS EXECUTE_ON_UAC

*g++* {
    QMAKE_MANIFEST = $${PWD}/AssociateFilesWithQuickViewer.exe.manifest
}

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

LIBS += -lole32

INCLUDEPATH += ../../components/file-association ../../components/i18n

SOURCES += \
        main.cpp \
        ../../components/file-association/fileassocdialog.cpp \
        ../../components/i18n/languagemanager.cpp \
        ../../components/i18n/texttranslator.cpp

HEADERS += \
        ../../components/file-association/fileassocdialog.h \
        ../../components/i18n/languagemanager.h \
        ../../components/i18n/texttranslator.h

FORMS += \
        ../../components/file-association/fileassocdialog.ui

RC_ICONS = app_icon2.ico
#!CONFIG(debug, debug|release):!mingw {
!mingw {
    CONFIG += embed_manifest_exe
    QMAKE_LFLAGS +=  /MANIFESTUAC:$$quote(\"level=\'requireAdministrator\' uiAccess=\'false\'\")
}

TARGET_ARCH = $${QT_ARCH}
contains(TARGET_ARCH, x86_64) {
    TARGET_ARCH = x64
} else {
    TARGET_ARCH = x86
}

DESTDIR = ../../bin

RESOURCES += \
    resources.qrc

OTHER_FILES += \
    icons/qv_apng.ico \
    icons/qv_bmp.ico \
    icons/qv_dds.ico \
    icons/qv_gif.ico \
    icons/qv_icon.ico \
    icons/qv_jpeg.ico \
    icons/qv_png.ico \
    icons/qv_raw.ico \
    icons/qv_tga.ico \
    icons/qv_tiff.ico \
    icons/qv_webp.ico
