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
| P11 | 設定頁 | 視窗右上角有齒輪按鈕，打開設定頁，內容是「語言」與「關於」（版本、授權、專案網站）；KDE 的 Global Menu 也有「檔案」「設定」「說明」。檢查或安裝中（`Installer.busy`）不能進入設定頁。關於頁保留 Kirigami Addons 內建的「使用函式庫」區塊：P2 的白話要求適用於主要使用流程，關於頁是使用者主動進入的內頁，可以完整顯示技術資訊 |
| P12 | 語言清單 | 「跟隨系統」（預設）、English、繁體中文；清單來自實際有翻譯的語言，新增 `po/<lang>.po` 後不用改程式；語言名稱用該語言自己的寫法 |
| P13 | 切換語言 | 下次啟動才生效，不做即時切換。選擇後立刻儲存，並用「目標語言」顯示提示（「重新啟動」「稍後再重啟」）；選擇後實際語言和目前相同就不提示 |
| P14 | 重新啟動 | 按「重新啟動」後，若有開著的檔案（Ready、AlreadyInstalled、Invalid、Failed）會自動開回同一個檔案；Installed 或 Empty 回到首頁 |
| P15 | 跟隨 KDE 外觀 | 在 KDE 桌面上，App 跟隨使用者的顏色（kdeglobals 配色）、應用程式風格與圖示主題；Flatpak 與 AppImage 都要。使用者選的風格 App 裡沒有時（Oxygen、第三方風格）退回 Breeze，顏色與圖示仍跟隨。Plasma 風格不適用，視窗裝飾由 KWin 負責。新的外觀在下次開啟時反映。非 KDE 桌面（GNOME 等）維持 portal 的 platform theme，不強套 KDE 外觀 |
| T1 | 發布方式 | 只提供 Flatpak（上架 Flathub）與 AppImage 兩種 |
| T2 | CPU 架構 | x86_64 與 aarch64（ARM64） |
| T3 | 桌面環境 | 專注 KDE Plasma |
| T4 | 基準平台 | Fedora 44（不可變系統，例如 Kinoite）；另需特別注意 SteamOS（KDE、不可變系統） |
| T5 | 設定頁的 UI 元件 | 使用 Kirigami Addons（FormCard、AboutPage），外觀接近 KDE 系統設定 |

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
9. **Global Menu 的 `com.canonical.AppMenu.Registrar` 權限來自 KDE runtime**：Flatpak 版的選單要送到 KDE Plasma 的 Global Menu，需要對這個 D-Bus 服務有 talk 權限。manifest 不需要自己加：`org.kde.Platform//6.10` 的 metadata 已含 `[Session Bus Policy] com.canonical.AppMenu.Registrar=talk`（還有 `org.kde.kconfig.notify`、`org.kde.KGlobalSettings`），已實測：拿掉 manifest 裡的 `--talk-name` 重新建置，產出的 App metadata 仍含這一項，在 App 的 sandbox 內（`flatpak run --command=sh <App>`）`NameHasOwner` 為 true，呼叫也到得了（用假的 registrar）。我先前以為需要手動加，是因為用 `flatpak run --command=sh org.kde.Platform//6.10` 直接執行 runtime 來測，那樣不會套用 runtime 的這段 metadata，測試方法有誤。所以送審 Flathub 不必為它辯護，但使用者看到的權限清單仍會有它們。
10. **語言覆寫需要主機上有可用的 locale**：KI18n 用 gettext，gettext 在 locale 是 `C`（含 `C.UTF-8`）或主機沒有產生該 locale 時，會忽略 `LANGUAGE`。程式在 locale 不可用時，會改用 `<語言>.UTF-8`，再不行用 `en_US.UTF-8`（見「設定與語言」）；如果主機兩者都沒有（AppImage 在極精簡的系統上），選了非英文的語言就不會生效。Flatpak 的 runtime 內建這兩個 locale。
11. **Kirigami、Kirigami Addons 本身的翻譯**：「關於」頁的固定文字（「授權」「首頁」等）來自 Kirigami Addons 自己的翻譯。Flatpak 版由 runtime 的 `org.kde.Platform.Locale` 提供，而 Flatpak 只會替系統設定的語言安裝它；所以在英文系統上選了繁體中文，關於頁的這些文字可能還是英文。AppImage 版由 `build-appimage.sh` 從同版本的 Kirigami Addons 原始碼編譯出翻譯放進去（Ubuntu 的套件沒有附翻譯）。Kirigami 本身的 Qt 翻譯（`libkirigami6_qt.qm`）沒有打包，AppImage 裡 Kirigami 自己的少數文字會跟隨主機。
12. **Kirigami Addons 的 AboutPage 會在 console 印出 QML 的 `TypeError`**（`Cannot read property 'width' of null`）：在 Ubuntu 26.04 的 Kirigami Addons 1.11 上，頁面建立時還沒加入視窗，授權對話框的 `parent` 暫時是 null；之後會重新計算，不影響使用。Flatpak 版使用 1.13.1，尚未驗證是否仍會出現。
13. **主機沒有圖示主題時，Kirigami 與 Kirigami Addons 的圖示會缺**：返回按鈕會變成文字（例如「N…k」），關於頁的「首頁」「回報問題」沒有圖示。在乾淨的 Ubuntu 容器（沒有 breeze）裡看到；裝上 breeze 圖示後返回按鈕正常。KDE Plasma 主機與 Flatpak 的 KDE runtime 都有 breeze，所以只影響沒有圖示主題的環境（例如極精簡的 AppImage 執行環境）。自己的圖示（齒輪等）是內嵌的，不受影響。
14. **每次開啟設定頁或關於頁，Kirigami 會印出 `PageRow.qml:1000 StackView has detected conflicting anchors`**：改成固定的 `globalToolBar.style`（不用依頁面深度切換）後仍會出現，所以不是我們的綁定造成，是 Kirigami 本身的訊息，不影響功能。

15. **AppImage 的「選擇檔案」依賴桌面的 portal**：AppImage 內打包了 Qt 的 `xdgdesktopportal` platform theme（`libqxdgdesktopportal.so`），檔案選擇走 `org.freedesktop.portal.FileChooser`：Plasma 上是 KDE 的原生視窗，GNOME 上是 GNOME 的。主機沒有 portal 時，Qt 會退回 QtQuick.Dialogs 的 QML 備用視窗（可以用，但外觀不是原生的）。已用假的 portal 實測呼叫與篩選條件；AppImage 已在 KDE Plasma 實機確認，Flatpak 版也已實機確認（含 document portal 回傳的路徑）；GNOME 尚未實機驗證。使用者自己設定的 `QT_QPA_PLATFORMTHEME`（`gtk2` 以外）會被尊重，這時由使用者的設定決定，而 AppImage 內只有 `xdgdesktopportal` 這個外掛。

16. **`xdg-config/kdeglobals:ro` 權限也來自 KDE runtime**：App 讀使用者的顏色、風格與圖示主題設定要讀 `~/.config/kdeglobals`；`org.kde.Platform//6.10` 的 metadata 已含 `[Context] filesystems=xdg-config/kdeglobals:ro`。已實測：manifest 不加這項，建置出的 App metadata 仍含它，在 App 的 sandbox 內看得到該檔案（同上，先前用直接執行 runtime 測「預設看不到」的方法有誤）。不需要在 manifest 另外宣告，Flathub 送審不必為它辯護，使用者看到的權限清單仍會有。圖示主題不需要額外權限：sandbox 的 `XDG_DATA_DIRS` 已包含 `/run/host/share`，主機的 `/usr/share/icons`（例如 Oxygen）在那裡。
17. **AppImage 變大**：為了跟隨 KDE 外觀，AppImage 多打包 plasma-integration 的 platform theme（`KDEPlasmaPlatformTheme6.so`）與 Breeze 風格（`breeze6.so`），以及它們需要的 KDE Frameworks 函式庫（KIO 等）：從約 64.9 MB（61.9 MiB）增加到約 69.5 MB（66.2 MiB），多約 4.6 MB。linuxdeploy 不打包 `libharfbuzz`，要靠主機提供（原本就如此）。
18. **AppImage 的 KDE 外觀只在有 KDE portal 時啟用**：plasma-integration 自己的檔案視窗用 KIO，而 KIO 的 file worker 沒有（也不適合）打包，所以要它改走 portal（`PLASMA_INTEGRATION_USE_PORTAL=1`）；但沒有 portal 時這個視窗完全不會出現（實測）。因此只有在偵測到 `xdg-desktop-portal/portals/kde.portal` 與 portal 的 D-Bus 服務檔時才用 Plasma 的 theme，否則退回 xdgdesktopportal theme 與 Qt 的備用視窗。portal 檔案存在但服務壞掉時，檔案視窗仍可能開不起來，這種情況沒有處理。
19. **Kirigami 自己會讀 kdeglobals**：只要 App 讀得到 `kdeglobals`（Flatpak 版由 runtime 的權限提供，見 16），即使 platform theme 是 portal 的（例如 GNOME），Kirigami 的顏色也會跟著 kdeglobals 走（實測）；風格與圖示不會。GNOME 主機上通常沒有這個檔案，所以實際上看不出來。

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
- 新增依賴：Kirigami Addons（`KF6KirigamiAddons`；執行時用到 `org.kde.kirigamiaddons.formcard` 與 `org.kde.coreaddons`）、Qt 的 `Qt.labs.platform`（Global Menu）。KDE runtime 6.10 已內建 Kirigami Addons 1.13.1 與 `Qt.labs.platform`，所以 Flatpak manifest 不需要另外的 module

## 實作進度（v1.0.0）

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
| P10 把檔案拖進視窗 | ✅ 在 X11 上實測；Flatpak 版已在 KDE Plasma 實機確認（從 Dolphin 拖入「下載」以外的檔案） |
| P11 設定頁與 Global Menu | ✅ 在 Ubuntu 26.04 容器（Xvfb）實測，包含用假的 `com.canonical.AppMenu.Registrar` 檢查選單內容；AppImage 版已在 KDE Plasma 實機確認 Global Menu 與設定頁，Flatpak 版也已實機確認 Global Menu |
| P12 語言清單 | ✅ 單元測試；AppImage 內也實測偵測得到翻譯 |
| P13 切換語言 | ✅ 單元測試與實測（`LANG=C`、`LANG=zh_TW.UTF-8`、`LANG=en_US.UTF-8` 各搭配 English、繁體中文、跟隨系統） |
| P14 重新啟動 | ✅ 在 Xvfb 實測（一般安裝與 AppImage）；AppImage 版已在 KDE Plasma 實機確認；Flatpak 版也已實機確認（語言改變，檔案開回） |
| P15 跟隨 KDE 外觀 | ✅ 在容器內用自訂配色（橘色、Icons/Theme=oxygen、widgetStyle=Oxygen）、BreezeDark、BreezeLight 實測：Flatpak 與 AppImage 的顏色、圖示（Oxygen）跟隨，風格退回 Breeze；GNOME 維持 portal theme。AppImage 版已在 KDE Plasma 實機確認（Oxygen 圖示、顏色、元件風格、原生檔案視窗、Global Menu）；Flatpak 版也已實機確認 |

## 設定與語言（P11–P14）

### 入口

- 視窗右上角的齒輪按鈕（`src/icons/settings.svg`）。它放在主畫面上方一條獨立的空白區域，不會蓋住內容；在最小視窗（30×28 gridUnit）的「準備安裝」畫面也確認過。檢查或安裝中隱藏。
- 設定頁與「關於」頁是疊在主畫面上的頁面（Kirigami 的 layers），上方有返回按鈕。開著設定頁時如果開始檢查或安裝，會自動回到主畫面。選單的「設定…」在關於頁開著時會回到設定頁，不會重複堆疊。
- **拖放（P10）在設定頁、關於頁開著時照常運作**：接收拖放的 `FileDropArea` 與「放開就可以安裝」的提示放在視窗最上層的 overlay；放開後一律先回到主畫面再開啟檔案（無效的檔案不會進入忙碌狀態，所以明確清除疊著的頁面）。
- 齒輪圖示是黑色的單色 SVG，用 `Kirigami.Icon` 的 `isMask` 以文字顏色著色，深色主題下也看得到（用 BreezeDark 與 BreezeLight 實測）。
- **Global Menu**：用 Qt 的 `Qt.labs.platform.MenuBar`。有 Global Menu 的環境（KDE Plasma）才會出現選單，沒有的環境視窗內不會多出選單列，只靠齒輪。選單內容：檔案（選擇檔案…、結束）、設定（設定…）、說明（關於）。`Installer.busy` 時停用「選擇檔案」「設定」「關於」；「結束」走原本的 `onClosing`，安裝中仍不能關閉。從選單選檔案時，如果設定頁開著，會先回到主畫面。

### 語言清單與儲存

- 有翻譯的語言：到 XDG 資料目錄的 `locale/<語言>/LC_MESSAGES/flatpak-file-installer.mo` 找，一律加上 `en`。這和 `KLocalizedString::availableDomainTranslations()` 找的地方相同，但不直接呼叫它：它會讓 Qt 與 KI18n 在那一刻就讀走環境裡的語言，之後再設定 `LANGUAGE` 就來不及了（實測過：選 English 時介面仍是繁體中文）。AppImage 的 `XDG_DATA_DIRS` 在 `main()` 開頭就加上 `$APPDIR/usr/share`，Flatpak 則是 `/app/share`，所以兩者都找得到。
- 語言名稱用該語言自己的寫法（`QLocale::nativeLanguageName()`）；`en` 特別處理，因為 `QLocale("en")` 會回傳 American English。
- 設定用 `QSettings`（組織名稱 `yuanliou`、應用程式名稱 `flatpak-file-installer`，明確指定，因為 `QApplication` 建構前就要讀），存在 `~/.config/yuanliou/flatpak-file-installer.conf`，Flatpak 下是 `~/.var/app/<id>/config/yuanliou/…`。鍵是 `language`，沒有值代表跟隨系統。存的語言已經沒有翻譯時，當作跟隨系統。
- 程式碼：`src/languagepreference.*`（不依賴 GUI，在 `installer_core`，有單元測試 `tests/languagepreferencetest.cpp`）與 `src/preferences.*`（給 QML 用的單例）。

### 怎麼讓整個 app 換語言

在 `QApplication` 建構前設定環境變數，KI18n、Kirigami、Kirigami Addons 與 Qt 都是在啟動時從環境讀語言：

- `LANGUAGE=<語言>:en_US`（選 English 時是 `en_US`）。要寫 `en_US` 而不是 `en`：KI18n 看到 `en_US` 才會停止往下找（`en` 沒有翻譯檔，會繼續用系統的下一個語言）。
- **`LANG=C` 的情況**：gettext 在 locale 是 `C`（含 `C.UTF-8`）或主機沒有產生該 locale 時，會忽略 `LANGUAGE`（用 `LANG=C LANGUAGE=zh_TW` 實測，連 `toString({"zh_TW"})` 都是英文）。所以這時另外把 `LC_MESSAGES`（若 `LC_ALL` 有設定則是 `LC_ALL`）設成主機上有的 `<語言>.UTF-8`，再不行用 `en_US.UTF-8`。這會被子行程繼承，但 `flatpak` 的輸出有語言影響的部分只有給人看的錯誤訊息（見下）。
- 「跟隨系統」時，系統語言依 gettext 的規則從環境判斷（`LANGUAGE`，再 `LC_ALL`／`LC_MESSAGES`／`LANG`）；如果 locale 不可用，gettext 會忽略這些，所以視為英文。

**子行程**：解析 `flatpak` 輸出的程式（`--installations`、`list`／`remotes` 的 `--columns`、`remote-info --show-commit`／`--show-metadata`）都不受語言影響，`pkcheck` 只看結束代碼。已在主機上以 `en_US` 和 `zh_TW` 比較過這些指令的輸出，內容相同；只有 `flatpak info` 與錯誤訊息的文字會翻譯，程式只看結束代碼或把它顯示給使用者。所以不需要在啟動子行程時處理。

**啟動使用者的 App 例外**：安裝完成或已安裝時按「開啟」（`Installer::launch`、`Installer::openExisting`）會把 `LANGUAGE`、`LC_ALL`、`LC_MESSAGES`、`QT_QPA_PLATFORMTHEME`（與 `XDG_DATA_DIRS`）還原成啟動時的值（原本沒設定的就移除），否則被開啟的 App 會繼承我們為自己設定的語言與 platform theme（實測 `flatpak run` 會把這些變數原樣帶進 sandbox）。做法是 `HostCommand::create` 多一個可選的 `SavedEnvironment`（`SavedEnvironment::applyTo`），資料與重新啟動用的是同一份，沒有另外維護清單。`flatpak install`／`update` 等指令不還原，所以錯誤訊息仍跟著使用者選的語言。AppImage 的 linuxdeploy hook 在 GNOME／XFCE 設的 `gtk2` 算原始值，還原它就是 AppImage 本來的行為。Flatpak 版不受影響：`flatpak-spawn --host` 不會把 sandbox 內的環境變數轉送到主機（程式沒有傳 `--env`），開啟 App 的指令本來就在主機的環境下執行。

### 切換與重新啟動

- 選擇語言後立刻儲存。算出「目標語言」（選了某個語言就是它；跟隨系統就是系統語言解析後的結果，沒有翻譯就是 English），和這次執行的語言相同就不提示。
- 提示的文字與按鈕用目標語言顯示（`ki18n(...).toString(QStringList{語言})`）。「稍後再重啟」只關掉提示。
- 「重新啟動」用 `execv` 取代目前的行程，不用 `QProcess::startDetached`：Flatpak 的 sandbox 在第一個行程結束時就會消失，document portal 的路徑也只在裡面有效。執行檔用 `/proc/self/exe` 指向的實際檔案（直接執行 `/proc/self/exe` 的話，行程名稱會變成 `exe`）。只有 `Installer` 不在忙碌時才會執行，C++ 這邊也有擋。
- `exec` 前把 `LANGUAGE`、`LC_ALL`、`LC_MESSAGES` 與 `XDG_DATA_DIRS` 還原成啟動時的值（原本沒設定的就移除）：否則從覆寫改回「跟隨系統」時會繼承舊的語言，AppImage 的 `XDG_DATA_DIRS` 也會一直累積 `$APPDIR/usr/share`。
- 參數：原本的命令列，拿掉原本的檔案，有開著的檔案就加上 `-- <檔案>`。

### 跟隨 KDE 外觀（P15）

- 用 plasma-integration 的 platform theme（`kde`）：它讀 `kdeglobals`，提供配色、圖示主題與風格名稱的清單（使用者的風格、`breeze`、`oxygen`、`fusion`、`windows`，依序找第一個存在的）。App 沒有使用者的風格時（例如 Oxygen）自然退回 Breeze；不另外打包 Oxygen 風格。程式沒有自己處理風格。
- 選擇邏輯在 `src/platformtheme.*`（可測試，`tests/platformthemetest.cpp`），`main()` 在 `QApplication` 之前呼叫：
  - `QT_QPA_PLATFORMTHEME` 使用者設定了（`gtk2` 在 AppImage 內除外，見上）就不動。
  - KDE（`XDG_CURRENT_DESKTOP` 含 `KDE`）：Flatpak 與 AppImage 設 `kde`，並設 `PLASMA_INTEGRATION_USE_PORTAL=1`（讓檔案選擇仍走 portal，原因與條件見已知風險 18；原始碼 `kdeplatformtheme.cpp` 查證過：預設用 KIO 的 `KDEPlatformFileDialogHelper`，這個環境變數為 1 時改用 `QXdgDesktopPortalFileDialog`）。
  - 其他桌面：AppImage 用 `xdgdesktopportal`；Flatpak 不動（Qt 在 sandbox 內自己選 `xdgdesktopportal`）。
  - Flatpak 必須主動要求 `kde`：實測 KDE 桌面上 Qt 在 sandbox 內仍先選 `xdgdesktopportal`，它只知道淺色／深色，不會跟隨配色、風格、圖示。
- 新增的環境變數（`PLASMA_INTEGRATION_USE_PORTAL`，與 `QT_QPA_PLATFORMTHEME`）都記在 `SavedEnvironment`：重新啟動時還原，啟動使用者的 App 時也還原（用假的 `flatpak` 實測：`run` 收到的兩者都是未設定，`install` 仍帶著覆寫值）。
- 打包：`build-appimage.sh` 已用 `DEPLOY_PLATFORM_THEMES=1`，會把 CI 裝的 `plasma-integration` 的 theme 外掛一起打包；`kde-style-breeze` 的 `breeze6.so` 由 linuxdeploy-plugin-qt 打包（`styles/`）。CI 的 appimage job 新增 `plasma-integration kde-style-breeze`。
- 反映時機：下次開啟。沒有做即時更新，也沒有為此增加權限。

### 選擇檔案（原生視窗）

- 「選擇檔案…」（按鈕與 Global Menu）用 QtQuick.Dialogs 的 `FileDialog`，它會透過 Qt 的 platform theme 使用桌面原生的視窗；篩選條件是 `*.flatpak`、`*.flatpakref`，選到檔案後進入安裝畫面。設定頁或關於頁開著時從選單選檔案，會先回到主畫面。
- **AppImage**：（KDE 上改用 Plasma 的 theme，見上一節；以下是其他桌面與沒有 KDE portal 時的做法。）`build-appimage.sh` 設定 `DEPLOY_PLATFORM_THEMES=1`，讓 linuxdeploy-plugin-qt 把 `xdgdesktopportal` platform theme 打包進去（它只依賴已打包的 Qt6 Gui／DBus；CI 的 appimage job 要安裝 `qt6-xdgdesktopportal-platformtheme`）。`main()` 在 `APPDIR` 區塊、`QApplication` 之前，如果 `QT_QPA_PLATFORMTHEME` 沒設定，或是 linuxdeploy 的 hook 在 GNOME／XFCE 設的 `gtk2`（沒有打包，所以沒有作用），就改成 `xdgdesktopportal`；而且只在 AppDir 裡真的有這個外掛時才改。取捨：無法區分 hook 設的 `gtk2` 與使用者自己設的 `gtk2`，但兩者在 AppImage 內都不會有作用，所以一律取代；其他值一律尊重。一般安裝與 Flatpak 不經過這段程式，行為不變。
- **Flatpak**：不需要改。Qt 在 sandbox 內會自動把 `xdgdesktopportal` 加進 platform theme 清單（實測 `qt.qpa.theme: Adding xdgdesktopportal to list of theme names`，並成功建立），KDE runtime 也有 `libqxdgdesktopportal.so`；實測檔案選擇會呼叫 portal。
- 沒有 portal 時退回備用視窗（實測）。
- AppImage 已在 KDE Plasma 實機確認：跳出 KDE 原生視窗、只列出 `.flatpak` 與 `.flatpakref`，選檔後進入安裝畫面，外觀不受影響。安裝完成後按「開啟」，被啟動的 App 用的是系統語言，不受這裡選的語言影響。

## AppImage 裡 Kirigami Addons 的翻譯

Ubuntu 的 Kirigami Addons 套件不含翻譯（`kirigami-addons6.mo`），所以 AppImage 的「關於」頁固定文字會是英文。`packaging/appimage/build-appimage.sh` 會下載版本相同的 Kirigami Addons 原始碼（釘住版本與 SHA-256），把 `po/LINGUAS` 列出的語言編譯成 `kirigami-addons6.mo` 放進 AppDir；上游沒有該語言時印出警告。升級 Ubuntu 的 Kirigami Addons 版本時，要同步更新腳本裡的版本與雜湊。

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
