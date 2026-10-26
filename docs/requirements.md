# Flatpak App Installer：需求紀錄

> 這份文件記錄已和產品負責人確認的需求，以及尚未決定的事項。技術驗證請見 [`spike/README.md`](../spike/README.md)。

## 產品定位

讓**完全不懂 Linux 的人**，從網路下載 Flatpak 檔案後按兩下就能安裝。安裝視窗參考 macOS `.dmg`：把 app 圖示**拖曳**到「使用者」或「系統」，分別對應 user level 與 system level 安裝。

## 已確認的需求

| # | 項目 | 決定 |
|---|---|---|
| P1 | 支援的檔案 | `.flatpak`（單檔 bundle）與 `.flatpakref`（指向遠端的參照檔）都支援 |
| P2 | 目標使用者 | 完全不懂 Linux 的人，所有文字以白話撰寫，不出現技術術語 |
| P3 | 缺少 runtime | 自動加入 Flathub 並下載，不打擾使用者 |
| P4 | 安裝前警告 | 要顯示 app 要求的權限與來源；文字要親切、白話，不可生硬嚇人 |
| P5 | 已安裝過 | 跳出選單，說明遇到的狀況（例如已裝在另一個位置、版本較舊／較新），讓使用者直接選擇怎麼做 |
| P6 | 第一版範圍 | 只做安裝；解除安裝、升級、管理已安裝的 app 不在範圍內 |
| P7 | 語言 | 先做台灣正體中文（`zh_TW`），但多國語言的架構要先備妥 |
| P8 | 操作方式 | **只能拖曳**（不提供點選按鈕替代） |
| P9 | 無法安裝到系統時 | 程式自行判斷；判斷為裝不了時，直接**隱藏**「系統」這個拖放目標，只留「使用者」 |
| T1 | 發布方式 | 只提供 Flatpak（上架 Flathub）與 AppImage 兩種 |
| T2 | CPU 架構 | x86_64 與 aarch64（ARM64） |
| T3 | 桌面環境 | 專注 KDE Plasma |
| T4 | 基準平台 | Fedora 44（不可變系統，例如 Kinoite）；另需特別注意 SteamOS（KDE、不可變系統） |

## 由需求推導出的技術方向

- **安裝一律透過主機上的 `flatpak` 指令**：Flatpak 版用 `flatpak-spawn --host flatpak ...`（需要 `--talk-name=org.freedesktop.Flatpak`，與 Flathub 上的 Warehouse 相同）；AppImage 版直接執行 `flatpak`。不把 libflatpak 打包進來執行安裝。
- **語言與 UI 框架**：C++；因為專注 KDE，UI 採用 Qt 6 + Kirigami（KDE 官方 UI 框架，KDE Flatpak runtime 內建）。
- **不可變系統**：不能安裝 rpm／修改 `/usr`，所以 app 本身只能是 Flatpak 或 AppImage（已符合）。`/var/lib/flatpak` 與 `~/.local/share/flatpak` 仍可寫入。
- **多國語言**：以 gettext 流程（`po/` 目錄、`.pot` 範本）管理 UI、`.desktop`、AppStream metainfo 的翻譯；原始字串用英文，第一個翻譯是 `zh_TW`。

## 已知風險

1. **Flathub 審核**：`--talk-name=org.freedesktop.Flatpak` 等於可在主機執行任意指令。Warehouse 有前例，但目前 Flathub 的審核政策尚未確認。
2. **SteamOS 的 system 安裝**：Valve 文件說明預設的 `deck` 帳號**沒有密碼**，因此需要管理員驗證的 system level 安裝很可能無法完成。處理方式見下方「判斷能否安裝到系統」。尚未在實機上驗證。
3. **AppImage 不會自動成為 `.flatpak` 的預設開啟程式**：需要 app 在第一次執行時協助註冊，或依賴 Gear Lever 等整合工具。
4. **AppImage 與 FUSE**：舊版 AppImage runtime 需要主機有 libfuse2；應使用新版靜態連結的 type2 runtime。尚未在 Fedora 44 Atomic／SteamOS 上實測。
5. **只能拖曳的無障礙限制**：只用鍵盤或輔助工具的使用者無法完成安裝。這是已確認的產品決定，在此記錄其影響。

## 判斷能否安裝到系統（P9）

不同的安裝動作對應不同的 polkit 權限，flatpak 預設的規則也不同（`/usr/share/polkit-1/actions/org.freedesktop.Flatpak.policy` 與 `rules.d/org.freedesktop.Flatpak.rules`）：

| 情境 | polkit action | 預設行為 |
|---|---|---|
| `.flatpak` bundle | `org.freedesktop.Flatpak.install-bundle` | 一律要管理員密碼 |
| `.flatpakref`，遠端已設定在系統上 | `org.freedesktop.Flatpak.app-install` | 管理員群組（Fedora 為 `wheel`）的使用者免密碼 |
| 需要新增遠端（例如補 Flathub） | `org.freedesktop.Flatpak.configure-remote` | 要管理員密碼 |

判斷流程（在主機上執行；Flatpak 版透過 `flatpak-spawn --host`）：

1. 主機沒有可用的 system installation → 隱藏「系統」。
2. 以 `pkcheck --action-id <上表的 action> --process <主機端 pid>` 查詢：
   - 已授權（免密碼）→ 顯示「系統」
   - 拒絕 → 隱藏「系統」
   - 需要驗證 → 若主機是 SteamOS 則隱藏（預設帳號沒有密碼，無法通過驗證）；其他系統則顯示，安裝時跳出密碼框
3. 主機的發行版從 `/etc/os-release` 判斷；在 Flatpak sandbox 內要讀 `/run/host/os-release`，因為 sandbox 內的 `/etc/os-release` 是 runtime 的。

已知取捨：SteamOS 使用者若自己設定過密碼，「系統」仍會被隱藏，因為程式無法在不取得 root 的情況下得知帳號有沒有密碼。

## 尚未決定

- [x] 程式語言：C++（UI 用 Qt 6 + Kirigami／QML）
- [ ] 授權條款
