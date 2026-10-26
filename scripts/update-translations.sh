#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Extracts user-visible text into po/flatpak-app-installer.pot and merges it
# into every po/<lang>.po listed in po/LINGUAS.
#
# Adding a language: add its code (e.g. "ja") to po/LINGUAS, run this
# script, then translate po/<code>.po.
set -eu

cd "$(dirname "$0")/.."
DOMAIN=flatpak-app-installer
POT=po/$DOMAIN.pot

KEYWORDS="--keyword=i18n:1 --keyword=i18nc:1c,2 --keyword=i18np:1,2 --keyword=i18ncp:1c,2,3"
VERSION=$(sed -n 's/^project(flatpak-app-installer VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
COMMON="--from-code=UTF-8 --add-location=file --package-version=$VERSION --add-comments=TRANSLATORS --sort-by-file --package-name=$DOMAIN --msgid-bugs-address=https://github.com/YuanLiou/FlatpakAppInstaller/issues"

# shellcheck disable=SC2086
xgettext $COMMON --language=C++ --kde $KEYWORDS -o "$POT" $(find src -name '*.cpp' | sort)
# shellcheck disable=SC2086
xgettext $COMMON --language=JavaScript $KEYWORDS --join-existing -o "$POT" $(find src -name '*.qml' | sort)
# shellcheck disable=SC2086
xgettext $COMMON --language=Desktop --join-existing -o "$POT" data/*.desktop.in
# shellcheck disable=SC2086
GETTEXTDATADIRS=po xgettext $COMMON --join-existing -o "$POT" data/*.metainfo.xml.in

for lang in $(grep -v '^#' po/LINGUAS); do
    if [ -f "po/$lang.po" ]; then
        msgmerge --quiet --update --backup=none --add-location=file "po/$lang.po" "$POT"
    else
        msginit --no-translator --locale="$lang" --input="$POT" --output-file="po/$lang.po"
    fi
done
