cmake_minimum_required(VERSION 2.8.11)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules
)

project(qt7z)

if(POLICY CMP0020)
    cmake_policy(SET CMP0020 NEW)
endif()

if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

find_package(Qt5 REQUIRED COMPONENTS Core)

file(GLOB QT7Z_RESOURCES
    "*.qrc"
)
qt5_add_resources(QT7Z_RESOURCES_RCC ${QT7Z_RESOURCES})

file(GLOB QT7Z_SRCS
    "*.cpp"
    "*.h"
)

set(QT7Z_THIRD_PARTY_ROOT
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
)

if(WIN32 OR OSX)
    include(7zip.cmake)
else()
    include_directories(
        ${QT7Z_THIRD_PARTY_ROOT}/p7zip/CPP
        ${QT7Z_THIRD_PARTY_ROOT}/p7zip/CPP/myWindows
        ${QT7Z_THIRD_PARTY_ROOT}/p7zip/CPP/include_windows
    )
    include(p7zip.cmake)
endif()

set(QT7Z_THIRD_PARTY_SRCS)
foreach(source ${7Z_SRCS})
    list(APPEND QT7Z_THIRD_PARTY_SRCS
        ${QT7Z_THIRD_PARTY_ROOT}/${source}
    )
endforeach()
set(7Z_SRCS ${QT7Z_THIRD_PARTY_SRCS})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(
    -DQT7Z_LIBRARY
    -DUNICODE
    -D_UNICODE
)

if(UNIX)
    if(APPLE)
        add_definitions(-DENV_MACOSX)
    else()
        add_definitions(-DENV_LINUX)
    endif()
endif()

add_library(qt7z
    ${QT7Z_SRCS}
    ${QT7Z_RESOURCES_RCC}
    ${7Z_SRCS}
)
target_link_libraries(qt7z
    Qt5::Core
    ${CMAKE_DL_LIBS}
)
