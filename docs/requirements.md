# Flatpak File Installer：需求紀錄

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
| P10 | 把檔案拖進視窗 | 從檔案管理員或瀏覽器把 `.flatpak`／`.flatpakref` 拖到視窗裡，就開啟該檔案的安裝畫面 |
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
6. **帶有 `runtime-repo` 的 bundle 離線裝不起來**：實測 flatpak 每次安裝這類 bundle 都會去下載 `.flatpakrepo`，即使 runtime 已經裝好也一樣，沒有網路就會失敗。目前的錯誤訊息會請使用者檢查網路連線。
7. **`--filesystem=xdg-download:ro`**：Flatpak 版多要了「下載」資料夾的唯讀權限，作為拖放時的備援（見 P10）。送審 Flathub 時需要說明理由。
8. **`--share=network`**：Flatpak 版可以連網，用來向 Flathub 查詢 `.flatpakref` 的權限（見 P4），以及下載 `.flatpakref` 的圖示（見「App 的圖示與名稱」）。送審 Flathub 時需要說明理由。向 Flathub API 查詢的部分只用本機的假伺服器測試過，還沒對真正的 flathub.org 驗證。

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

## 已決定的工程事項

- 程式語言：C++20（UI 用 Qt 6 + Kirigami／QML）
- 授權條款：Apache-2.0
- 名稱：Flatpak File Installer（中文：Flatpak 檔案安裝程式）。第一次發布前由 Flatpak App Installer 改名，App ID 與 GitHub repo 名稱一起改，所以不需要 Flathub 的改名遷移。
- App ID：`io.github.yuanliou.FlatpakFileInstaller`
- 最低版本：Qt 6.8、KDE Frameworks 6.9；Flatpak 版使用 KDE runtime 6.10

## 實作進度（v0.1.0 骨架）

| 需求 | 狀態 |
|---|---|
| P1 `.flatpak`／`.flatpakref` | ✅ 解析與安裝 |
| P2 白話文字 | ✅ 所有畫面 |
| P3 缺少 runtime 自動補 | ✅ 見下方「缺少 runtime 時（P3）」；離線無法驗證真正的 Flathub，以本機 repository 代替測試 |
| P4 權限與來源警告 | ✅ 見下方「安裝前的權限與來源（P4）」 |
| P5 已安裝過的選單 | ✅ 見下方「已經裝過時（P5）」 |
| P7 多國語言架構 + `zh_TW` | ✅ |
| P8 只能拖曳 | ✅ |
| P9 判斷並隱藏「系統」 | ✅ 邏輯與單元測試；尚未在 Fedora 44／SteamOS 實機驗證 |
| P10 把檔案拖進視窗 | ✅ 在 X11 上實測；Flatpak sandbox 內的 portal 路徑尚未在實機驗證 |

## 缺少 runtime 時（P3）

開啟 bundle 時，分別檢查「使用者」與「系統」兩個安裝位置：

1. `flatpak info --user|--system <runtime>`：runtime 有沒有裝。裝在系統的 runtime，使用者層級的 App 也能用（已用測試確認）。
2. `flatpak remotes --user|--system --columns=url`：Flathub 有沒有設定。

安裝時，如果 runtime 不在、Flathub 也沒設定，而且 bundle 沒有自帶 `runtime-repo`，就先執行
`flatpak remote-add --user|--system --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo`，
再照常安裝；flatpak 會自動從 Flathub 下載缺少的 runtime（已用本機 repository 代替 Flathub 測試）。
bundle 自帶 `runtime-repo` 時，flatpak 會自己加入那個 repository，不需要我們處理。

需要下載 runtime 時，安裝中的畫面會說明「還需要從網路下載一些額外的元件」。

## 安裝前的權限與來源（P4）

安裝畫面上方有一張卡片，先說 App 從哪裡來，再列出它可以做什麼。

**來源**

| 情況 | 顯示 | 標示 |
|---|---|---|
| `.flatpak` 檔案 | 你下載的檔案：不是從 App 商店來的，沒辦法確認是誰做的，請確定你信任提供這個檔案的網站 | 橘色 |
| `.flatpakref` 指向 Flathub | 來自 Flathub：Linux 上最大的 App 商店 | 藍色 |
| `.flatpakref` 指向其他網站 | 來自 〈網站〉：不是常見的 App 商店，請確定你信任這個網站 | 橘色 |

**權限**

- `.flatpak`：檔案裡的 metadata 就有。
- `.flatpakref`：檔案裡沒有，依序這樣查：
  1. 它的遠端已經設定在使用者或系統上：用 `flatpak remote-info --show-metadata` 向遠端查詢。
  2. 遠端還沒設定（或查詢失敗），而且是 Flathub 的 App：向 Flathub 網站的 API 查詢，`https://flathub.org/api/v2/summary/<app-id>?branch=<branch>`，把回應的 `metadata.permissions` 轉回 Flatpak metadata 的格式再解析。只接受會用到的 key，D-Bus 名稱也先檢查格式，並交給 GKeyFile 處理跳脫字元，所以回應的內容沒辦法藉此多塞權限。最多等 8 秒。
  3. 都查不到：顯示「目前無法得知這個 App 需要哪些權限」。
- 只列出一般人需要知道的權限，用白話描述；GPU、IPC、只讀取其他 App 設定（例如 KDE App 常見的 `xdg-config/kdeglobals:ro`）等不列出。
- 已經涵蓋的就不重複列：可以存取整台電腦，就不再列個人檔案；可以存取所有個人檔案，就不再列個別資料夾。
- 較敏感的權限（執行任何程式、存取所有檔案或個人檔案、所有裝置、密碼、金鑰、修改其他 App 的資料、舊式顯示方式等）排在前面，用橘色的小驚嘆號標示，最後加一句「很多好用的 App 都需要這類權限，只要確定你信任這個 App 就好」。不用紅色，也不擋住安裝。

分類規則在 `src/permissions.cpp`，白話文字在 `src/permissiontext.cpp`。

## App 的圖示與名稱

**`.flatpak`**

1. `flatpak build-bundle` 打包時，如果 App 裡有 AppStream 資料（`files/share/app-info/`），會把 64px（或 128px）的圖示和 AppStream XML 一起放進 bundle 的 metadata，直接用這些就好。
2. 很多 bundle 沒有這些資料（例如 App 沒有附 AppStream metainfo）。這時改從 bundle 的內容裡，讀出 App 給選單用的檔案：
   - 名稱與說明：`export/share/applications/<app-id>.desktop` 的 `Name`、`Comment`（含翻譯）。
   - 圖示：`export/share/icons/hicolor/` 底下，先找 `scalable` 的 SVG（畫成 256px），沒有的話用最大的 PNG。`.desktop` 的 `Icon` 以 App ID 開頭時，就用它的名稱。

   bundle 是從零開始的 OSTree static delta：所有檔案分成幾個 part（預設每個約 32 MB，大多用 xz 壓縮），各自附一串寫出檔案的操作。程式從 commit 的根目錄一路找到需要的檔案，只解壓縮有用到的 part。150 MB、內容無法壓縮的測試 bundle 約要 0.2 秒。程式碼在 `src/bundlecontents.cpp`。

   讀出來的檔案不驗證 checksum，因為只拿來顯示，這些資訊 bundle 本來就可以自己寫在 metadata 裡；真正安裝時 flatpak 會驗證所有內容。解析器用隨機破壞的 bundle 在 AddressSanitizer 下測試過。

**`.flatpakref`**

檔案裡的 `Icon=` 是圖示的網址（Flathub 的 `.flatpakref` 都有）。開啟檔案後在背景下載，下載好之前先顯示通用圖示。只接受 HTTPS，而且主機要和 `Url=` 的 repository 相同，避免開啟檔案時連到其他網站；檔案最大 4 MB，最多等 8 秒。

## 已經裝過時（P5）

開啟檔案時，先用 `flatpak list --user|--system --columns=application,branch,arch,version,active` 找有沒有同一個 App（ID、分支、架構都相同）。先找使用者層級，因為 `flatpak run` 也優先用它。找到的話，不顯示拖曳安裝畫面，改顯示「「X」已經在這台電腦上了」，說明目前裝在哪裡、版本差在哪裡，再讓使用者選。

**怎麼判斷版本**

- `.flatpak`：bundle 裡 commit 的 SHA-256 就是 OSTree 的 commit ID，和 `flatpak list` 的 `active` 欄位比對（前 12 碼）。相同就是同一個版本；不同再比較 AppStream 的版本號（數字照數值比，`1.10` 比 `1.9` 新）。版本號相同但 commit 不同（重新建置）也當作同一個版本；任一邊沒有版本號就是「不同的版本」。
- `.flatpakref`：遠端已設定時，用 `flatpak remote-info --show-commit` 取得遠端最新的 commit 來比對；沒辦法比對時就是「無法得知」。

**選項**（第一個是主要按鈕）

| 情況 | 說明 | 選項 |
|---|---|---|
| 同一個版本 | 你已經有同一個版本（1.2.3）了 | 開啟、重新安裝、關閉 |
| 檔案比較新 | 這個檔案是比較新的版本（2.0），你現在用的是 1.2.3 | 更新到這個版本、開啟目前的版本、關閉 |
| 檔案比較舊 | 你已經有比較新的版本（2.0），這個檔案是比較舊的版本（1.2.3） | 開啟目前的版本、換成這個舊版本、關閉 |
| 比不出新舊 | 這個檔案和你現在用的是不同的版本 | 開啟目前的版本、換成這個版本、關閉 |
| `.flatpakref`，有新版本 | 有新版本可以更新 | 更新、開啟、關閉 |
| `.flatpakref`，已是最新 | 你已經有最新的版本了 | 開啟、關閉 |
| `.flatpakref`，無法比對 | 可以檢查看看有沒有新版本 | 開啟、檢查更新、關閉 |

- 提供變更選項時會說明「App 裡的設定和資料都會保留」；會換成檔案裡的版本時，也會顯示 P4 的來源與權限卡片。
- `.flatpak` 用 `flatpak install --reinstall` 替換；`.flatpakref` 不支援重新安裝，只提供 `flatpak update`。
- 裝給所有人用、但這台電腦不允許變更系統時（例如 SteamOS），隱藏變更選項，改說明「只有管理這台電腦的人可以變更它」。系統層級的更新（`app-update`）依 flatpak 預設規則不需要密碼，所以 `.flatpakref` 的更新在 SteamOS 上也能用；替換 bundle 需要 `install-bundle`，要密碼。

## 把檔案拖進視窗（P10）

- 視窗背後有一個 C++ 的 `FileDropArea`。拖進來的東西如果有 `.flatpak`／`.flatpakref` 檔名才接受，並顯示「放開就可以安裝這個 App」；其他檔案不接受。
- 放開時用 KCoreAddons 的 `KUrlMimeData::urlsFromMimeData()` 解讀，在 sandbox 裡會透過 FileTransfer portal 換成 App 讀得到的路徑（Dolphin 等 KDE 程式支援）。
- 瀏覽器的下載清單通常只給一般路徑，所以 Flatpak 版另外要了「下載」資料夾的唯讀權限。
- 安裝或檢查中不接受拖放；其他時候拖進新檔案會換掉目前的檔案。
