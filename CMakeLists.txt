cmake_minimum_required(VERSION 3.21)
project(VoStudio VERSION 0.2.0 LANGUAGES CXX)

option(VOSTUDIO_BUILD_TESTS "Build VoStudio engine tests" ON)
option(VOSTUDIO_BUILD_TOOLS "Build VoStudio command-line tools" ON)
option(VOSTUDIO_BUILD_APP "Build the cross-platform Qt Quick application" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(VoStudio.Engine)

if(VOSTUDIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(VoStudio.Tests)
endif()

if(VOSTUDIO_BUILD_TOOLS)
    add_subdirectory(tools/vostudio-render)
endif()

if(VOSTUDIO_BUILD_APP)
    find_package(Qt6 6.8 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2
        QuickDialogs2
        Multimedia
        Concurrent
    )
    qt_standard_project_setup(REQUIRES 6.8)
    add_subdirectory(VoStudio.App)
endif()
