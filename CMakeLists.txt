cmake_minimum_required(VERSION 3.24)
project(rigpa VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.10 exactly: vip's Ubuntu 26.04 ships 6.10.2, and the Mac builds against the same version
# from .qt/ (mise run qt).
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Svg Test)
qt_standard_project_setup(REQUIRES 6.10)

# Everything that isn't QML: the tests link this without a window.
qt_add_library(rigpa_core STATIC)
target_sources(rigpa_core PRIVATE
    src/app/buildinfo.h
)
target_include_directories(rigpa_core PUBLIC src)
target_link_libraries(rigpa_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml)

qt_add_executable(rigpa MACOSX_BUNDLE src/main.cpp)
qt_add_qml_module(rigpa
    URI Rigpa
    QML_FILES
        qml/Main.qml
)
target_link_libraries(rigpa PRIVATE rigpa_core Qt6::Quick Qt6::QuickControls2 Qt6::Svg)
set_target_properties(rigpa PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER cc.hemsley.rigpa)

enable_testing()
add_subdirectory(tests)
