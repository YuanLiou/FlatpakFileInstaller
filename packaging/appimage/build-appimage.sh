#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Builds Flatpak_App_Installer-<version>-<arch>.AppImage for the machine's
# architecture (x86_64 or aarch64). Needs the same Qt 6 / KF6 development
# packages as a normal build; see .github/workflows/ci.yml for the list.
#
# Usage: packaging/appimage/build-appimage.sh [work-dir]
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$(mkdir -p "${1:-$ROOT/build-appimage}" && cd "${1:-$ROOT/build-appimage}" && pwd)
ARCH=$(uname -m)
APP_ID=io.github.yuanliou.FlatpakAppInstaller
VERSION=$(sed -n 's/^project(flatpak-app-installer VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")

LINUXDEPLOY_VERSION=1-alpha-20251107-1
LINUXDEPLOY_QT_VERSION=1-alpha-20250213-1

TOOLS=$WORK/tools
APPDIR=$WORK/AppDir
mkdir -p "$TOOLS"

fetch() { # <file> <url>
    if [ ! -x "$TOOLS/$1" ]; then
        curl -fsSL -o "$TOOLS/$1.part" "$2"
        chmod +x "$TOOLS/$1.part"
        mv "$TOOLS/$1.part" "$TOOLS/$1"
    fi
}
fetch "linuxdeploy-$ARCH.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/$LINUXDEPLOY_VERSION/linuxdeploy-$ARCH.AppImage"
fetch "linuxdeploy-plugin-qt-$ARCH.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/$LINUXDEPLOY_QT_VERSION/linuxdeploy-plugin-qt-$ARCH.AppImage"

cmake -S "$ROOT" -B "$WORK/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=OFF
cmake --build "$WORK/build"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$WORK/build"

# Containers and CI runners usually have no FUSE.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${QMAKE:-$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake)}"
# QML imports to bundle: ours, plus ones only loaded at runtime.
export QML_SOURCES_PATHS="$ROOT/src/qml:$ROOT/packaging/appimage/qml"
export EXTRA_QT_MODULES="svg;waylandclient"
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
export OUTPUT="Flatpak_App_Installer-$VERSION-$ARCH.AppImage"

cd "$WORK"
PATH="$TOOLS:$PATH" "$TOOLS/linuxdeploy-$ARCH.AppImage" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/$APP_ID.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/$APP_ID.svg" \
    --plugin qt \
    --output appimage

echo "Built $WORK/$OUTPUT"
