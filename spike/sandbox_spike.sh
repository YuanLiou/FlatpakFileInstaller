#!/bin/sh
# Spike 2: can an app running INSIDE a Flatpak sandbox install a .flatpak bundle on the host?
# Mirrors Warehouse's approach: --talk-name=org.freedesktop.Flatpak + flatpak-spawn --host flatpak ...
# Needs: flatpak, flatpak-xdg-utils (flatpak-spawn), busybox-static, a session + system D-Bus.
# Usage: dbus-run-session -- sh sandbox_spike.sh <workdir>
set -e
W=${1:?workdir}; HERE=$(cd "$(dirname "$0")" && pwd); mkdir -p "$W"; cd "$W"
export FLATPAK_USER_DIR="$W/inst"
SP=/usr/libexec/flatpak-xdg-utils/flatpak-spawn

# runtime = busybox + flatpak-spawn and its shared-lib closure
mkdir -p rt/usr/bin rt/usr/lib64 rt/files
cp /bin/busybox rt/usr/bin/; for c in sh echo cat ls id head; do ln -sf busybox rt/usr/bin/$c; done
cp $SP rt/usr/bin/
for l in $(ldd $SP | awk '/=>/{print $3} /ld-linux/{print $1}'); do cp -L "$l" rt/usr/lib64/; done
printf '[Runtime]\nname=org.spike.Platform\nruntime=org.spike.Platform/x86_64/1\nsdk=org.spike.Platform/x86_64/1\n' > rt/metadata
flatpak build-export --runtime repo rt 1
flatpak build-bundle --runtime repo platform.flatpak org.spike.Platform 1
flatpak --user install -y --noninteractive platform.flatpak

# the sandboxed "installer" app
flatpak build-init inst-app org.spike.Installer org.spike.Platform org.spike.Platform 1
mkdir -p inst-app/files/bin
cat > inst-app/files/bin/installer <<'IN'
#!/bin/sh
# $1 = bundle path as seen inside the sandbox; $2 = user|system
export LD_LIBRARY_PATH=/usr/lib64
exec flatpak-spawn --host env FLATPAK_USER_DIR="$FLATPAK_USER_DIR_HOST" flatpak install --$2 -y --noninteractive "$1"
IN
chmod +x inst-app/files/bin/installer
flatpak build-finish inst-app --command=installer --talk-name=org.freedesktop.Flatpak --env=FLATPAK_USER_DIR_HOST="$W/inst"
flatpak build-export repo inst-app stable
flatpak build-bundle repo installer.flatpak org.spike.Installer stable
flatpak --user install -y --noninteractive installer.flatpak

# the bundle to install, handed to the sandbox through the document portal (as a double-click would)
sh "$HERE/build_test_bundles.sh" "$W/target"
D=$(flatpak document-export --app=org.spike.Installer -r "$W/target/hello.flatpak")
echo "### sandbox sees bundle at: $D"
flatpak run --user org.spike.Installer "$D" user
flatpak list --user --columns=application
