#!/bin/sh
# Builds a busybox-only runtime and a hello app, then exports both as .flatpak bundles.
# Needs no network (no Flathub). Usage: sh build_test_bundles.sh <workdir>
set -e
W=${1:?workdir}; mkdir -p "$W"; cd "$W"
mkdir -p rt/usr/bin rt/files
cp /bin/busybox rt/usr/bin/; for c in sh echo cat; do ln -sf busybox rt/usr/bin/$c; done
printf '[Runtime]\nname=org.spike.Platform\nruntime=org.spike.Platform/x86_64/1\nsdk=org.spike.Platform/x86_64/1\n' > rt/metadata
flatpak build-export --runtime repo rt 1
flatpak build-bundle --runtime repo platform.flatpak org.spike.Platform 1
# build-init needs the SDK installed somewhere; use a throwaway installation
FLATPAK_USER_DIR="$W/builder-inst" flatpak --user install -y --noninteractive platform.flatpak
FLATPAK_USER_DIR="$W/builder-inst" flatpak build-init app org.spike.Hello org.spike.Platform org.spike.Platform 1
mkdir -p app/files/bin app/files/share/applications
printf '#!/bin/sh\necho hello from sandbox\n' > app/files/bin/hello; chmod +x app/files/bin/hello
printf '[Desktop Entry]\nName=Hello\nExec=hello\nType=Application\n' > app/files/share/applications/org.spike.Hello.desktop
flatpak build-finish app --command=hello
flatpak build-export repo app stable
flatpak build-bundle repo hello.flatpak org.spike.Hello stable
