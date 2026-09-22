#-------------------------------------------------
#
# Project created by QtCreator 2016-05-24T17:06:01
#
#-------------------------------------------------

include(../../QVproject.pri)
include(../../qmake/windows-target.pri)
# A test project file points QV_APP_SOURCE at this directory before including
# this file; only the application itself copies its icons into its build tree,
# because the test projects build into the same directory in parallel.
QV_ASSOC_ICONS_FOR_THIS_PROJECT = true
!isEmpty(QV_APP_SOURCE): QV_ASSOC_ICONS_FOR_THIS_PROJECT = false
isEmpty(QV_APP_SOURCE): QV_APP_SOURCE = $$PWD
RESVG_SOURCE_ROOT = $$clean_path($$QV_APP_SOURCE/../../third_party/resvg)
include(../../qmake/third_party/resvg/resvg.pri)

QT       += core gui concurrent sql svgwidgets widgets network

VERSION = 2.0.0

TARGET = QuickViewer
TEMPLATE = app
CONFIG += plugin lrelease

QMAKE_TARGET_COMPANY = QuickViewer contributors
QMAKE_TARGET_PRODUCT = QuickViewer
QMAKE_TARGET_DESCRIPTION = QuickViewer
QMAKE_TARGET_COPYRIGHT = Copyright (C) 2017 KATO Kanryu and contributors

DEFINES += \
  APP_VERSION=\\\"$$VERSION\\\" \
  APP_NAME=\\\"$$QMAKE_TARGET_PRODUCT\\\" \

CODECFORSRC = UTF-8

DESTDIR = ../../bin
LRELEASE_DIR = $${DESTDIR}/translations

TRANSLATIONS = \
    translations/quickviewer_ar.ts \
    translations/quickviewer_el.ts \
    translations/quickviewer_es.ts \
    translations/quickviewer_fr.ts \
    translations/quickviewer_ja.ts \
    translations/quickviewer_ru.ts \
    translations/quickviewer_zh.ts

INCLUDEPATH += ../../third_party/resizehalf
INCLUDEPATH += ../../third_party/easyexif
INCLUDEPATH += ../../components/fileloader
INCLUDEPATH += ../../components/qzimg
INCLUDEPATH += ../../third_party/libspng/spng
INCLUDEPATH += ./src ./src/catalog ./src/widgets ./src/models ./src/folderview
INCLUDEPATH += ../../components/i18n ./src/qnamedpipe ./src/qactionmanager


LIBDIR = ../../lib

LIBS += -L$${LIBDIR}  -leasyexif -lresizehalf -lfileloader -lunrar -lzimg -lspng

contains(DEFINES, QV_WITH_LUMINOR) {
    INCLUDEPATH += $$PWD/../../components/qluminor
    win32 {
        LIBS += -L$$PWD/../../third_party/luminor/$${LUMINOR_BIN_PATH} -lluminor -lluminor_rgba -lhalide_runtime -lqluminor
    }
    unix {
        LIBS += -L$$PWD/../../third_party/luminor/$${LUMINOR_BIN_PATH} \
                $$PWD/../../third_party/luminor/$${LUMINOR_BIN_PATH}/luminor.o \
                $$PWD/../../third_party/luminor/$${LUMINOR_BIN_PATH}/luminor_rgba.o \
                $$PWD/../../third_party/luminor/$${LUMINOR_BIN_PATH}/halide_runtime.a \
                -lqluminor -ldl
    }
}

# The file type icons the association dialog registers next to the executable.
QV_ASSOC_ICONS = \
    ../../components/file-association/icons/qv_apng.ico \
    ../../components/file-association/icons/qv_bmp.ico \
    ../../components/file-association/icons/qv_dds.ico \
    ../../components/file-association/icons/qv_gif.ico \
    ../../components/file-association/icons/qv_icon.ico \
    ../../components/file-association/icons/qv_jpeg.ico \
    ../../components/file-association/icons/qv_png.ico \
    ../../components/file-association/icons/qv_raw.ico \
    ../../components/file-association/icons/qv_tga.ico \
    ../../components/file-association/icons/qv_tiff.ico \
    ../../components/file-association/icons/qv_webp.ico \

win32 {
    win32-msvc* {
        QMAKE_CXXFLAGS += /wd4819
        # more heap area for x86
        equals(TARGET_ARCH, x86) {
            QMAKE_LFLAGS += /LARGEADDRESSAWARE
        }
    }
    LIBS += -luser32 -ladvapi32 -lshell32 -lShlwapi -loleaut32 -lole32 -luuid -ldwmapi

    # copy official 7z.dll to build/bin/
    QMAKE_POST_LINK += $$QMAKE_COPY /B $$shell_quote($$shell_path($$PWD/../../third_party/7zip/windll/$${TARGET_ARCH}/7z.dll)) $$shell_path($${DESTDIR}) $$escape_expand(\n\t)

    # The association dialog registers these icons next to the executable, so a
    # build that is run before it is packaged needs them there as well. The last
    # icon is the file the copy is keyed on: it is missing with the directory
    # and older than any icon that changed.
    contains(QV_ASSOC_ICONS_FOR_THIS_PROJECT, true) {
        assoc_icons.target = $${DESTDIR}/iconengines/qv_webp.ico
        assoc_icons.depends =
        for(icon, QV_ASSOC_ICONS) {
            assoc_icons.depends += $$PWD/$$icon
        }
        assoc_icons.commands = if not exist "$$shell_path($${DESTDIR}/iconengines)" $(MKDIR) "$$shell_path($${DESTDIR}/iconengines)" $$escape_expand(\n\t)
        for(icon, QV_ASSOC_ICONS) {
            assoc_icons.commands += $$QMAKE_COPY /B $$shell_quote($$shell_path($$PWD/$$icon)) $$shell_quote($$shell_path($${DESTDIR}/iconengines)) $$escape_expand(\n\t)
        }
        QMAKE_EXTRA_TARGETS += assoc_icons
        PRE_TARGETDEPS += $$assoc_icons.target
    }
}
linux {
    DEFINES += _UNIX
    GCC_MAJOR = 6
    contains(DEFINES, QV_PORTABLE) {
        QMAKE_LFLAGS += -Wl,-rpath,../lib
    } else {
        QMAKE_LFLAGS += -Wl,-rpath,$${QV_LIB_PATH}
    }
}
macos {
    DEFINES += _UNIX
    QMAKE_LFLAGS += -Wl,-rpath,../lib -Wl,-rpath,../Frameworks
    GCC_MAJOR = 6
    # Info.plist variables
    QMAKE_INFO_PLIST = $$PWD/Info.plist
    ICON = $$PWD/icons/quickviewer.icns
    ASSETCATALOG_COMPILER_APPICON_NAME=quickviewer.icns
    EXECTABLE_NAME=QuickViewer
}


SOURCES += \
    src/benchmark/imagebenchmarkrunner.cpp \
    src/benchmark/startupfoldertextprofile.cpp \
    src/catalog/catalogbuilder.cpp \
    src/catalog/catalogbuildqueue.cpp \
    src/catalog/catalogdatabase.cpp \
    src/catalog/catalogvolumelistview.cpp \
    src/catalog/catalogwindow.cpp \
    src/catalog/databasesettingdialog.cpp \
    src/catalog/managedatabasedialog.cpp \
    src/catalog/searchwords.cpp \
    src/catalog/volumecoverpane.cpp \
    src/catalog/volumeitemmodel.cpp \
    src/catalog/volumenameparser.cpp \
    src/catalog/volumetagdialog.cpp \
    src/exifdialog.cpp \
    src/folderview/folderitemdelegate.cpp \
    src/folderview/folderitemmodel.cpp \
    src/folderview/foldertextcache.cpp \
    src/folderview/folderlistview.cpp \
    src/folderview/folderwindow.cpp \
    src/imageview.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/models/readprogressstore.cpp \
    src/models/filemanager.cpp \
    src/models/boundedexecutor.cpp \
    src/models/imagecontent.cpp \
    src/models/pagedisplayformatter.cpp \
    src/models/renderedpage.cpp \
    src/models/renderedpages.cpp \
    src/models/visiblepagecomposer.cpp \
    src/models/viewersession.cpp \
    src/models/prefetchplanner.cpp \
    src/models/qvapplication.cpp \
    src/models/shadermanager.cpp \
    src/models/shadereffect.cpp \
    src/models/svgloader.cpp \
    src/startupprofiler.cpp \
    src/models/volumecache.cpp \
    src/models/volumehandle.cpp \
    src/optionsdialog.cpp \
    src/renamedialog.cpp \
    src/widgets/flowlayout.cpp \
    src/widgets/pageslider.cpp \
    ../../components/i18n/languagemanager.cpp \
    src/qnamedpipe/qnamedpipe.cpp \
    src/widgets/innerframe.cpp \
    src/models/movie.cpp \
    src/models/volume.cpp \
    src/models/imagedecoder.cpp \
    src/models/jpegorientation.cpp \
    src/models/volumeloader.cpp \
    src/models/volumelocation.cpp \
    src/models/storedvolumelocation.cpp \
    src/qactionmanager/keyconfigdialog.cpp \
    src/qactionmanager/mouseconfigdialog.cpp \
    src/qactionmanager/qactionmanager.cpp \
    src/qactionmanager/qmousesequence.cpp \
    src/qactionmanager/shortcutbutton.cpp \
    src/models/imagestring.cpp \
    src/models/loupecontroller.cpp \
    src/retouchwindow.cpp \
    src/models/fileoperator.cpp \
    ../../components/i18n/texttranslator.cpp \
    src/models/imagemetadata.cpp


HEADERS  += \
    src/benchmark/imagebenchmarkrunner.h \
    src/benchmark/startupfoldertextprofile.h \
    src/catalog/catalogbuilder.h \
    src/catalog/catalogbuildqueue.h \
    src/catalog/catalogdatabase.h \
    src/catalog/catalogvolumelistview.h \
    src/catalog/catalogwindow.h \
    src/catalog/databasesettingdialog.h \
    src/catalog/managedatabasedialog.h \
    src/catalog/searchwords.h \
    src/catalog/volumecoverpane.h \
    src/catalog/catalogrecords.h \
    src/catalog/volumeitemmodel.h \
    src/catalog/volumenameparser.h \
    src/catalog/volumetagdialog.h \
    src/exifdialog.h \
    src/folderview/folderitemdelegate.h \
    src/folderview/folderitem.h \
    src/folderview/folderitemmodel.h \
    src/folderview/foldertextcache.h \
    src/folderview/folderlistview.h \
    src/folderview/folderwindow.h \
    src/imageview.h \
    src/mainwindow.h \
    src/models/readprogressstore.h \
    src/models/filemanager.h \
    src/models/boundedexecutor.h \
    src/models/cursorscrollmapping.h \
    src/models/lrucache.h \
    src/models/imageloadcontext.h \
    src/models/imageloadmetrics.h \
    src/models/loupecontroller.h \
    src/models/pagenavigator.h \
    src/models/imagecontent.h \
    src/models/pagedisplayformatter.h \
    src/models/renderedpage.h \
    src/models/visiblepagecomposer.h \
    src/models/viewersession.h \
    src/models/latestresultdispatcher.h \
    src/models/prefetchplanner.h \
    src/models/qvapplication.h \
    src/models/shadermanager.h \
    src/models/shadereffect.h \
    src/models/svgloader.h \
    src/models/volumecache.h \
    src/models/volumehandle.h \
    src/models/viewerstate.h \
    src/models/viewerloadstatus.h \
    src/models/visiblepages.h \
    src/models/renderedpages.h \
    src/models/renderedpagemetrics.h \
    src/optionsdialog.h \
    src/qvenums.h \
    src/renamedialog.h \
    src/pch.h \
    src/widgets/flowlayout.h \
    src/widgets/pageslider.h \
    ../../components/i18n/languagemanager.h \
    src/qnamedpipe/qnamedpipe.h \
    src/widgets/innerframe.h \
    src/models/movie.h \
    src/models/volume.h \
    src/models/imagedecoder.h \
    src/models/jpegorientation.h \
    src/models/decodemetricsscope.h \
    src/models/volumeloader.h \
    src/models/volumelocation.h \
    src/models/storedvolumelocation.h \
    src/qactionmanager/keyconfigdialog.h \
    src/qactionmanager/mouseconfigdialog.h \
    src/qactionmanager/qactionmanager.h \
    src/qactionmanager/qmousesequence.h \
    src/qactionmanager/shortcutbutton.h \
    src/models/imagestring.h \
    src/retouchwindow.h \
    src/startupprofiler.h \
    src/models/fileoperator.h \
    ../../components/i18n/texttranslator.h \
    src/models/imagemetadata.h

win32 {
    INCLUDEPATH += ../../components/file-association
    SOURCES += src/mainwindowforwindows.cpp ../../components/file-association/fileassocdialog.cpp
    HEADERS += src/mainwindowforwindows.h ../../components/file-association/fileassocdialog.h
}


PRECOMPILED_HEADER += src/pch.h

FORMS    += \
    src/mainwindow.ui \
    src/exifdialog.ui \
    src/qactionmanager/keyconfigdialog.ui \
    src/catalog/cataloglist.ui \
    src/catalog/catalogwindow.ui \
    src/catalog/createdb.ui \
    src/catalog/volumetagdialog.ui \
    src/folderview/folderwindow.ui \
    src/optionsdialog.ui \
    src/renamedialog.ui \
    ../../components/file-association/fileassocdialog.ui \
    src/retouchwindow.ui

RESOURCES += toolbar.qrc \
    themes.qrc

!CONFIG(debug, debug|release) {
    win32 {
        RESOURCES += qtconf-win.qrc
    }
    linux : contains(DEFINES, QV_PORTABLE) {
        RESOURCES += qtconf-linux-appimage.qrc
    }
    linux : !contains(DEFINES, QV_PORTABLE) {
#        RESOURCES += qtconf-linux.qrc
    }
    macos {
        RESOURCES += qtconf-macos.qrc
    }
}

RC_ICONS = icons/appicon.ico


DBS += \
    database/schema.sql \

OTHER_FILES  += qt.conf $$DBS

DBBIN += \
    database/thumbnail.sqlite3.db \

DBDIR += database/

# win32 depoying, please add 'jom install' into build setting on qt-creator
win32 : !CONFIG(debug, debug|release) {
    mingw {
        MY_DEFAULT_INSTALL = ../../../QuickViewer-$${VERSION}-mingw-$${TARGET_ARCH}

        install_target.files = $${DESTDIR}/QuickViewer.exe $${LIBDIR}/fileloader.dll $$PWD/../../third_party/7zip/windll/$${TARGET_ARCH}/7z.dll

        INSTALLS += install_target install_deploy_files install_translations install_assoc_icons
    } else {
        contains(DEFINES, QV_PORTABLE) {
            MY_DEFAULT_INSTALL = ../../../QuickViewer-portable-$${VERSION}-$${TARGET_ARCH}
        } else {
            MY_DEFAULT_INSTALL = ../../../QuickViewer-$${VERSION}/$${TARGET_ARCH}
        }

        install_target.path = $${MY_DEFAULT_INSTALL}
        install_target.files = \
            $${DESTDIR}/QuickViewer.exe \
            $$PWD/../../third_party/7zip/windll/$${TARGET_ARCH}/7z.dll \

        install_qrawspeed.path = $${MY_DEFAULT_INSTALL}/imageformats
        install_qrawspeed.files = \
            ../../../../qrawspeed/imageformats-$${TARGET_ARCH}/qrawspeed0.dll \
            ../../../../qrawspeed/imageformats-$${TARGET_ARCH}/qapng.dll \
            ../../../../qrawspeed/imageformats-$${TARGET_ARCH}/qjp2.dll \
            ../../../../qrawspeed/imageformats-$${TARGET_ARCH}/qdds.dll \
            ../../../../qrawspeed/imageformats-$${TARGET_ARCH}/qjpegxr.dll \

#            ../../../../qrawspeed/imageformats-$${TARGET_ARCH}/qlodepng0.dll \

        install_qvavif.path = $${MY_DEFAULT_INSTALL}/imageformats
        install_qvavif.files = \
            ../../../../qt-avif-image-plugin/imageformats-$${TARGET_ARCH}/qavif6.dll \

        install_qvheif.path = $${MY_DEFAULT_INSTALL}/imageformats
        install_qvheif.files = \
            ../../../../qt-heic-image-plugin/qtbuild_6.11.2/kimg_heif6.dll \

        install_qvheif_runtime.path = $${MY_DEFAULT_INSTALL}
        install_qvheif_runtime.files = $$files(../../../../qt-heic-image-plugin/3rdparty/install/bin/*.dll)

        # dlls instead of vcredist_xxx.exe
        install_msvcrt.PATH = C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC/14.38.33130/x64/Microsoft.VC143.CRT
        install_msvcrt.path = $${MY_DEFAULT_INSTALL}
        install_msvcrt.removefiles = $$shell_path($${MY_DEFAULT_INSTALL}/vc_redist.$${TARGET_ARCH}.exe)
        install_msvcrt.commands = -$(DEL_FILE) "$${install_msvcrt.removefiles}"
        install_msvcrt.depends = install_install_deploy_files
        install_msvcrt.files = \
            "$${install_msvcrt.PATH}/concrt140.dll" \
            "$${install_msvcrt.PATH}/msvcp140.dll" \
            "$${install_msvcrt.PATH}/vccorlib140.dll" \
            "$${install_msvcrt.PATH}/vcruntime140.dll"

        INSTALLS += install_target install_deploy_files install_translations install_translations2 install_qrawspeed install_qvavif install_qvheif install_qvheif_runtime install_msvcrt install_assoc_icons
    }
    install_deploy_files.path = $${MY_DEFAULT_INSTALL}
    install_deploy_files.files = \
        $${PWD}/../../README.md \
        $${PWD}/../../LICENSE

    # The application draws through the platform's widget backend and asks for
    # neither OpenGL nor the shader compilers of the D3D backends, so the
    # deployment leaves Qt's software OpenGL and the D3D compilers out: the
    # windeployqt flags cover the first two, and the D3D12 compilers that
    # windeployqt copies despite them are removed afterwards.
    install_deploy_files.commands = $$shell_path($$[QT_INSTALL_BINS]/windeployqt) --release --compiler-runtime --no-opengl-sw --no-system-d3d-compiler $$shell_path($${MY_DEFAULT_INSTALL}/QuickViewer.exe) $$escape_expand(\n\t) -$(DEL_FILE) $$shell_path($${MY_DEFAULT_INSTALL}/dxcompiler.dll) $$shell_path($${MY_DEFAULT_INSTALL}/dxil.dll)

    install_translations.path = $${MY_DEFAULT_INSTALL}/translations
    QM_FILES_INSTALL_PATH = $${install_translations.path}
    install_translations.commands = $$shell_path($$[QT_INSTALL_BINS]/../../../Tools/QtCreator/bin/qbs) resolve -f $${PWD}/translations/maketransconf.qbs qbs.installRoot:$${MY_DEFAULT_INSTALL}
    install_translations.files = \
        $${PWD}/translations/languages.ini \
        $${PWD}/translations/qt_el.qm \

    install_translations2.path = $${MY_DEFAULT_INSTALL}/translations
    install_translations2.commands = $$shell_path($$[QT_INSTALL_BINS]/../../../Tools/QtCreator/bin/qbs) -f $${PWD}/translations/maketransconf.qbs qbs.installRoot:$${MY_DEFAULT_INSTALL}
    install_translations2.depends = install_install_translations
    install_translations2.files = \
        $$[QT_INSTALL_TRANSLATIONS]/qt_zh_CN.qm \

    install_assoc_icons.path = $${MY_DEFAULT_INSTALL}/iconengines
    install_assoc_icons.files = $$QV_ASSOC_ICONS

    install_db.path = $${MY_DEFAULT_INSTALL}/database
    install_db.depends = install_install_assoc_icons
    install_db.files = \
        $$DBS \
        $$DBBIN \

    INSTALLS += install_db

    # App installer with NSIS
    install_nsis.path = $${MY_DEFAULT_INSTALL}/..
    install_nsis.commands = "C:\Program Files (x86)\NSIS\makensis.exe"  /DAPPVERSION=$${VERSION} $${PWD}/install.nsi
    install_nsis.depends = install_install_db install_install_deploy_files
    #install_nsis.files = $${MY_DEFAULT_INSTALL}/../QuickViewer-Installer-$${VERSION}.exe

    INSTALLS += install_nsis

    install_direct2d.path = $${MY_DEFAULT_INSTALL}/platforms
    install_direct2d.files = $$[QT_INSTALL_PLUGINS]/platforms/qdirect2d.dll
    INSTALLS += install_direct2d
}

# linuxdeployqt is required.
linux : !CONFIG(debug, debug|release) : contains(DEFINES, QV_PORTABLE) {

    APPDIR = QuickViewer-$${VERSION}-$${TARGET_ARCH}.AppDir
    APPIMAGE = QuickViewer-$${VERSION}-$${TARGET_ARCH}.AppImage
    MY_DEFAULT_INSTALL = $${OUT_PWD}/../../../$${APPDIR}
    message(DESTDIR $${DESTDIR})

    # for(var, $$list($$enumerate_vars())) {
    #   message($$var)
    #   message($$eval($$var))
    # }

    install_target.files = \
        $${OUT_PWD}/../../bin/QuickViewer \
        $${OUT_PWD}/../../../bundle/7z.so \

    install_target.path = $${MY_DEFAULT_INSTALL}/usr/bin

    install_libs.files = \
        $${OUT_PWD}/../../lib/libfileloader.so.1 \

    install_libs.path = $${MY_DEFAULT_INSTALL}/usr/lib

    install_desktop.files = \
        $${PWD}/QuickViewer.desktop \
        $${PWD}/../../docs/quickviewer.png \

    install_desktop.path = $${MY_DEFAULT_INSTALL}

    install_deploy_files.path = $${MY_DEFAULT_INSTALL}
    install_deploy_files.files = \
        $${PWD}/../../README.md \
        $${PWD}/../../LICENSE \

    install_deploy_files.commands = linuxdeployqt $${MY_DEFAULT_INSTALL}/QuickViewer.desktop -qmake=$$[QT_INSTALL_BINS]/qmake -bundle-non-qt-libs -exclude-libs=libqsqlmimer,libqsqlmysql,libqsqlodbc,libqsqlpsql
    install_deploy_files.depends = install_install_target install_install_libs install_install_desktop

#    install_translations.commands = ldd $${MY_DEFAULT_INSTALL}/usr/bin/QuickViewer | awk \'\$$1==\"libstdc++.so.$${GCC_MAJOR}\" {print \$$3}\' | xargs cp -t $${MY_DEFAULT_INSTALL}/usr/lib

    install_translations.path = $${MY_DEFAULT_INSTALL}/translations
    QM_FILES_INSTALL_PATH = $${install_translations.path}
    install_translations.files = \
        $${PWD}/translations/languages.ini \
        $${PWD}/translations/qt_el.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qt_zh_CN.qm \

    install_assoc_icons.path = $${MY_DEFAULT_INSTALL}/usr/shared/icons
    install_assoc_icons.files = $$QV_ASSOC_ICONS

    install_appimage.path = $${MY_DEFAULT_INSTALL}/..
    install_appimage.files = $${APPIMAGE}
    install_appimage.commands = appimagetool $${MY_DEFAULT_INSTALL} $${MY_DEFAULT_INSTALL}/../$${APPIMAGE}
    install_appimage.depends = install_install_deploy_files install_install_translations install_install_assoc_icons install_install_db

    INSTALLS += install_target install_libs install_desktop install_deploy_files install_apprun install_translations install_assoc_icons install_appimage

#    contains(DEFINES, QV_PORTABLE) {
#        install_deploy_files.files += $${PWD}/AppRun
#        install_deploy_files.files -= $${PWD}/../../LICENSE

#        install_apprun.path = $${MY_DEFAULT_INSTALL}
#        install_apprun.files = $${PWD}/../../LICENSE
#        install_apprun.commands = chmod 755 $${MY_DEFAULT_INSTALL}/AppRun
#        install_apprun.depends = install_install_deploy_files
#    }

    install_db.path = $${MY_DEFAULT_INSTALL}/var/database
    install_db.files = $$DBS $$DBBIN

    INSTALLS += install_db
}

# not portable, install into /usr/local/bin
linux : !CONFIG(debug, debug|release) : !contains(DEFINES, QV_PORTABLE) {
    APPDIR = QuickViewer-$${VERSION}-$${TARGET_ARCH}.AppDir
    APPIMAGE = QuickViewer-$${VERSION}-$${TARGET_ARCH}.AppImage
    MY_DEFAULT_INSTALL = ../../../$${APPDIR}

    install_target.files = $${DESTDIR}/QuickViewer
    install_target.path = $${QV_BIN_PATH}

    install_libs.files = \
        $${DESTDIR}/../lib/libfileloader.so.1 \
        $${DESTDIR}/../lib/lib7z.so \

    install_libs.path = $${QV_LIB_PATH}


    install_deploy_files.path = $${QV_SHARED_PATH}/QuickViewer
    install_deploy_files.files = \
        $${PWD}/../../README.md \
        $${PWD}/../../LICENSE \

    install_deploy_files.depends = install_install_target install_install_libs

    install_translations.path = $$[QT_INSTALL_TRANSLATIONS]
    QM_FILES_INSTALL_PATH = $${install_translations.path}
    install_translations.files = \
        $${PWD}/translations/languages.ini \
        $${PWD}/translations/qt_el.qm

    install_assoc_icons.path = $${QV_SHARED_PATH}/QuickViewer/icons
    install_assoc_icons.files = $$QV_ASSOC_ICONS

    INSTALLS += install_target install_libs install_deploy_files install_translations install_assoc_icons
}

macos : !CONFIG(debug, debug|release) {
    APPDIR = QuickViewer.app
    APPIMAGE = QuickViewer-$${VERSION}-$${TARGET_ARCH}.dmg
    MY_DEFAULT_INSTALL = ../../../$${APPDIR}

    install_target.files = $${DESTDIR}/$${APPDIR}
    install_target.path = ../../../

    install_libs.files = $${DESTDIR}/../lib/lib7z.1.0.dylib $${DESTDIR}/../lib/libfileloader.1.0.dylib
    install_libs.commands = cp -rfp $${DESTDIR}/$${APPDIR} ../../../
    install_libs.path = $${MY_DEFAULT_INSTALL}/Contents/Frameworks

    install_deploy_files.path = $${MY_DEFAULT_INSTALL}/Contents
    install_deploy_files.files = $${PWD}/../../README.md
    install_deploy_files.commands = $$shell_path($$[QT_INSTALL_BINS]/macdeployqt) $${MY_DEFAULT_INSTALL} -libpath=$$${DESTDIR}/../lib
    install_deploy_files.depends = install_install_target install_install_libs

    install_translations.path = $${MY_DEFAULT_INSTALL}/Contents/Resources/translations
    QM_FILES_INSTALL_PATH = $${install_translations.path}
    install_translations.commands = rm -f $${MY_DEFAULT_INSTALL}/Contents/PlugIns/sqldrivers/libqsqlmysql.dylib $${MY_DEFAULT_INSTALL}/Contents/PlugIns/sqldrivers/libqsqlpsql.dylib
    install_translations.files = \
        $${PWD}/translations/languages.ini \
        $${PWD}/translations/qt_el.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qt_zh_CN.qm \

    install_dmg.path = $${MY_DEFAULT_INSTALL}/..
    install_dmg.files = $${APPIMAGE}
    install_dmg.commands = $$shell_path($$[QT_INSTALL_BINS]/macdeployqt) $${MY_DEFAULT_INSTALL} -no-plugins -no-strip -dmg
    install_dmg.depends = install_install_deploy_files install_install_translations install_install_db

    install_rename_dmg.path = $${MY_DEFAULT_INSTALL}/Contents
    install_rename_dmg.files = test
    install_rename_dmg.commands = mv ../../../QuickViewer.dmg ../../../$${APPIMAGE}
    install_rename_dmg.depends = install_install_dmg

    INSTALLS += install_target install_libs install_desktop install_deploy_files install_translations install_db install_dmg install_rename_dmg

    install_db.path = $${MY_DEFAULT_INSTALL}/Contents/Resources
    install_db.files = $$DBS $$DBBIN

    INSTALLS += install_db
}
