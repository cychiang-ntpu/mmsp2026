# TextLink 介面文件（範本）

> 目標：**別組只看這份文件，就能寫出與我們互通的程式。** 每個欄位寫清楚幾 bytes、什麼順序、
> 位元組順序與位元順序。標「（待填）」的地方請換成你們的設計；殼已經定好的部分先幫你們填了，改了程式要同步改這裡。

組別：（待填）　成員與角色：（待填）　文件版本／日期：（待填）

## 1. 模組與資料流

（待填：一張圖或一段文字，說明 main → net → frame → utf8／huffman → chat／transfer 之間誰呼叫誰、資料怎麼流。）

## 2. frame 外框（規格固定）

| 欄位 | bytes | 說明 |
|---|---|---|
| length | 4 | big-endian 無號整數＝type＋payload 的 bytes 數；合法範圍 1–16,777,216 |
| type | 1 | 見下表 |
| payload | length − 1 | 依 type 而定 |

## 3. 各 type 的 payload

### 0x01 TEXT_RAW

UTF-8 文字，不含結尾 `\0`；最長 4095 bytes。接收端以 RFC 3629 檢查，不合法則丟棄並顯示系統訊息。

### 0x02 TEXT_HUFF

（待填：是第 4 節的 Huffman 區塊原樣放入，還是用固定 codebook？壓完比原文大時怎麼處理？理由？）

### 0x10 FILE_BEGIN（傳送端 → 接收端）

| 欄位 | bytes | 說明 |
|---|---|---|
| mode | 1 | 0＝raw、1＝huff |
| orig_size | 8 | big-endian；原檔 bytes 數；上限 64 MiB |
| data_size | 8 | big-endian；之後所有 FILE_DATA 的 payload 總 bytes 數；raw 時必須等於 orig_size |
| name | 其餘 | 檔名，不含路徑；接收端只保留 `0-9 A-Z a-z . - _`，其餘換成 `_` |

### 0x11 FILE_DATA（傳送端 → 接收端）

傳輸資料的一段，每個最多 65,536 bytes，依序接起來共 data_size bytes。
raw：就是檔案內容。huff：第 4 節 Huffman 區塊的一段。

### 0x12 FILE_END

傳送端 → 接收端：payload 為空，表示資料送完。（待填：若加了 checksum，寫在這裡。）
接收端 → 傳送端：payload 1 byte，0＝解碼與寫檔成功、其他＝失敗。傳送端收到 0 才以結束碼 0 結束。

## 4. Huffman 區塊格式（`huff_encode` 的輸出）

（待填，這是本文件最重要的一節。至少要回答：）

- 三種符號（UTF-8 字元、16-bit sample、byte）各怎麼切？區塊裡怎麼記錄用的是哪一種？
- WAV 的檔頭與 data 區以外的 bytes 放在區塊的哪裡？data 長度是奇數時怎麼辦？
- 每種符號在 codebook 裡佔幾 bytes？
- 原始長度放在哪裡、幾 bytes、什麼位元組順序？
- codebook 怎麼存？每個欄位幾 bytes？一個有 K 種符號的輸入，codebook 共幾 bytes？
- 解碼端如何由 codebook 重建出與編碼端**完全相同**的 code？（平手時誰的 code 比較小？）
- bitstream 的位元順序：每個 byte 先填高位還是低位？最後不足 8 bits 補什麼？
- 邊界：空輸入、只有一種符號時，輸出長什麼樣子？
- 附一個手算得出來的小例子（例如 `ABRACADABRA`），列出每個 byte 的 hex。

## 5. 錯誤處理

| 情況 | 我們的行為 |
|---|---|
| length 為 0 或超過上限 | 關閉連線，結束碼非 0 |
| 不認得的 type、frame 順序不對 | （待填） |
| 傳到一半斷線 | 不留下輸出檔（先寫 `.part`，成功才改名），結束碼非 0 |
| Huffman 區塊損壞 | （待填：哪些檢查？） |
| 非法 UTF-8 | 丟棄該則訊息並顯示系統訊息 |

## 6. 已知限制

（待填）
