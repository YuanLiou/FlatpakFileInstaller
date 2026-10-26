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
#   hub-repo/, hub.flatpakrepo
#                    a local repository with the runtime and the notes app,
#                    standing in for Flathub when testing that a missing
#                    runtime is fetched and that a ref's permissions are read
#   notes-hub.flatpakref  points to the notes app in hub-repo
#
# notes.flatpak and hello.flatpak are checked in (built on x86_64);
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
EXPORT="flatpak build-export --timestamp=2026-01-01T00:00:00Z"

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
python3 -I - notes/files/share/app-info/icons/flatpak/64x64/org.example.Notes.png <<'PY'
import struct, sys, zlib
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
rows = b''.join(b'\x00' + bytes([0x33, 0x99, 0xff, 0xff] * 64) for _ in range(64))
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 64, 64, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
PY
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
