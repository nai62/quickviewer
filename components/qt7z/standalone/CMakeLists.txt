cmake_minimum_required(VERSION 2.8.11)

project(qt7z-standalone)

enable_testing()

find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Test
)

add_subdirectory(.. ${CMAKE_CURRENT_BINARY_DIR}/Qt7z)
add_subdirectory(../../../tests/qt7z ${CMAKE_CURRENT_BINARY_DIR}/Qt7zTest)
