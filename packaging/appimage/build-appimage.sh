#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Builds Flatpak_File_Installer-<version>-<arch>.AppImage for the machine's
# architecture (x86_64 or aarch64). Needs the same Qt 6 / KF6 development
# packages as a normal build; see .github/workflows/ci.yml for the list.
#
# Usage: packaging/appimage/build-appimage.sh [work-dir]
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$(mkdir -p "${1:-$ROOT/build-appimage}" && cd "${1:-$ROOT/build-appimage}" && pwd)
ARCH=$(uname -m)
APP_ID=io.github.yuanliou.FlatpakFileInstaller
VERSION=$(sed -n 's/^project(flatpak-file-installer VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")

LINUXDEPLOY_VERSION=1-alpha-20251107-1
LINUXDEPLOY_QT_VERSION=1-alpha-20250213-1

# Ubuntu's Kirigami Addons package has no translations, so the About page
# would stay in English. The release this matches has them; they are
# compiled below for the languages in po/LINGUAS.
KIRIGAMI_ADDONS_VERSION=1.11.0
KIRIGAMI_ADDONS_SHA256=6dcdfa324f710f00887e6321212fc8582a3c4712450d129d91435bb1edd8d523

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

fetch_file() { # <file> <url>
    if [ ! -f "$TOOLS/$1" ]; then
        curl -fsSL -o "$TOOLS/$1.part" "$2"
        mv "$TOOLS/$1.part" "$TOOLS/$1"
    fi
}
ADDONS_TARBALL=kirigami-addons-$KIRIGAMI_ADDONS_VERSION.tar.xz
fetch_file "$ADDONS_TARBALL" "https://download.kde.org/stable/kirigami-addons/$ADDONS_TARBALL"
echo "$KIRIGAMI_ADDONS_SHA256  $TOOLS/$ADDONS_TARBALL" | sha256sum -c -
ADDONS_PO=$WORK/kirigami-addons-po
rm -rf "$ADDONS_PO"
mkdir -p "$ADDONS_PO"
tar -xf "$TOOLS/$ADDONS_TARBALL" -C "$ADDONS_PO" --strip-components=2 "kirigami-addons-$KIRIGAMI_ADDONS_VERSION/po"
for lang in $(grep -v '^#' "$ROOT/po/LINGUAS"); do
    if [ -f "$ADDONS_PO/$lang/kirigami-addons6.po" ]; then
        mkdir -p "$APPDIR/usr/share/locale/$lang/LC_MESSAGES"
        msgfmt -o "$APPDIR/usr/share/locale/$lang/LC_MESSAGES/kirigami-addons6.mo" "$ADDONS_PO/$lang/kirigami-addons6.po"
    else
        echo "warning: Kirigami Addons has no $lang translation" >&2
    fi
done

# Containers and CI runners usually have no FUSE.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${QMAKE:-$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake)}"
# QML imports to bundle: ours, plus ones only loaded at runtime.
export QML_SOURCES_PATHS="$ROOT/src/qml:$ROOT/packaging/appimage/qml"
export EXTRA_QT_MODULES="svg;waylandclient"
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
# Qt's XDG portal platform theme (native file dialogs through the desktop portal)
# and Plasma's (the user's KDE colors, icons and widget style; needs the Breeze
# style, installed with kde-style-breeze).
export DEPLOY_PLATFORM_THEMES=1
export OUTPUT="Flatpak_File_Installer-$VERSION-$ARCH.AppImage"

cd "$WORK"
PATH="$TOOLS:$PATH" "$TOOLS/linuxdeploy-$ARCH.AppImage" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/$APP_ID.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/$APP_ID.svg" \
    --plugin qt \
    --output appimage

echo "Built $WORK/$OUTPUT"
