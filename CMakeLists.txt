cmake_minimum_required(VERSION 3.24)
project(rigpa VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.10 exactly: vip's Ubuntu 26.04 ships 6.10.2, and the Mac builds against the same version
# from .qt/ (mise run qt).
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Svg Test)
qt_standard_project_setup(REQUIRES 6.10)

# Everything that isn't QML, as the QML module Rigpa.Core: the tests link it without a window.
qt_add_library(rigpa_core STATIC)
qt_add_qml_module(rigpa_core
    URI Rigpa.Core
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Rigpa/Core
    SOURCES
        src/app/App.cpp src/app/App.h
        src/app/Config.cpp src/app/Config.h
        src/app/DevInput.cpp src/app/DevInput.h
        src/app/FocusRing.cpp src/app/FocusRing.h
        src/app/Navigator.cpp src/app/Navigator.h
        src/app/Settings.cpp src/app/Settings.h
)
target_include_directories(rigpa_core PUBLIC src)
# The generated QML type registrations include each header by its bare name.
target_include_directories(rigpa_core PRIVATE src/app)
target_link_libraries(rigpa_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml)

qt_add_executable(rigpa MACOSX_BUNDLE src/main.cpp)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(rigpa
    URI Rigpa
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Shell.qml
        qml/Menu.qml
        qml/Header.qml
        qml/Footer.qml
        qml/InfoPanel.qml
        qml/views/RigView.qml
        qml/views/BrowseView.qml
        qml/views/SettingsView.qml
        qml/controls/MenuButton.qml
        qml/controls/Panel.qml
        qml/controls/ValueBar.qml
        qml/controls/Toggle.qml
        qml/controls/Segmented.qml
        qml/controls/HeaderButton.qml
    RESOURCES
        resources/icons/rigpa.svg
        resources/icons/folder.svg
        resources/icons/settings.svg
)
target_link_libraries(rigpa PRIVATE rigpa_core rigpa_coreplugin Qt6::Quick Qt6::QuickControls2 Qt6::Svg)
set_target_properties(rigpa PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER cc.hemsley.rigpa)

enable_testing()
add_subdirectory(tests)
