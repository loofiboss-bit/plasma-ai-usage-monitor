cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_VALUE
     LIMIT_COUNT 1 REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+$")
if(NOT PROJECT_VERSION_VALUE)
    message(FATAL_ERROR "VERSION must contain one MAJOR.MINOR.PATCH value")
endif()
project(plasma-ai-usage-monitor VERSION "${PROJECT_VERSION_VALUE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# Must set before KDEInstallDirs so it uses KF6 paths (knotifications6, etc.)
set(QT_MAJOR_VERSION 6)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Network Sql Concurrent)
qt_policy(SET QTP0001 NEW)
find_package(OpenSSL REQUIRED)
find_package(Plasma REQUIRED)
find_package(KF6Wallet REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6I18n REQUIRED)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME demo_contract
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_demo_contract.py"
    )
endif()

# Install the plasmoid package
plasma_install_package(package com.github.loofi.aiusagemonitor)

# Install KNotification event definitions
install(FILES plasma_applet_com.github.loofi.aiusagemonitor.notifyrc
        DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

# Install the metadata icon into the icon theme so Plasma's widget explorer can
# resolve KPlugin.Icon without falling back to a generic document icon.
install(FILES assets/logo.svg
        RENAME com.github.loofi.aiusagemonitor.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

# Install AppStream metainfo for Discover / AppStream catalogs
install(FILES com.github.loofi.aiusagemonitor.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})

# Build the C++ QML plugin
add_subdirectory(plugin)
