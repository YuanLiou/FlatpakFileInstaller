# SPDX-License-Identifier: Apache-2.0

# Everything that doesn't need a GUI, so tests can link it.
add_library(installer_core STATIC
    flathubclient.cpp
    flatpakcli.cpp
    hostcommand.cpp
    hostenvironment.cpp
    packagefile.cpp
    permissions.cpp
)
target_include_directories(installer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(installer_core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Network
    PRIVATE KF6::Archive PkgConfig::GLIB
)

add_executable(flatpak-app-installer main.cpp)

qt_add_qml_module(flatpak-app-installer
    URI io.github.yuanliou.FlatpakAppInstaller
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppTile.qml
        qml/DropTarget.qml
        qml/PermissionsCard.qml
    SOURCES
        filedroparea.cpp
        filedroparea.h
        installer.cpp
        installer.h
        permissiontext.cpp
        permissiontext.h
)

set_source_files_properties(${PROJECT_SOURCE_DIR}/data/icons/${APP_ID}.svg PROPERTIES QT_RESOURCE_ALIAS app.svg)
qt_add_resources(flatpak-app-installer icons
    PREFIX /icons
    BASE icons
    FILES
        ${PROJECT_SOURCE_DIR}/data/icons/${APP_ID}.svg
        icons/arrow.svg
        icons/computer.svg
        icons/done.svg
        icons/dot.svg
        icons/error.svg
        icons/info.svg
        icons/package.svg
        icons/user.svg
)

target_compile_definitions(flatpak-app-installer PRIVATE
    TRANSLATION_DOMAIN="${TRANSLATION_DOMAIN}"
    APP_ID="${APP_ID}"
    APP_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(flatpak-app-installer PRIVATE
    installer_core
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
)

install(TARGETS flatpak-app-installer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
