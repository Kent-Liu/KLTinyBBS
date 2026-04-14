# KLTinyBBS 編譯指南

> ⚠️ **警告 1**
> 若要自行編譯模組，可能需要有能力自行編譯 Meshtastic 的原始碼，以及有能力自行燒錄韌體進入設備中；在本專案的教學以及文件中將不會詳述這些內容，請使用者自行查閱官方文件。

> ⚠️ **警告 2**
> 目前模組只有在 `2.7.15` 的韌體中編譯、測試過。作者本身沒有時間針對不同版本的韌體與硬體逐一測試，但理論上仍有機會可成功；使用者需自行承擔風險，作者不對任何可能產生的軟硬體問題負責。

本文件說明如何將 KLTinyBBS 模組整合進官方 Meshtastic 韌體原始碼後自行編譯。

---

## 編譯前準備

- 先準備一份可正常編譯的官方 Meshtastic 韌體原始碼環境。
- 確認你有寫入目標韌體專案目錄與燒錄設備的權限。
- 建議先在乾淨分支或備份環境操作，避免覆蓋既有修改。

---

## 整合模組步驟

1. **複製模組原始碼到官方目錄**
   - 將本專案的 `src/modules/KLTinyBBS/` 整個資料夾，複製到官方韌體原始碼的 `src/modules/` 目錄下。

2. **在 `src/modules/Modules.cpp` 加入 include**
   - 開啟官方韌體專案中的 `src/modules/Modules.cpp`。
   - 在其他 module include 區段新增：
     - `#include "KLTinyBBS/KLTinyBBSModule.h"`

3. **在模組註冊區加入 KLTinyBBS**
   - 於 `Modules.cpp` 內的 `Example: Put your module here` 區段，新增：
     - `new KLTinyBBSModule();`

---

## 後續編譯與燒錄

- 完成上述整合後，請依你原本的 Meshtastic 編譯流程建置韌體。
- 編譯成功後，再依你的設備流程進行燒錄與上機測試。
- 若編譯失敗，建議先確認：
  - `KLTinyBBS/` 目錄是否放在正確路徑。
  - `Modules.cpp` 的 include 路徑是否正確。
  - `new KLTinyBBSModule();` 是否放在正確的模組註冊區塊。
