cmake_minimum_required(VERSION 3.16)
project(plasma-ai-usage-monitor VERSION 8.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# Must set before KDEInstallDirs so it uses KF6 paths (knotifications6, etc.)
set(QT_MAJOR_VERSION 6)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Network Sql)
find_package(OpenSSL REQUIRED)
find_package(Plasma REQUIRED)
find_package(KF6Wallet REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6I18n REQUIRED)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

# Install the plasmoid package
plasma_install_package(package com.github.loofi.aiusagemonitor)

# Install KNotification event definitions
install(FILES plasma_applet_com.github.loofi.aiusagemonitor.notifyrc
        DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

# Install AppStream metainfo for Discover / AppStream catalogs
install(FILES com.github.loofi.aiusagemonitor.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})

# Build the C++ QML plugin
add_subdirectory(plugin)
