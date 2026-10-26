"""Spike: inspect a .flatpak bundle and install it at user or system level via libflatpak."""
import sys
import gi
gi.require_version("Flatpak", "1.0")
from gi.repository import Flatpak, Gio, GLib

path, level = sys.argv[1], sys.argv[2]
f = Gio.File.new_for_path(path)

# 1) Read bundle metadata without installing (what the installer window would show)
b = Flatpak.BundleRef.new(f)
print("name      :", b.get_name(), b.get_arch(), b.get_branch())
print("kind      :", b.get_kind().value_nick)
print("inst size :", b.get_installed_size())
print("runtime   :", (b.get_metadata().get_data().decode().split("runtime=")[1].split("\n")[0]
                      if b.get_kind() == Flatpak.RefKind.APP else "-"))
print("origin url:", b.get_origin())
print("rt repo   :", b.get_runtime_repo_url())
print("appstream :", "yes" if b.get_appstream() else "no", "| icon64:", "yes" if b.get_icon(64) else "no")

# 2) Install into chosen level
inst = (Flatpak.Installation.new_user() if level == "user"
        else Flatpak.Installation.new_system())
print("target    :", inst.get_path().get_path())

t = Flatpak.Transaction.new_for_installation(inst)
t.add_install_bundle(f, None)
t.connect("new-operation", lambda _t, op, prog: print("  op:", op.get_operation_type().value_nick, op.get_ref()))
t.connect("operation-error", lambda _t, op, err, d: (print("  ERROR:", err.message), False)[1])
try:
    t.run()
    print("RESULT    : installed OK")
except GLib.Error as e:
    print("RESULT    : failed ->", e.message)
    sys.exit(1)
