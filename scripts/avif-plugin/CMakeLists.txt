cmake_minimum_required(VERSION 3.20)
project(QuickViewerAvifPlugin LANGUAGES CXX)

set(AVIF_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../qt-avif-image-plugin"
    CACHE PATH "qt-avif-image-plugin source directory")
find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(libavif CONFIG REQUIRED)

# Keep all AVIF codec dependencies inside the Windows plug-in. CMake's imported
# target supplies libavif's transitive libraries instead of a hand-written list.
if(WIN32)
    get_target_property(avif_library_type avif TYPE)
    if(NOT avif_library_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "Windows AVIF builds require the x64-windows-static-md triplet")
    endif()
endif()

add_library(qavif6 MODULE
    "${AVIF_SOURCE}/src/main.cpp"
    "${AVIF_SOURCE}/src/qavifhandler.cpp"
    "${AVIF_SOURCE}/src/qavifhandler_p.h"
    "${AVIF_SOURCE}/src/util_p.h"
    "${AVIF_SOURCE}/src/avif.json")
set_target_properties(qavif6 PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>")
target_compile_definitions(qavif6 PRIVATE QT_PLUGIN)
target_link_libraries(qavif6 PRIVATE Qt6::Gui avif)
