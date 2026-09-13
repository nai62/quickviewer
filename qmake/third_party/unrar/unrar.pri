UNRAR_SOURCE_ROOT = $$clean_path($$PWD/../../../third_party/unrar)

HEADERS += $$UNRAR_SOURCE_ROOT/*.hpp

SOURCES += \
    $$UNRAR_SOURCE_ROOT/rar.cpp \
    $$UNRAR_SOURCE_ROOT/strlist.cpp \
    $$UNRAR_SOURCE_ROOT/strfn.cpp \
    $$UNRAR_SOURCE_ROOT/pathfn.cpp \
    $$UNRAR_SOURCE_ROOT/smallfn.cpp \
    $$UNRAR_SOURCE_ROOT/global.cpp \
    $$UNRAR_SOURCE_ROOT/file.cpp \
    $$UNRAR_SOURCE_ROOT/filefn.cpp \
    $$UNRAR_SOURCE_ROOT/filcreat.cpp \
    $$UNRAR_SOURCE_ROOT/archive.cpp \
    $$UNRAR_SOURCE_ROOT/arcread.cpp \
    $$UNRAR_SOURCE_ROOT/unicode.cpp \
    $$UNRAR_SOURCE_ROOT/system.cpp \
    $$UNRAR_SOURCE_ROOT/isnt.cpp \
    $$UNRAR_SOURCE_ROOT/crypt.cpp \
    $$UNRAR_SOURCE_ROOT/crc.cpp \
    $$UNRAR_SOURCE_ROOT/rawread.cpp \
    $$UNRAR_SOURCE_ROOT/encname.cpp \
    $$UNRAR_SOURCE_ROOT/resource.cpp \
    $$UNRAR_SOURCE_ROOT/match.cpp \
    $$UNRAR_SOURCE_ROOT/timefn.cpp \
    $$UNRAR_SOURCE_ROOT/rdwrfn.cpp \
    $$UNRAR_SOURCE_ROOT/consio.cpp \
    $$UNRAR_SOURCE_ROOT/options.cpp \
    $$UNRAR_SOURCE_ROOT/errhnd.cpp \
    $$UNRAR_SOURCE_ROOT/rarvm.cpp \
    $$UNRAR_SOURCE_ROOT/secpassword.cpp \
    $$UNRAR_SOURCE_ROOT/rijndael.cpp \
    $$UNRAR_SOURCE_ROOT/getbits.cpp \
    $$UNRAR_SOURCE_ROOT/sha1.cpp \
    $$UNRAR_SOURCE_ROOT/sha256.cpp \
    $$UNRAR_SOURCE_ROOT/blake2s.cpp \
    $$UNRAR_SOURCE_ROOT/hash.cpp \
    $$UNRAR_SOURCE_ROOT/extinfo.cpp \
    $$UNRAR_SOURCE_ROOT/extract.cpp \
    $$UNRAR_SOURCE_ROOT/volume.cpp \
    $$UNRAR_SOURCE_ROOT/list.cpp \
    $$UNRAR_SOURCE_ROOT/find.cpp \
    $$UNRAR_SOURCE_ROOT/unpack.cpp \
    $$UNRAR_SOURCE_ROOT/headers.cpp \
    $$UNRAR_SOURCE_ROOT/threadpool.cpp \
    $$UNRAR_SOURCE_ROOT/rs16.cpp \
    $$UNRAR_SOURCE_ROOT/cmddata.cpp \
    $$UNRAR_SOURCE_ROOT/ui.cpp \
    $$UNRAR_SOURCE_ROOT/largepage.cpp \
    $$UNRAR_SOURCE_ROOT/motw.cpp \

contains(DEFINES, RAR_BUILD_UNRAR) {
    SOURCES += \
        $$UNRAR_SOURCE_ROOT/filestr.cpp \
        $$UNRAR_SOURCE_ROOT/recvol.cpp \
        $$UNRAR_SOURCE_ROOT/rs.cpp \
        $$UNRAR_SOURCE_ROOT/scantree.cpp \
        $$UNRAR_SOURCE_ROOT/qopen.cpp \

}

contains(DEFINES, RAR_BUILD_LIB) {
    SOURCES += \
        $$UNRAR_SOURCE_ROOT/filestr.cpp \
        $$UNRAR_SOURCE_ROOT/scantree.cpp \
        $$UNRAR_SOURCE_ROOT/dll.cpp \
        $$UNRAR_SOURCE_ROOT/qopen.cpp \

}

