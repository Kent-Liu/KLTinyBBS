# KLTinyBBS 管理者操作手冊

## ⚠️ 首次使用前必看

KLTinyBBS 預設 admin 密碼為 `12345678`。為了避免他人未授權進入管理模式，**第一次啟用後請立即登入 admin 並變更密碼**（`/admin pass 新密碼`），再開始其他設定。

---

## 平台差異與規格限制（nRF52 / ESP32）

KLTinyBBS 在 nRF52 與 ESP32 都可運作，但儲存容量與可用資源不同，建議先依部署平台評估可承載規模。

| 項目 | ESP32 | nRF52 | 說明 |
|------|-------|-------|------|
| 最多用戶數 | 32 | 8 | `KLTINYBBS_MAX_USERS`，影響可註冊帳號上限。 |
| 訊息儲存空間（PM/News 共用） | 16384 bytes | 2048 bytes | `KLTINYBBS_PM_FILE_SIZE`，空間滿時需靠刪除舊資料騰出容量。 |
| 單則訊息最大長度 | 104 bytes | 104 bytes | `KLTINYBBS_PM_RECORD_TEXT_LEN`，私訊與公告都受此限制。 |
| 自動 flush 間隔 | 1800 秒 | 3600 秒 | `KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC`，影響資料寫入Flash頻率。 |
| private 閒置重鎖時間 | 900 秒 | 900 秒 | `KLTINYBBS_PRIVATE_IDLE_SEC`，解鎖後閒置 15 分鐘會自動上鎖。 |
| 列表每次最多取得筆數 | 16 | 16 | `KLTINYBBS_LIST_MAX`，讀取列表的單次回傳上限。 |
| 摘要預覽最多行數 | 5 | 5 | `KLTINYBBS_LIST_PREVIEW_LINES`，`/m`、`/n` 預設預覽行數。 |

---

## 進入與離開 Admin 模式

- **進入 admin**：輸入 `/admin 管理密碼`
  - 密碼正確回覆 `OK`，即進入 admin 模式。
  - 密碼錯誤會回覆未知指令樣式，不會額外揭露 admin 入口。

- **離開 admin**：輸入 `/admin bye`
  - 回覆 `OK` 後離開 admin 模式。
  - 若有尚未寫入的偏好設定，系統會在此時一併寫入。

- **查看 admin 指令**：在 admin 模式內輸入 `/h`、`/help` 或 `/?`
  - 顯示內容會切換為管理者可用指令集合。

---

## Admin 主要功能

以下功能需在 **admin 模式** 下操作；未進入 admin 時，多數管理指令會被視為未知指令。

- **變更 admin 密碼**：`/admin pass 新密碼`
  - 新密碼規則：4～8 個可顯示 ASCII 字元。
  - 建議定期更換，並避免使用可猜測字串。

- **設定登入歡迎詞**：`/welcome 文字內容`
  - 成功登入時，會在歡迎訊息後附加這段文字。
  - 長度有上限（約 103 bytes），過長會收到失敗提示。

- **private 模式管理**：
  - `/private status`：查看目前是 `locked` 或 `unlocked`。
  - `/private lock`：啟用 private 模式。
  - `/private unlock`：停用 private 模式。
  - `/private pass 新密碼`：變更 private 解鎖密碼（4～8 個可顯示 ASCII）。

- **刪除任意公告**：`/delete post N`
  - `N=1` 代表最新一則。
  - admin 可刪除任何使用者發佈的公告；一般使用者僅可刪除自己的公告。

- **重置整個 BBS**：`/reset 管理密碼`
  - 會清空郵件/公告、使用者資料、偏好設定，並清除所有登入與 admin/private session。
  - 成功後 admin/private 密碼會回到預設值 `12345678`，需重新登入管理。

---

## Admin 功能限制與注意事項

- admin 與 private 解鎖狀態屬於**記憶體 session**，重開機或 deep sleep 喚醒後會失效。
- private 是否啟用屬於持久設定（PrefStore），重啟後仍維持 lock/unlock 設定值。
- 管理密碼與 private 密碼長度固定為 4～8 字元，且需為可顯示 ASCII（不含中文）。
- 指令回覆長度受封包限制，列表與長訊息會以摘要方式回傳，必要時請分批查閱。

---

## Private 模式說明

Private 模式用來避免未授權節點直接使用 BBS。啟用後，目標節點在解鎖前幾乎無法操作一般指令。

- **啟用後的行為**：
  - 節點僅接受 `/private 密碼` 進行解鎖。
  - 其他指令不回應（降低被探測風險）。

- **解鎖後的有效期**：
  - 正確輸入 `/private 密碼` 會回覆 `Unlocked.`，並進入可使用狀態。
  - 若該節點閒置超過 15 分鐘，會自動重鎖。
  - 解鎖期間每次有效操作都會重設 15 分鐘計時。

- **與登入狀態關係**：
  - private 因閒置重鎖時，不會強制登出既有帳號綁定。
  - 同一節點再次解鎖後，通常可直接延續原本登入狀態。

---

## Private 模式操作教學（建議流程）

以下流程可用於實務部署（例如固定節點、家人親友共享設備）：

1. 先以 `/admin 管理密碼` 進入管理模式。
2. 執行 `/private pass 新密碼`，先更新 private 密碼。
3. 執行 `/private lock` 啟用 private 模式。
4. 用一般使用者節點測試：先送一般指令（應無回應），再送 `/private 新密碼`（應回覆 `Unlocked.`）。
5. 驗證完成後，管理者可用 `/admin bye` 離開管理模式。
6. 若有需要更深入的隱藏設備的存在，也可以適當的設定裝置為 CLIENT_HIDDEN。

若需暫時開放不鎖定，可由管理者進入 admin 後執行 `/private unlock`；作業結束再視需要重新 `/private lock`。
