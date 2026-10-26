#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Builds the test bundles without network access:
#   runtime.flatpak  org.test.Platform//1, a busybox-only runtime
#   notes.flatpak    org.example.Notes//stable with AppStream data (en + zh_TW),
#                    a 64px icon, permissions and a runtime-repo URL
#   notes-offline.flatpak  the same app without the runtime-repo URL; flatpak
#                    fetches that URL on every install, even when the runtime
#                    is already there, so the offline install test needs this
#   hello.flatpak    org.spike.Hello//stable, a minimal app with no AppStream data
#   paint.flatpak    org.example.Paint//stable, no AppStream data but an
#                    exported desktop file (en + zh_TW) and an SVG icon
#   sketch.flatpak   org.example.Sketch//stable, no AppStream data, PNG icons
#                    only (the biggest is a symlink into files/) and a file
#                    big enough to split the bundle into several parts
#   hub-repo/, hub.flatpakrepo
#                    a local repository with the runtime and the notes app,
#                    standing in for Flathub when testing that a missing
#                    runtime is fetched and that a ref's permissions are read
#   notes-hub.flatpakref  points to the notes app in hub-repo
#
# notes.flatpak, hello.flatpak, paint.flatpak and sketch.flatpak are checked
# in (built on x86_64);
# runtime.flatpak is not, because it contains a busybox binary. The install
# test runs this script to build all three for the machine it runs on.
#
# Usage: make-fixtures.sh <output-dir>
# Needs: flatpak, busybox (static), python3
set -eu

OUT=$(mkdir -p "$1" && cd "$1" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"
export FLATPAK_USER_DIR="$WORK/installation"
BUSYBOX=$(command -v busybox)
# A commit's ID covers its timestamp. With a fixed one, exporting the same
# build to two repositories gives the same commit, which the install test
# relies on (the "remote" must have exactly the build that was installed).
# Exported icons are checked in a sandbox, which can't start inside a
# container or chroot; these icons are our own.
EXPORT="flatpak build-export --timestamp=2026-01-01T00:00:00Z --disable-sandbox"

build_runtime() { # <id>
    rm -rf rt && mkdir -p rt/usr/bin rt/files
    cp "$BUSYBOX" rt/usr/bin/busybox
    for c in sh echo cat; do ln -sf busybox rt/usr/bin/$c; done
    printf '[Runtime]\nname=%s\nruntime=%s/%s/1\nsdk=%s/%s/1\n' "$1" "$1" "$(flatpak --default-arch)" "$1" "$(flatpak --default-arch)" > rt/metadata
    $EXPORT --runtime repo rt 1 >/dev/null
}

build_runtime org.test.Platform
flatpak build-bundle --runtime repo "$OUT/runtime.flatpak" org.test.Platform 1
$EXPORT --runtime "$OUT/hub-repo" rt 1 >/dev/null

build_runtime org.spike.Platform
flatpak build-bundle --runtime repo spike.flatpak org.spike.Platform 1
flatpak --user install --noninteractive --assumeyes "$OUT/runtime.flatpak" >/dev/null
flatpak --user install --noninteractive --assumeyes spike.flatpak >/dev/null

# org.example.Notes
flatpak build-init notes org.example.Notes org.test.Platform org.test.Platform 1
mkdir -p notes/files/bin notes/files/share/app-info/xmls notes/files/share/app-info/icons/flatpak/64x64
printf '#!/bin/sh\necho notes\n' > notes/files/bin/notes
chmod +x notes/files/bin/notes
cat > notes.xml <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<components version="0.8" origin="flatpak">
  <component type="desktop">
    <id>org.example.Notes</id>
    <name>Notes</name>
    <name xml:lang="zh_TW">筆記本</name>
    <summary>Write things down</summary>
    <summary xml:lang="zh_TW">把事情記下來</summary>
    <developer_name>Example Inc.</developer_name>
    <releases><release version="1.2.3" date="2026-01-01"/></releases>
  </component>
</components>
XML
gzip -c notes.xml > notes/files/share/app-info/xmls/org.example.Notes.xml.gz
make_png() { # <file> <size>
    python3 -I - "$1" "$2" <<'PY'
import struct, sys, zlib
size = int(sys.argv[2])
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
rows = b''.join(b'\x00' + bytes([0x33, 0x99, 0xff, 0xff] * size) for _ in range(size))
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
PY
}
make_png notes/files/share/app-info/icons/flatpak/64x64/org.example.Notes.png 64
flatpak build-finish notes --command=notes --share=network --socket=wayland --socket=pulseaudio --filesystem=home --device=dri >/dev/null
$EXPORT repo notes stable >/dev/null
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo repo "$OUT/notes.flatpak" org.example.Notes stable
flatpak build-bundle repo "$OUT/notes-offline.flatpak" org.example.Notes stable
$EXPORT "$OUT/hub-repo" notes stable >/dev/null
flatpak build-update-repo "$OUT/hub-repo" >/dev/null
printf '[Flatpak Repo]\nTitle=Test Hub\nUrl=file://%s/hub-repo\n' "$OUT" > "$OUT/hub.flatpakrepo"
printf '[Flatpak Ref]\nName=org.example.Notes\nBranch=stable\nTitle=Notes\nUrl=file://%s/hub-repo\n' "$OUT" > "$OUT/notes-hub.flatpakref"

# org.spike.Hello
flatpak build-init hello org.spike.Hello org.spike.Platform org.spike.Platform 1
mkdir -p hello/files/bin
printf '#!/bin/sh\necho hello from sandbox\n' > hello/files/bin/hello
chmod +x hello/files/bin/hello
flatpak build-finish hello --command=hello >/dev/null
$EXPORT repo hello stable >/dev/null
flatpak build-bundle repo "$OUT/hello.flatpak" org.spike.Hello stable

# org.example.Paint
flatpak build-init paint org.example.Paint org.test.Platform org.test.Platform 1
mkdir -p paint/files/bin paint/files/share/applications paint/files/share/icons/hicolor/scalable/apps
printf '#!/bin/sh\necho paint\n' > paint/files/bin/paint
chmod +x paint/files/bin/paint
cat > paint/files/share/applications/org.example.Paint.desktop <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Paint
Name[zh_TW]=小畫家
Comment=Draw pictures
Comment[zh_TW]=畫圖
Exec=paint
Icon=org.example.Paint
DESKTOP
printf '<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48"><rect width="48" height="48" rx="8" fill="#3399ff"/></svg>\n' \
    > paint/files/share/icons/hicolor/scalable/apps/org.example.Paint.svg
flatpak build-finish paint --command=paint >/dev/null
$EXPORT repo paint stable >/dev/null
flatpak build-bundle repo "$OUT/paint.flatpak" org.example.Paint stable

# org.example.Sketch
flatpak build-init sketch org.example.Sketch org.test.Platform org.test.Platform 1
mkdir -p sketch/files/bin sketch/files/share/applications sketch/files/share/sketch
printf '#!/bin/sh\necho sketch\n' > sketch/files/bin/sketch
chmod +x sketch/files/bin/sketch
printf '[Desktop Entry]\nType=Application\nName=Sketch\nExec=sketch\nIcon=org.example.Sketch\n' \
    > sketch/files/share/applications/org.example.Sketch.desktop
for size in 32 64 128; do
    mkdir -p sketch/files/share/icons/hicolor/${size}x${size}/apps
    make_png sketch/files/share/icons/hicolor/${size}x${size}/apps/org.example.Sketch.png $size
done
# OSTree starts a new part after about 32 MB; zeros compress to almost nothing.
head -c 40000000 /dev/zero > sketch/files/share/sketch/brushes.dat
flatpak build-finish sketch --command=sketch >/dev/null
ICONS=sketch/export/share/icons/hicolor/128x128/apps
rm "$ICONS/org.example.Sketch.png"
ln -s ../../../../../../files/share/icons/hicolor/128x128/apps/org.example.Sketch.png "$ICONS/org.example.Sketch.png"
$EXPORT repo sketch stable >/dev/null
flatpak build-bundle repo "$OUT/sketch.flatpak" org.example.Sketch stable
