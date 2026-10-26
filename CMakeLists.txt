# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.22)

project(flatpak-app-installer VERSION 0.1.0 LANGUAGES CXX)

set(APP_ID "io.github.yuanliou.FlatpakAppInstaller")
set(TRANSLATION_DOMAIN "flatpak-app-installer")

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.9.0") # KAboutLicense::Apache_V2
set(KDE_COMPILERSETTINGS_LEVEL "6.0")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2)
qt_policy(SET QTP0001 NEW) # QML modules under qrc:/qt/qml/
qt_policy(SET QTP0004 NEW)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS I18n CoreAddons Archive)
# Kirigami is only used from QML, but fail early if it is missing.
find_package(KF6Kirigami ${KF_MIN_VERSION} REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
find_package(Gettext REQUIRED)

add_subdirectory(po)
add_subdirectory(data)
add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
