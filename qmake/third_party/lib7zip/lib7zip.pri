#DEFINES += QT7Z_LIBRARY QT7Z_STATIC EXTERNAL_CODECS
DEFINES += QT_DEPRECATED_WARNINGS

THIRD_PARTY_ROOT = $$clean_path($$PWD/../../../third_party)
SEVENZIP_ROOT = $$THIRD_PARTY_ROOT/7zip/7zip
P7ZIP_ROOT = $$THIRD_PARTY_ROOT/p7zip
LIB7ZIP_ROOT = $$THIRD_PARTY_ROOT/lib7zip

win32 {
#    DEFINES += QT7Z_STATIC_LINK
#    DEFINES += UNICODE
#    DEFINES += _UNICODE
#    DEFINES += WIN_LONG_PATH
#    DEFINES += _7ZIP_LARGE_PAGES
#    DEFINES += SUPPORT_DEVICE_FILE
    !CONFIG(debug, debug|release): DEFINES += NDEBUG

    INCLUDEPATH += \
        $$SEVENZIP_ROOT \
        $$SEVENZIP_ROOT/CPP \
        $$LIB7ZIP_ROOT/src \

    HEADERS += \
        $$LIB7ZIP_ROOT/src/7ZipArchiveOpenCallback.h \
        $$LIB7ZIP_ROOT/src/7ZipCodecInfo.h \
        $$LIB7ZIP_ROOT/src/7ZipCompressCodecsInfo.h \
        $$LIB7ZIP_ROOT/src/7ZipDllHandler.h \
        $$LIB7ZIP_ROOT/src/7ZipFormatInfo.h \
        $$LIB7ZIP_ROOT/src/7ZipFunctions.h \
        $$LIB7ZIP_ROOT/src/7ZipInStreamWrapper.h \
        $$LIB7ZIP_ROOT/src/GUIDs.h \
        $$LIB7ZIP_ROOT/src/HelperFuncs.h \
        $$LIB7ZIP_ROOT/src/OSFunctions.h \
        $$LIB7ZIP_ROOT/src/OSFunctions_OS2.h \
        $$LIB7ZIP_ROOT/src/OSFunctions_UnixLike.h \
        $$LIB7ZIP_ROOT/src/OSFunctions_Win32.h \
        $$LIB7ZIP_ROOT/src/lib7zip.h \
        $$SEVENZIP_ROOT/CPP/Common/AutoPtr.h \
        $$SEVENZIP_ROOT/CPP/Common/C_FileIO.h \
        $$SEVENZIP_ROOT/CPP/Common/ComTry.h \
        $$SEVENZIP_ROOT/CPP/Common/CommandLineParser.h \
        $$SEVENZIP_ROOT/CPP/Common/Common.h \
        $$SEVENZIP_ROOT/CPP/Common/Defs.h \
        $$SEVENZIP_ROOT/CPP/Common/DynLimBuf.h \
        $$SEVENZIP_ROOT/CPP/Common/DynamicBuffer.h \
        $$SEVENZIP_ROOT/CPP/Common/IntToString.h \
        $$SEVENZIP_ROOT/CPP/Common/Lang.h \
        $$SEVENZIP_ROOT/CPP/Common/ListFileUtils.h \
        $$SEVENZIP_ROOT/CPP/Common/MyBuffer.h \
        $$SEVENZIP_ROOT/CPP/Common/MyBuffer2.h \
        $$SEVENZIP_ROOT/CPP/Common/MyCom.h \
        $$SEVENZIP_ROOT/CPP/Common/MyException.h \
        $$SEVENZIP_ROOT/CPP/Common/MyGuidDef.h \
        $$SEVENZIP_ROOT/CPP/Common/MyInitGuid.h \
        $$SEVENZIP_ROOT/CPP/Common/MyLinux.h \
        $$SEVENZIP_ROOT/CPP/Common/MyMap.h \
        $$SEVENZIP_ROOT/CPP/Common/MyString.h \
        $$SEVENZIP_ROOT/CPP/Common/MyTypes.h \
        $$SEVENZIP_ROOT/CPP/Common/MyUnknown.h \
        $$SEVENZIP_ROOT/CPP/Common/MyVector.h \
        $$SEVENZIP_ROOT/CPP/Common/MyWindows.h \
        $$SEVENZIP_ROOT/CPP/Common/MyXml.h \
        $$SEVENZIP_ROOT/CPP/Common/NewHandler.h \
        $$SEVENZIP_ROOT/CPP/Common/Random.h \
        $$SEVENZIP_ROOT/CPP/Common/StdAfx.h \
        $$SEVENZIP_ROOT/CPP/Common/StdInStream.h \
        $$SEVENZIP_ROOT/CPP/Common/StdOutStream.h \
        $$SEVENZIP_ROOT/CPP/Common/StringConvert.h \
        $$SEVENZIP_ROOT/CPP/Common/StringToInt.h \
        $$SEVENZIP_ROOT/CPP/Common/TextConfig.h \
        $$SEVENZIP_ROOT/CPP/Common/UTFConvert.h \
        $$SEVENZIP_ROOT/CPP/Common/Wildcard.h \

    SOURCES += \
        $$LIB7ZIP_ROOT/src/7ZipArchive.cpp \
        $$LIB7ZIP_ROOT/src/7ZipArchiveItem.cpp \
        $$LIB7ZIP_ROOT/src/7ZipArchiveOpenCallback.cpp \
        $$LIB7ZIP_ROOT/src/7ZipCodecInfo.cpp \
        $$LIB7ZIP_ROOT/src/7ZipCompressCodecsInfo.cpp \
        $$LIB7ZIP_ROOT/src/7ZipDllHandler.cpp \
        $$LIB7ZIP_ROOT/src/7ZipFormatInfo.cpp \
        $$LIB7ZIP_ROOT/src/7ZipInStreamWrapper.cpp \
        $$LIB7ZIP_ROOT/src/7zipLibrary.cpp \
        $$LIB7ZIP_ROOT/src/7ZipObjectPtrArray.cpp \
        $$LIB7ZIP_ROOT/src/7ZipOpenArchive.cpp \
        $$LIB7ZIP_ROOT/src/HelperFuncs.cpp \
        $$LIB7ZIP_ROOT/src/GUIDs.cpp \
        $$LIB7ZIP_ROOT/src/OSFunctions_OS2.cpp \
        $$LIB7ZIP_ROOT/src/OSFunctions_UnixLike.cpp \
        $$LIB7ZIP_ROOT/src/OSFunctions_Win32.cpp \
        $$SEVENZIP_ROOT/CPP/Common/MyWindows.cpp \
        $$SEVENZIP_ROOT/CPP/Windows/PropVariant.cpp \

    PRECOMPILED_HEADER += $$SEVENZIP_ROOT/CPP/Common/StdAfx.h $$SEVENZIP_ROOT/CPP/Common/MyWindows.h
    precompile_header:!isEmpty(PRECOMPILED_HEADER) {
        DEFINES += USING_PCH
    }
}

unix {
#    DEFINES += QT7Z_STATIC_LINK
    DEFINES += EXTERNAL_CODECS
    DEFINES += _FILE_OFFSET_BITS=64
    DEFINES += _LARGEFILE_SOURCE
    DEFINES += _REENTRANT
    DEFINES += ENV_UNIX
    DEFINES += BREAK_HANDLER
    DEFINES += UNICODE
    DEFINES += _UNICODE
    DEFINES += UNIX_USE_WIN_FILE
#    DEFINES += LOCALE_IS_UTF8
    DEFINES += USE_LIB7Z_DLL

    LIBS += -ldl

    INCLUDEPATH += \
        $$P7ZIP_ROOT \
        $$P7ZIP_ROOT/CPP \
        $$P7ZIP_ROOT/CPP/include_windows \
        $$P7ZIP_ROOT/CPP/7zip/Archive/Common \
        $$P7ZIP_ROOT/CPP/Common \
        $$LIB7ZIP_ROOT/src \

    HEADERS += \
        $$LIB7ZIP_ROOT/src/7ZipArchiveOpenCallback.h \
        $$LIB7ZIP_ROOT/src/7ZipCodecInfo.h \
        $$LIB7ZIP_ROOT/src/7ZipCompressCodecsInfo.h \
        $$LIB7ZIP_ROOT/src/7ZipDllHandler.h \
        $$LIB7ZIP_ROOT/src/7ZipFormatInfo.h \
        $$LIB7ZIP_ROOT/src/7ZipFunctions.h \
        $$LIB7ZIP_ROOT/src/7ZipInStreamWrapper.h \
        $$LIB7ZIP_ROOT/src/HelperFuncs.h \
        $$LIB7ZIP_ROOT/src/OSFunctions.h \
        $$LIB7ZIP_ROOT/src/OSFunctions_OS2.h \
        $$LIB7ZIP_ROOT/src/OSFunctions_UnixLike.h \
        $$LIB7ZIP_ROOT/src/OSFunctions_Win32.h \
        $$LIB7ZIP_ROOT/src/lib7zip.h \
        $$P7ZIP_ROOT/CPP/Common/AutoPtr.h \
        $$P7ZIP_ROOT/CPP/Common/C_FileIO.h \
        $$P7ZIP_ROOT/CPP/Common/ComTry.h \
        $$P7ZIP_ROOT/CPP/Common/CommandLineParser.h \
        $$P7ZIP_ROOT/CPP/Common/Common.h \
        $$P7ZIP_ROOT/CPP/Common/Defs.h \
        $$P7ZIP_ROOT/CPP/Common/DynLimBuf.h \
        $$P7ZIP_ROOT/CPP/Common/DynamicBuffer.h \
        $$P7ZIP_ROOT/CPP/Common/IntToString.h \
        $$P7ZIP_ROOT/CPP/Common/Lang.h \
        $$P7ZIP_ROOT/CPP/Common/ListFileUtils.h \
        $$P7ZIP_ROOT/CPP/Common/MyBuffer.h \
        $$P7ZIP_ROOT/CPP/Common/MyBuffer2.h \
        $$P7ZIP_ROOT/CPP/Common/MyCom.h \
        $$P7ZIP_ROOT/CPP/Common/MyException.h \
        $$P7ZIP_ROOT/CPP/Common/MyGuidDef.h \
        $$P7ZIP_ROOT/CPP/Common/MyInitGuid.h \
        $$P7ZIP_ROOT/CPP/Common/MyLinux.h \
        $$P7ZIP_ROOT/CPP/Common/MyMap.h \
        $$P7ZIP_ROOT/CPP/Common/MyString.h \
        $$P7ZIP_ROOT/CPP/Common/MyTypes.h \
        $$P7ZIP_ROOT/CPP/Common/MyUnknown.h \
        $$P7ZIP_ROOT/CPP/Common/MyVector.h \
        $$P7ZIP_ROOT/CPP/Common/MyWindows.h \
        $$P7ZIP_ROOT/CPP/Common/MyXml.h \
        $$P7ZIP_ROOT/CPP/Common/NewHandler.h \
        $$P7ZIP_ROOT/CPP/Common/Random.h \
        $$P7ZIP_ROOT/CPP/Common/StdAfx.h \
        $$P7ZIP_ROOT/CPP/Common/StdInStream.h \
        $$P7ZIP_ROOT/CPP/Common/StdOutStream.h \
        $$P7ZIP_ROOT/CPP/Common/StringConvert.h \
        $$P7ZIP_ROOT/CPP/Common/StringToInt.h \
        $$P7ZIP_ROOT/CPP/Common/TextConfig.h \
        $$P7ZIP_ROOT/CPP/Common/UTFConvert.h \
        $$P7ZIP_ROOT/CPP/Common/Wildcard.h \
        $$P7ZIP_ROOT/CPP/myWindows/StdAfx.h \

    SOURCES += \
        $$LIB7ZIP_ROOT/src/7ZipArchive.cpp \
        $$LIB7ZIP_ROOT/src/7ZipArchiveItem.cpp \
        $$LIB7ZIP_ROOT/src/7ZipArchiveOpenCallback.cpp \
        $$LIB7ZIP_ROOT/src/7ZipCodecInfo.cpp \
        $$LIB7ZIP_ROOT/src/7ZipCompressCodecsInfo.cpp \
        $$LIB7ZIP_ROOT/src/7ZipDllHandler.cpp \
        $$LIB7ZIP_ROOT/src/7ZipFormatInfo.cpp \
        $$LIB7ZIP_ROOT/src/7ZipInStreamWrapper.cpp \
        $$LIB7ZIP_ROOT/src/7zipLibrary.cpp \
        $$LIB7ZIP_ROOT/src/7ZipObjectPtrArray.cpp \
        $$LIB7ZIP_ROOT/src/7ZipOpenArchive.cpp \
        $$LIB7ZIP_ROOT/src/HelperFuncs.cpp \
        $$LIB7ZIP_ROOT/src/OSFunctions_OS2.cpp \
        $$LIB7ZIP_ROOT/src/OSFunctions_UnixLike.cpp \
        $$LIB7ZIP_ROOT/src/OSFunctions_Win32.cpp \
        $$P7ZIP_ROOT/CPP/Common/MyWindows.cpp \
        $$P7ZIP_ROOT/CPP/Windows/PropVariant.cpp \

    PRECOMPILED_HEADER += $$P7ZIP_ROOT/CPP/myWindows/StdAfx.h
    precompile_header:!isEmpty(PRECOMPILED_HEADER) {
        DEFINES += USING_PCH
    }
}
