#-------------------------------------------------
#
# Project created by QtCreator 2014-02-08T18:57:16
#
#-------------------------------------------------

#QT       -= core
QT       += core gui

TARGET = zimg
TEMPLATE = lib
CONFIG += staticlib
CONFIG += warn_off
CONFIG += c++17

DEFINES += ZIMG_X86

ZIMG_SOURCE_ROOT = $$clean_path($$PWD/../../third_party/zimg)

win32-msvc* {
    QMAKE_CXXFLAGS += /wd4819 /wd4996
    !CONFIG(debug, debug|release) {
        QMAKE_CXXFLAGS += /GL /W3 /Gy /Gm- /Gd /Oi
    }
}

*clang* || *g++* {
    QMAKE_CXXFLAGS += -O2 -MD -MP -include $$PWD/StdAfx.h
    !CONFIG(debug, debug|release) {
    }
}

macos {
    QMAKE_CXXFLAGS += -O2 -MD -MP
    !CONFIG(debug, debug|release) {
    }
}

SOURCES += \
    $$ZIMG_SOURCE_ROOT/src/zimg/api/zimg.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/colorspace.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/colorspace_param.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/gamma.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/graph.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/matrix3.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/operation.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/operation_impl.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_x86.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/cpuinfo.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/x86/cpuinfo_x86.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/libm_wrapper.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/matrix.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/x86/x86util.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/blue.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/depth.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/depth_convert.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_x86.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/dither.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_x86.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/quantize.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/basic_filter.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/filtergraph.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/graphbuilder.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/graphnode.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/filter.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/resize.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/resize_impl.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_x86.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/unresize/bilinear.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/unresize/unresize.cpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/unresize/unresize_impl.cpp \
    qzimg.cpp

HEADERS += \
    $$ZIMG_SOURCE_ROOT/src/zimg/api/zimg.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/api/zimg++.hpp \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/colorspace.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/colorspace_param.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/gamma.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/graph.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/matrix3.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/operation.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/operation_impl.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_x86.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/align.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/alloc.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/builder.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/ccdep.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/checked_int.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/cpuinfo.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/x86/cpuinfo_x86.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/except.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/libm_wrapper.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/make_unique.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/matrix.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/pixel.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/static_map.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/x86/x86util.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/common/zassert.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/blue.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/depth.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/depth_convert.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_x86.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/dither.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_x86.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/f16c_x86.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/depth/quantize.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/basic_filter.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/filtergraph.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/graphbuilder.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/graphnode.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/image_buffer.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/graph/image_filter.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/filter.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/resize.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/resize_impl.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_x86.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/unresize/bilinear.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/unresize/unresize.h \
    $$ZIMG_SOURCE_ROOT/src/zimg/unresize/unresize_impl.h \
    $$ZIMG_SOURCE_ROOT/src/testcommon/aligned_malloc.h \
    qzimg.h

INCLUDEPATH += \
    $$ZIMG_SOURCE_ROOT/src/zimg  $$ZIMG_SOURCE_ROOT/src/testcommon

DESTDIR = ../../lib

# CPU specialized codes

win32-msvc* {
    SOURCES += \
        $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_sse.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_sse.cpp \

    SOURCES += \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_sse2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_sse2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/f16c_sse2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_sse2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_sse2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/error_diffusion_sse2.cpp \

    SOURCES += \
        $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_avx.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_avx.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/f16c_ivb.cpp \

    SOURCES += \
        $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_avx2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_avx2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_avx2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/error_diffusion_avx2.cpp \
        $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_avx2.cpp \

}

*clang* || *g++* {
    SOURCES_SSE = \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_sse.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_sse.cpp \

    sse.name = sse
    sse.input = SOURCES_SSE
    sse.dependency_type = TYPE_C
    sse.variable_out = OBJECTS
    sse.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    sse.commands = $${QMAKE_CXX} $(CXXFLAGS) -msse $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    SOURCES_SSE2 = \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/f16c_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/error_diffusion_sse2.cpp \

    sse2.name = sse2
    sse2.input = SOURCES_SSE2
    sse2.dependency_type = TYPE_C
    sse2.variable_out = OBJECTS
    sse2.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    sse2.commands = $${QMAKE_CXX} $(CXXFLAGS) -msse2 $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    SOURCES_AVX = \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_avx.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_avx.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/f16c_ivb.cpp \

    avx.name = avx
    avx.input = SOURCES_AVX
    avx.dependency_type = TYPE_C
    avx.variable_out = OBJECTS
    avx.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    avx.commands = $${QMAKE_CXX} $(CXXFLAGS) -mavx -mf16c $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    SOURCES_AVX2 = \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/error_diffusion_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_avx2.cpp \

    avx2.name = avx2
    avx2.input = SOURCES_AVX2
    avx2.dependency_type = TYPE_C
    avx2.variable_out = OBJECTS
    avx2.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    avx2.commands = $${QMAKE_CXX} $(CXXFLAGS)  -mavx2 -mf16c -mfma $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    QMAKE_EXTRA_COMPILERS += sse sse2 avx avx2
}

macos {
    SOURCES_SSE = \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_sse.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_sse.cpp \

    sse.name = sse
    sse.input = SOURCES_SSE
    sse.dependency_type = TYPE_C
    sse.variable_out = OBJECTS
    sse.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    sse.commands = $${QMAKE_CXX} $(CXXFLAGS) -msse $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    SOURCES_SSE2 = \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/f16c_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_sse2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/error_diffusion_sse2.cpp \

    sse2.name = sse2
    sse2.input = SOURCES_SSE2
    sse2.dependency_type = TYPE_C
    sse2.variable_out = OBJECTS
    sse2.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    sse2.commands = $${QMAKE_CXX} $(CXXFLAGS) -msse2 $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    SOURCES_AVX = \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_avx.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_avx.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/f16c_ivb.cpp \

    avx.name = avx
    avx.input = SOURCES_AVX
    avx.dependency_type = TYPE_C
    avx.variable_out = OBJECTS
    avx.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    avx.commands = $${QMAKE_CXX} $(CXXFLAGS) -mavx -mf16c $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    SOURCES_AVX2 = \
            $$ZIMG_SOURCE_ROOT/src/zimg/colorspace/x86/operation_impl_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/depth_convert_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/dither_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/depth/x86/error_diffusion_avx2.cpp \
            $$ZIMG_SOURCE_ROOT/src/zimg/resize/x86/resize_impl_avx2.cpp \

    avx2.name = avx2
    avx2.input = SOURCES_AVX2
    avx2.dependency_type = TYPE_C
    avx2.variable_out = OBJECTS
    avx2.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    avx2.commands = $${QMAKE_CXX} $(CXXFLAGS)  -mavx2 -mf16c -mfma $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    QMAKE_EXTRA_COMPILERS += sse sse2 avx avx2
}
