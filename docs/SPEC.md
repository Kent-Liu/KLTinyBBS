# KLTinyBBSModule 功能規格清單

## 1. 模組概述

- **名稱**：KLTinyBBS（kltinybbs）
- **Port**：`meshtastic_PortNum_TEXT_MESSAGE_APP`（文字訊息）
- **行為**：僅處理「發給本節點」的文字封包（`p->to == nodeDB->getNodeNum()`，port 為 `TEXT_MESSAGE_APP`），實作精簡 BBS：用戶登入、私人郵件、新聞公告、用戶列表；回覆單則最長約 200 bytes，列表每行約 35 bytes，並做 UTF-8 安全截斷。指令 payload 前後空白與結尾 CR/LF 會先被去除再解析。
- **儲存**：UserStore（用戶帳號）、PmStore（郵件/新聞）、PrefStore（偏好與進階設定）。路徑與常數可由 `config.h` 及編譯參數調整。
- **版本**：模組版本字串見 `KLTinyBBSModule::MODULE_VERSION`（例：v0.8.0），登入歡迎詞中會顯示。

---

## 2. 一般功能（對外說明）

以下指令會出現在 `/h`、`/help`、`/?` 的說明中。**時間**顯示皆為節點**本地時間**（依 RTC 時區在模組內換算）。郵件／新聞／用戶列表的**最後一行**不帶多餘換行。單則訊息最長為 PmStore 的 `maxTextLen()`（預設 104 bytes）；超過時不寫入並回覆約超出百分比與上限。

### 2.1 `/hi <user> <pass>`

- **比對**：指令為 `hi`，後接兩個 token（用戶名、密碼）。
- **行為**：登入或註冊。帳號與密碼皆為 **4～8 個可列印 ASCII**（不可 Unicode）。
- **回覆**：
  - 登入成功：`KLTinyBBS vX.X.X, Welcome, <user>.`；若有新信則加 ✉️；結尾可接自訂問候語（PrefStore）。
  - 註冊成功：同上。密碼錯誤、名稱已存在、BBS 滿等時回覆對應錯誤字串。

### 2.2 `/bye`

- **比對**：指令為 `bye`。
- **行為**：登出目前節點綁定的帳號。
- **回覆**：`Bye.`

### 2.3 `/pass <newpass>`

- **比對**：指令為 `pass`，後接一個 token（新密碼）。
- **行為**：變更目前登入帳號的密碼；**需先登入**。新密碼 4～8 字元可列印 ASCII。
- **回覆**：成功 `Password updated.`；未登入則 `Login first: /hi user pass`；格式不符則密碼規則提示。

### 2.4 `/m`、`/mail`、`/m <N>`、`/m <N>-`

- **比對**：指令為 `m` 或 `mail`；可選第二個 token 為數字或 `N-`。
- **行為**：列出或讀取私人郵件；**需登入**。
- **回覆**：
  - 無參數：第一行 `Mail(N)`，接著最多 5 筆摘要（索引、寄件者、本地時間、摘要），UTF-8 安全截斷。
  - `<N>`：第 N 則全文（含標題行）。
  - `<N>-`：從第 N 則開始列出摘要。無則回覆 `No such mail.`

### 2.5 `@<username> <訊息>`

- **比對**：payload 以 `@` 開頭，後接用戶名與訊息內容。
- **行為**：發送私人訊息給該用戶；**需登入**。
- **回覆**：成功 `OK`；用戶不存在、空訊息、長度超過上限（回覆約超出百分比與上限，不發送）等則對應錯誤字串。

### 2.6 `/n`、`/news`、`/n <N>`、`/n <N>-`

- **比對**：指令為 `n` 或 `news`；可選第二個 token 為數字或 `N-`。
- **行為**：列出或讀取新聞（公告）；**需登入**。
- **回覆**：格式同郵件；第一行 `News(N)`，摘要與讀取規則同上。無則回覆 `No such news.`

### 2.7 `/p <內文>`、`/post <內文>`

- **比對**：指令為 `p` 或 `post`，其餘為內文。
- **行為**：張貼一則新聞；**需登入**。內文超過單則上限時不張貼。
- **回覆**：成功 `OK`；未登入則登入提示；長度超過則回覆約超出百分比與上限。

### 2.8 `/delete post <N>`

- **比對**：指令為 `delete`，次 token 必須為 `post`，第三個 token 為正整數 `N`（新聞列表索引，1-based，1 代表最新一則）。
- **行為**：刪除一則新聞（公告），使用與 `/n`、`/news` 相同的可見列表索引規則。僅處理 `post` 類型，預留未來擴充 `/delete mail <N>`。
- **權限**：
  - **一般登入用戶**：可刪除自己發佈的公告；若目標公告作者不是自己則拒絕。
  - **admin 模式**：可刪除任意公告（不受作者限制）；即使未登入一般帳號也可執行。
- **回覆**：
  - 成功：`OK`。
  - 語法錯誤或參數缺漏：回覆用法提示（例如繁中 `用法:/delete post 編號`、英文 `Usage: /delete post N`）。
  - 非 admin 且未登入：回覆登入提示（`Login first: /hi user pass`）。
  - 索引不存在：`No such news.`／`無此則`。
  - 非 admin 嘗試刪除他人公告：回覆拒絕字串（例如繁中 `不可刪他人公告`、英文 `Cannot delete others' posts.`）。
  - 寫入失敗（標記刪除失敗）：回覆刪除失敗字串（例如 `Delete failed.`／`刪除失敗`）。

### 2.9 `/users`、`/users <關鍵字>`

- **比對**：指令為 `users`；可選第二個 token 為關鍵字。
- **行為**：列出用戶；**需登入**。
- **回覆**：第一行 `User(N)`，接著每行「用戶名 最後登入時間」（本地時間）。有關鍵字時僅顯示用戶名包含該關鍵字（不區分大小寫）的用戶。

### 2.10 `/h`、`/help`、`/?`

- **比對**：payload 為 `"/h"`、`"/help"` 或 `"/?"`（指令 token 為 `h`、`help`、`?`）。
- **行為**：依目前情境回傳對應說明，每段總長 < 200 bytes：
  - **未登入**：僅列出登入相關指令（如 `/hi 帳 密`）與 `/h` 說明。
  - **已登入**：僅列出登入後可用的指令（登出、改密、郵件、新聞、貼文、`/delete post N`、私訊、用戶列表等）與 `/h` 說明。
  - **已進入 admin 模式**：僅列出 admin 相關指令（例如 `/admin bye`、`/admin pass`、`/sync`、`/reset`、`/welcome`、`/private`）與 `/h` 說明。

### 2.11 `/sysinfo`

- **比對**：payload 為字串 `"/sysinfo"`（8 字元）。
- **行為**：回傳本機記憶體狀態；**ESP32** 時另顯示檔案系統（與 KLARBot 類似）。**不需登入**。

---

## 3. Admin 功能（可整段刪除後再分享）

以下為**進階管理**，與一般用戶登入無關；**未進入 admin 時**使用這些指令會得到「未知指令」回覆。**已進入 admin 時**，`/h` 會顯示這些 admin 指令的說明。成功／失敗皆會回覆（如 OK 或錯誤說明）。

### 3.1 `/admin <密碼>`

- **比對**：指令為 `admin`，後接一個 token（密碼）。
- **行為**：以 admin 密碼進入 admin 模式。
- **回覆**：正確則 `OK`；錯誤則 `Unknown cmd. /h for help.`（不揭露 admin 存在）。

### 3.2 `/admin bye`

- **比對**：指令為 `admin`，次 token 為 `bye`。
- **行為**：結束 admin 模式；若 PrefStore 有未寫入變更則寫入 flash。
- **回覆**：若目前為 admin 則 `OK`；否則 `Unknown cmd.`

### 3.3 `/admin pass <新密碼>`

- **比對**：指令為 `admin`，次 token 為 `pass`，再一個 token 為新密碼。
- **行為**：變更 admin 密碼（4～8 字元可列印 ASCII）。**僅在 admin 模式下有效**。
- **回覆**：成功 `OK`；格式不符則密碼規則提示；未在 admin 則未知指令。

### 3.4 `/welcome <文字>`

- **比對**：指令為 `welcome`，其餘為問候語文字。
- **行為**：設定登入成功後顯示的自訂問候語（C string，最長 103 字元 + null）。**僅在 admin 模式下有效**。
- **回覆**：成功 `OK`；過長則 `Welcome too long. Max N bytes.`；未在 admin 則未知指令。

### 3.5 `/private pass <新密碼>`

- **比對**：指令為 `private`，次 token 為 `pass`，再一個 token 為新密碼。
- **行為**：變更 private 解鎖用密碼（4～8 字元）。**僅在 admin 模式下有效**。
- **回覆**：成功 `OK`；格式不符則密碼規則提示；未在 admin 則未知指令。

### 3.6 `/private lock`

- **比對**：指令為 `private`，次 token 為 `lock`。
- **行為**：啟用 private 模式（之後須輸入 private 密碼才能使用 BBS）。**僅在 admin 模式下有效**。
- **回覆**：成功 `OK`。

### 3.7 `/private unlock`

- **比對**：指令為 `private`，次 token 為 `unlock`。
- **行為**：關閉 private 模式。**僅在 admin 模式下有效**。
- **回覆**：成功 `OK`。

### 3.8 `/private status`

- **比對**：指令為 `private`，次 token 為 `status`。
- **行為**：查詢目前 private 為 locked 或 unlocked。**僅在 admin 模式下有效**。
- **回覆**：`Private: locked.` 或 `Private: unlocked.`

### 3.9 `/reset <admin 密碼>`

- **比對**：指令為 `reset`，後接**一個** token（**目前 PrefStore 中的 admin 登入密碼**，作為二次確認，格式與長度規則同一般 admin 密碼：4～8 字元可列印 ASCII，由指令解析之 token 緩衝截斷）。
- **前置**：**僅在已進入 admin 模式時有效**。未進入 admin 時使用 `/reset` 會得到與其他僅限 admin 指令相同之「未知指令」回覆（不另外揭露功能）。
- **行為**（admin 密碼驗證通過後）：
  1. **清除三個持久化 store**：依序清除 PmStore（所有郵件／新聞資料檔並重建為空）、UserStore（用戶檔）、PrefStore（偏好檔並在記憶體還原為出廠預設：admin／private 密碼回到預設 `12345678`、private 模式關閉、自訂問候語清空等，等同**首次使用**時 PrefStore 不存在檔案後之預設狀態）。
  2. **清除模組內所有記憶體 session 狀態**：所有節點↔userId 登入綁定、所有 admin 模式 slot、所有 private 解鎖 slot 一併清空（執行指令之節點亦會立即離開 admin 模式）。
- **回覆**：
  - 三個 store 皆清除成功：`OK`。
  - 未帶密碼 token：回覆用法提示（編譯語系；例如繁中 `用法:/reset 管理密碼`、英文 `Usage: /reset admin_pw`）。
  - admin 密碼錯誤：回覆與一般密碼錯誤相同之字串（如 `Bad password.`／`密碼錯誤`）。
  - 任一 `clearStorage` 失敗：回覆重置失敗字串（如 `Reset failed.`／`重置失敗.`）；**且不清除**登入綁定與 admin／private 解鎖等記憶體狀態，以便營運端重試。
- **備註**：成功後須以**預設** admin 密碼重新 `/admin <密碼>` 進入管理（除非後續再行變更）；與 `/clean messages|users|pref` 分開測試指令不同，`/reset` 會**同時**處理三 store 與全部 session，並列於 admin 模式之 `/h`、`/help`、`/?` 說明中。

---

## 4. Private 功能（可整段刪除後再分享）

Private 模式用於限制未授權使用者隨意使用 BBS；與是否登入一般帳號無關。

### 4.1 觸發條件與行為

- **未啟用時**：行為與一般 BBS 相同，無額外限制。
- **啟用後**：節點**僅接受** `/private <密碼>`；其餘指令**一律不回應**（不揭露 BBS 存在）。
- **密碼正確**：該節點「解鎖」，回覆 `Unlocked.` 與 15 分鐘閒置提示；之後可正常使用一般與 admin 指令。
- **密碼錯誤**：不回應。

### 4.2 閒置重鎖

- 解鎖後若該節點**超過 15 分鐘**無任何有效訊息，會再次鎖定；須重新輸入 `/private <密碼>`。
- 每一筆來自該節點的有效訊息都會**重設 15 分鐘**計時。

### 4.3 閒置重鎖與登入狀態

- 閒置 15 分鐘導致 private 重鎖時，**不解除該節點的登入狀態**（node↔userId 綁定仍保留）。
- 使用者再次輸入 `/private <密碼>` 解鎖後，**無須再次 `/hi`**，即可繼續以原帳號使用一般指令（如 `/m`、`/n`、`@user msg` 等）。

---

## 5. 隱藏功能（可整段刪除後再分享）

以下**不會**出現在 `/h` 說明中，供維護／測試或內部使用。

### 5.1 `/sync`

- **比對**：指令為 `sync`。
- **行為**：強制將 UserStore 與 PrefStore 的未寫入變更寫入 flash。
- **回覆**：成功 `Synced.`；失敗 `Sync failed.`

### 5.2 `/clean messages`

- **比對**：指令為 `clean`，次 token 為 `messages`。
- **行為**：清除 PmStore（所有郵件與新聞）。
- **回覆**：成功 `OK`；失敗 `Clean failed.`

### 5.3 `/clean users`

- **比對**：指令為 `clean`，次 token 為 `users`。
- **行為**：清除 UserStore（所有用戶與 session）。
- **回覆**：成功 `OK`；失敗 `Clean failed.`

### 5.4 `/clean pref`

- **比對**：指令為 `clean`，次 token 為 `pref`。
- **行為**：清除 PrefStore 並還原為預設值。
- **回覆**：成功 `OK`；失敗 `Clean failed.`

### 5.5 PrefStore 寫入時機

- 一般營運下 PrefStore **僅在** `/admin bye` 或 `/sync` 時寫入 flash（若 dirty）。
- **Deep sleep／reboot** 時會自動對 UserStore 與 PrefStore 執行 flush（若 PrefStore 有 dirty）。

---

## 6. 儲存與常數

### 6.1 UserStore

- 用戶帳號、密碼雜湊（與 iterated SHA256 同機制）、最後登入。
- A/B 輪寫；可設定 flush 間隔與不活動 eviction 天數（config 與編譯參數）。

### 6.1.1 Session（登入／Admin／Private 解鎖）為記憶體狀態

- **登入狀態**：節點↔userId 綁定僅存於記憶體，**不寫入 flash**。重開機或 deep sleep 喚醒後，所有綁定清除，使用者須重新 `/hi` 登入。
- **Admin 模式**與 **Private 解鎖**（某節點已輸入 private 密碼）亦為記憶體內狀態，重開後一併清除；private 模式開關本身仍由 PrefStore 持久保存。

### 6.2 PmStore

- 固定 slot 長度（如 128 bytes）、單則文字上限（如 104 bytes）；郵件與新聞共用同一儲存。

### 6.3 PrefStore

- 256 bytes 固定區塊：版本、admin/private 密碼 SHA256（各 32 bytes）、private 開關、問候語（104 bytes C string）、保留區。
- 預設 admin/private 密碼均為 `12345678`（儲存為 hash）。

### 6.4 常數摘要（可於 config.h / 編譯覆寫）

| 類別 | 常數範例 | 說明 |
|------|----------|------|
| 回應 | MAX_RESPONSE, MAX_LINE | 單則最長、列表每行最長 bytes |
| 用戶 | MAX_USERS, USERNAME_MIN/MAX, PASSWORD_MIN/MAX | 用戶數、帳密長度 |
| 儲存 | PM_SLOT_SIZE, PM_FILE_SIZE, PM_RECORD_TEXT_LEN | PmStore 佈局與單則長度 |
| 偏好 | PREF_FILE_SIZE, PRIVATE_IDLE_SEC | PrefStore 大小、private 閒置秒數 |
| 列表 | LIST_MAX, LIST_PREVIEW_LINES | 一次取得筆數、預覽行數 |
| 語言 | KLTINYBBS_LANG | 編譯期 UI 語言，見 6.6 |

### 6.4.1 平台差異參數（ESP32 vs nRF52）

下表整理 `config.h` 目前對兩平台分流定義的參數（其餘未列者在兩平台相同）：

| 參數 | ESP32 (`ARCH_ESP32`) | nRF52 (`ARCH_NRF52`) | 差異說明 |
|------|----------------------|----------------------|---------|
| `KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC` | `1800` 秒 | `3600` 秒 | ESP32 更頻繁 flush（較偏向資料耐久）；nRF52 間隔較長（較偏向 flash 壽命）。 |
| `KLTINYBBS_MAX_USERS` | `32` | `8` | ESP32 可容納較多使用者；nRF52 受資源限制採較小上限。 |
| `KLTINYBBS_PM_FILE_SIZE` | `16384` bytes | `2048` bytes | ESP32 給較大訊息儲存空間；nRF52 採精簡容量。 |

> 補充：以下參數在兩平台目前相同：`KLTINYBBS_SHA_ITER=2000`、`KLTINYBBS_INACTIVE_SECS=30天`、`KLTINYBBS_PM_SLOT_SIZE=128`、`KLTINYBBS_PM_RECORD_TEXT_LEN=104`。
>  
> 若未定義 `ARCH_ESP32` / `ARCH_NRF52`（例如 host tools），fallback 預設採用 nRF52 同級數值（`flush=3600`、`max_users=8`、`pm_file_size=2048`）。

### 6.5 Eviction（踢人）與 PmStore

- 當 BBS 已滿（`userCount() >= MAX_USERS`）且新用戶註冊時，會依**最後登入時間**選出超過設定不活動天數的**最久未登入**用戶，將其帳號刪除並釋出 userId 給新用戶使用（eviction）。
- **若無符合條件之用戶**（例如所有人皆在不活動天數內曾登入），則不進行踢人，新用戶註冊失敗，回覆 BBS 滿無空位（如 `BBS full, no slot.`）。
- **踢人後不繼承舊信、不誤掛舊新聞作者**：在刪除該用戶（`deleteUser`）並釋出 userId **之前**，會對 PmStore 執行 `markDeletedForEvictedUser(userId)`：
  1. **收件人為該 userId 的私人郵件**（`to == userId` 且 `type == MSG_PM`）全部標記為刪除 → 新用戶不會看到前手收到的信。
  2. **發件人為該 userId 的新聞**（`from == userId` 且 `type == MSG_ANN`）全部標記為刪除 → 前手張貼的新聞不會以新用戶名義顯示。
- 列表與讀取時會略過已標記刪除的 slot，因此新用戶使用同一 userId 時不會繼承舊信，也不會被誤掛為舊新聞作者。

### 6.6 UI 語言（編譯期選擇）

- **常數**：`config.h` 內 `KLTINYBBS_LANG`，編譯時決定納入的字串語系。
- **選項**：
  - `KLTINYBBS_LANG_EN`（0，預設）：英文。所有提示、錯誤訊息、`/h` 說明等為英文。
  - `KLTINYBBS_LANG_ZH_TW`（1）：繁體中文。字串由 `KLTinyBBSStrings.cpp` 以條件編譯納入；設計上盡量簡短以節省傳輸。
- **使用方式**：編譯時定義 `KLTINYBBS_LANG=KLTINYBBS_LANG_ZH_TW` 或 `KLTINYBBS_LANG=1` 即為繁體中文；未定義時為英文。
- **範圍**：影響 `KLTinyBBSStrings` 所提供之所有對外字串（含登入提示、錯誤訊息、Mail/News/User 標頭、`/h` 說明、Admin/Private 相關回覆等）。指令本身（如 `/hi`、`/bye`）與協定不因語言而改變。

---

## 7. 非功能需求（實作約定）

- 列表與訊息摘要使用 **UTF-8 安全截斷**（不切斷多 byte 字元）；實作侷限在模組內。
- 時間顯示使用節點本地時間（依 RTC 時區在模組內換算，不依賴 RTC 以外模組介面）；時間戳為 0 時顯示為無效時間字串。
- 功能與編碼處理均侷限在 KLTinyBBS 模組內，不修改專案其他檔案（除模組註冊與 config 常數外）。
- 回覆封包經由 `sendReply(toNode, text, len)` 統一送出，長度上限由 `MAX_RESPONSE` 控管。
- Admin 與 Private 解鎖的並行節點數有上限（實作上為固定 slot 數，如 8）；超過時新節點無法進入 admin 或無法佔用解鎖 slot（依實作為準）。
