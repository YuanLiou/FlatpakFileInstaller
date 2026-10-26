# Spike：用 libflatpak 安裝 `.flatpak` bundle 到 user / system

環境：Ubuntu 24.04、flatpak / libflatpak 1.14.6、PyGObject（`gir1.2-flatpak-1.0`）。

```sh
sudo apt install flatpak gir1.2-flatpak-1.0 python3-gi busybox-static
sh spike/build_test_bundles.sh /tmp/spike-work
export FLATPAK_USER_DIR=/tmp/spike-work/user-inst   # 不污染真正的 ~/.local/share/flatpak
python3 spike/spike_install.py /tmp/spike-work/hello.flatpak user     # 預期失敗：缺 runtime
python3 spike/spike_install.py /tmp/spike-work/platform.flatpak user
python3 spike/spike_install.py /tmp/spike-work/hello.flatpak user     # 成功
sudo python3 spike/spike_install.py /tmp/spike-work/platform.flatpak system
sudo python3 spike/spike_install.py /tmp/spike-work/hello.flatpak system
```

## 結果

| 項目 | 結果 |
|---|---|
| 不安裝就讀 bundle 的中繼資料（`Flatpak.BundleRef`：名稱、分支、大小、所需 runtime、appstream、icon、runtime-repo URL） | ✅ 可行 |
| `Flatpak.Transaction` + `add_install_bundle()` 安裝到 **user** installation | ✅ 可行 |
| 同一個 API 安裝到 **system** installation（以 root 執行） | ✅ 可行 |
| 安裝後 `.desktop` 會匯出到 `exports/share/applications/`，選單看得到 | ✅ 可行 |
| bundle 依賴的 runtime 不存在、又沒有 remote 可抓 | ❌ 會失敗：`requires the runtime ... which was not found` |
| 一般使用者裝到 system（走 polkit / flatpak-system-helper） | ⚠️ 未驗證：容器內沒有 system bus / polkit agent |
| 從 Flathub 自動補 runtime | ⚠️ 未驗證：容器連不到 Flathub |

# Spike 2：從 Flatpak sandbox 裡把 bundle 裝到主機

```sh
sudo apt install flatpak-xdg-utils
dbus-run-session -- sh spike/sandbox_spike.sh /tmp/spike2-work
```

做法與 Flathub 上的 Warehouse 相同：`--talk-name=org.freedesktop.Flatpak`，再用 `flatpak-spawn --host flatpak install ...`。

| 項目 | 結果 |
|---|---|
| 有 `--talk-name=org.freedesktop.Flatpak` 時，從 sandbox 內安裝到主機的 user installation | ✅ 成功 |
| 同上，裝到 system installation | ✅ 成功（以 root 執行） |
| 以 `flatpak override --no-talk-name=...` 拿掉權限 | ❌ 如預期失敗：`--host only works when the Flatpak is allowed to talk to org.freedesktop.Flatpak` |
| bundle 經 document portal 傳入（`/run/user/<uid>/doc/<id>/hello.flatpak`），主機端 flatpak 直接讀得到這個路徑 | ✅ 成功 |
| sandbox 內看不到主機的 `/var/lib/flatpak`（沒給 filesystem 權限） | 確認 |
