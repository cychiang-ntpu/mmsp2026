# Team 1｜TextLink：文字、封包與 Huffman 傳輸

- 期間：9/21－10/12｜繳交截止：**10/11（日）18:00**｜評測：10/12（三節課內）
- 團隊 repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、
  `docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`
- 繳交：登錄 repo URL＋完整 commit SHA
- 語言：C（`gcc -Wall -Wextra` 無警告）；測試腳本與畫圖可以用 Python

## 一句話

做一支叫 `textlink` 的程式：兩台電腦用 TCP 互傳**任何 UTF-8 文字**、**大文字檔**與 **WAV 檔**，
傳輸內容可以選擇「原樣送」或「先用 Huffman coding 壓縮再送」，收端還原後必須與原檔**逐 byte 相同**；
最後用量測數字回答兩個問題：**壓縮率是多少？壓縮之後，到底有沒有比較快？**

## 三個功能

### 功能 1｜文字聊天：所有 UTF-8 字元＋Huffman

- 兩端互傳文字訊息。1、2、3、4 bytes 的 UTF-8 字元都要正確（英文、`é`、中文、emoji `😀`、
  組合 emoji `👨‍👩‍👧`、罕用字 `𠮷`），見[第 2 週講義 1.2c](../../lectures/wk02_0914_text-utf8/README.md)。
- 訊息經 TCP 傳輸時必須有**長度前綴**，半包、黏包都要能正確還原成一則一則的訊息。
- 傳送路徑要有 Huffman encode、接收路徑要有 Huffman decode，並可用參數切換 `--raw`／`--huff`。
  **符號是 UTF-8 字元**：統計每個字元出現的機率再編碼（見「Huffman 的要求」）。
- 收到的文字要檢查是合法 UTF-8（RFC 3629：拒絕 overlong、代理區、超過 U+10FFFF）；不合法的訊息不可讓程式當掉。
- **你們會遇到的設計問題**：一則聊天訊息只有幾十 bytes，如果每則都附上自己的 codebook，壓完會比原文還大。
  可能的做法：(a) 每則附 codebook，照實報告膨脹了多少；(b) 兩端內建一份事先統計好的固定 codebook；
  (c) 壓完比較大就改送 raw。選哪一種由你們決定，在 `docs/interface.md` 寫下理由，
  並在報告中列出至少 5 則不同長度訊息的「原始 bytes／實際上線 bytes（含 codebook）」。

### 功能 2｜大文字檔傳輸與速度比較

- 傳送端讀入一個大文字檔（UTF-8，至少 1 MB），**以 UTF-8 字元為符號**統計機率、做 Huffman 編碼後傳輸；接收端解碼、寫回檔案。
- 還原的檔案必須與原檔逐 byte 相同（含 `\r\n`、BOM、檔尾有沒有換行，**一個 byte 都不能改**）。
- 同一個檔案分別用 `--raw` 與 `--huff` 傳，比較速度（量什麼、怎麼量，見「量測」）。

### 功能 3｜WAV 檔傳輸與速度比較

- 同功能 2，但檔案是 WAV（16-bit PCM，至少 1 MB）。**符號是 16-bit 的 sample value**：解析 RIFF 檔頭找到 data 區，
  對 sample 值做 histogram，再用這個機率分布做 Huffman 編碼。檔頭與其他 chunk 不是 sample，原樣保留；
  還原後要逐 byte 相同、能正常播放。
- 同樣比較 `--raw` 與 `--huff`。**WAV 的壓縮率會和文字檔差很多，報告要解釋為什麼**
  （提示：畫 sample 值的 histogram、算熵，再看看出現了幾種不同的 sample 值、codebook 佔了多少）。

## 固定的部分（各組一律相同，評測會照這個跑）

### 命令列介面

```
textlink chat server <port> [--bind <ip>] [--raw|--huff]   # 功能 1：等對方連進來
textlink chat client <ip> <port> [--raw|--huff]            # 功能 1：連到對方
textlink recv <port> <outdir> [--bind <ip>]                # 功能 2、3：收一個檔案存到 outdir，收完結束
textlink send <ip> <port> <file> [--raw|--huff]            # 功能 2、3：送一個檔案，送完結束
```

- **IP 與 port 一律由命令列指定，程式裡不可以寫死**（不可寫死 `127.0.0.1`、不可寫死 port 號）。
  評測是**兩台不同的電腦**互連：一台跑 `chat server`／`recv`，另一台用對方的 IP 跑 `chat client`／`send`。
- 連線端（`chat client`、`send`）：`<ip>` 是對方電腦的 IPv4 位址（例如 `192.168.1.23`），`<port>` 是對方監聽的 port。
- 監聽端（`chat server`、`recv`）：`<port>` 是自己要監聽的 port（1024–65535）。預設監聽本機**所有**網路介面（`0.0.0.0`，
  也就是 `INADDR_ANY`），別台電腦才連得進來；加 `--bind <ip>` 則只監聽指定的那個本機 IP
  （電腦同時有 Wi-Fi 與有線網路時用得到；`--bind 127.0.0.1` 代表只接受本機連線）。
- 監聽端啟動後要印出自己正在監聽的 IP 與 port；連線成功後兩端都要印出對方的 IP 與 port，展示與除錯時才知道連到誰。
- IP 格式錯誤、port 超出範圍、port 被占用、連不上對方：印出看得懂的錯誤訊息，結束碼非 0，不可當掉或無限等待
  （連線逾時請設上限，建議 10 秒）。
- 預設模式是 `--huff`。接收端不需要指定模式，要能從封包自己判斷。
- **聊天畫面裡也要能傳檔案**（展示時用這個；`send`／`recv` 兩個指令是給自動測試與量測用的）：

  | 聊天中輸入 | 作用 |
  |---|---|
  | `/files [資料夾]` | 列出該資料夾（預設目前資料夾）裡的 `.txt` 與 `.wav`，附編號與大小 |
  | `/send <編號或路徑>` | 用目前的模式（RAW／HUFF）把檔案傳給對方；同一條連線，傳完可以繼續聊天 |
  | `/raw`、`/huff` | 切換之後送出的訊息與檔案要不要經 Huffman 編碼 |
  | `/quit` | 離開 |

  對方的聊天畫面要自動接收、解碼、存到 `received/` 資料夾；**兩端畫面都要顯示**檔名、原始 bytes、上線 bytes、
  壓縮率、編碼或解碼時間，傳送端還要顯示對方是否成功存檔。
- `send`／`recv` 成功時結束碼 0；任何失敗（連不上、中途斷線、解碼失敗、檔案寫不進去）結束碼非 0，
  而且**不可以留下一個看起來完整、其實壞掉的輸出檔**。
- 建置：在 repo 根目錄打 `make`（Windows PowerShell 為 `mingw32-make`）要產生 `textlink`（`textlink.exe`）；
  `make test` 要跑完你們 `tests/` 裡的自動測試。

### 封包外框（frame）

TCP 上的每一個封包都是：

```
+----------------------+-----------+----------------------+
| length：4 bytes      | type：1   | payload：length-1    |
| big-endian，無號整數  | byte      | bytes                |
+----------------------+-----------+----------------------+
length = type 與 payload 的總 bytes 數（不含 length 自己）；合法範圍 1 到 16,777,216（16 MiB）
```

| type | 名稱 | payload |
|---|---|---|
| `0x01` | TEXT_RAW | UTF-8 文字，不含結尾 `\0` |
| `0x02` | TEXT_HUFF | Huffman 編碼後的文字（格式各組自訂） |
| `0x10` | FILE_BEGIN | 檔名、原始大小、模式等（格式各組自訂） |
| `0x11` | FILE_DATA | 檔案內容的一段，raw 或 Huffman bitstream（格式各組自訂） |
| `0x12` | FILE_END | 結束與完整性檢查資訊（格式各組自訂） |

- 收到 length 為 0、超過上限，或不認得的 type：回報錯誤並關閉連線，不可當掉、不可照著 length 去配置記憶體。
- 大檔案請切成多個 FILE_DATA 送（建議每個 64 KiB 以內），不要整個檔案塞一個 frame。
- 「各組自訂」的 payload 格式要完整寫在 `docs/interface.md`：每個欄位幾 bytes、什麼順序、位元順序，
  寫到**別組只看文件就能寫出與你們互通的程式**的程度。codebook 怎麼傳是其中最重要的一段。

### 量測輸出

`send` 與 `recv` 結束前各在 **stderr** 印一行，欄位名稱固定（多印其他欄位可以）：

```
STATS role=send mode=huff sym=char file_bytes=1048576 wire_bytes=743210 ratio=0.7088 encode_ms=35.2 send_ms=12.8 total_ms=51.0
STATS role=recv mode=huff file_bytes=1048576 wire_bytes=743210 ratio=0.7088 decode_ms=41.7 total_ms=55.3
```

- `sym`（只有 `role=send` 需要）：這次 Huffman 用的符號，`char`（UTF-8 字元）、`s16`（16-bit sample）或 `byte`；`--raw` 時印 `none`。
- `file_bytes`：原檔大小；`wire_bytes`：實際送上 TCP 的總 bytes（**含所有 frame 的 header 與 codebook**）。
- `ratio`：**壓縮率** = `wire_bytes ÷ file_bytes`，印到小數第 4 位；`file_bytes` 為 0 時印 `0.0000`。`--raw` 也要印（會略大於 1，多出來的是 frame 標頭）。
- `encode_ms`／`decode_ms`：純編、解碼時間（`--raw` 時為 0）；`total_ms`：從開檔到最後一個 byte 送出／寫完。
- 計時用單調時鐘（`clock_gettime(CLOCK_MONOTONIC, …)`），不要用 `time()` 或 `clock()`。

## Huffman 的要求

- 自己實作：切符號 → 統計機率 → 建樹 → 產生 codebook → 位元打包 → 解碼。不可使用現成的壓縮函式庫（zlib 等）。
- **符號的定義（必做，不是加分）**：Huffman 是對「符號出現的機率」編碼，符號定得對不對，決定能壓到多小。

  | 資料 | 符號 | 怎麼得到機率 |
  |---|---|---|
  | 文字（聊天訊息、`.txt`） | **一個 UTF-8 字元**（code point）。「多」是一個符號，不是 `E5`、`A4`、`9A` 三個 | 把 bytes 切成字元（MP1 做過），統計每個字元出現的次數 |
  | WAV（`.wav`，16-bit PCM） | **一個 16-bit sample value**（−32768 到 32767） | 解析 RIFF 找到 data 區，每 2 bytes（little-endian）一個 sample，做 sample 值的 histogram；雙聲道就是左右交錯的 sample |
  | 其他檔案，或內容不符（`.txt` 其實不是合法 UTF-8、`.wav` 不是 16-bit PCM） | 一個 byte | byte 的 histogram；這是保證「任何檔案都能還原」的退路 |

  編碼後的資料要自己記下用的是哪一種符號，接收端才知道怎麼還原。WAV 的 data 區不一定從第 44 byte 開始
  （中間可能有 `LIST` chunk），請逐個 chunk 走，不要寫死 44。
- 符號種類會很多（sample 最多 65,536 種、字元數千種）：建樹不能用「每輪線性找最小」，codebook 怎麼存也要設計，
  否則 codebook 本身就把省下來的空間吃掉了。
- **必須計算並報告壓縮率**，定義見下一節「壓縮率」。
- 必須處理的邊界：空檔案（0 byte）、只有一種符號的檔案（只有一個符號時 code 長度不能是 0）、
  1–4 bytes 的字元混合且帶 BOM 與 CRLF 的文字、data 前後有其他 chunk 或 data 長度為奇數的 WAV、只有檔頭的 WAV、
  最後一個 byte 沒填滿的位元（解碼端要知道有效位元到哪裡，不能多解出幾個符號）。
- 與 MP4 的關係：MP4 是個人作業、同樣以 UTF-8 字元為符號、輸出 codebook CSV；Team 1 要再加上 sample 與 byte 兩種符號、
  在記憶體中編解碼、codebook 用二進位傳輸、還要能拒絕壞資料。觀念相同、程式不同。可以互相討論演算法，
  **但 MP4 仍須各自獨立完成**，團隊程式中沿用了誰的 MP4 程式要寫在 `CONTRIBUTIONS.md`。

## 壓縮率：每次傳輸都要算，報告要分析

**本課程的定義：壓縮率 = 壓縮後的大小 ÷ 原始大小**，用百分比表示，**越小越好**；超過 100% 代表「壓完反而變大」。
（有些教科書用倒數「原始 ÷ 壓縮後」，例如 2:1；報告裡請一律用本課程的定義，並寫出算式，不要只寫一個數字。）

程式要算、要顯示的：

| 哪裡 | 算什麼 | 怎麼呈現 |
|---|---|---|
| `send`／`recv` | 傳輸壓縮率 = `wire_bytes ÷ file_bytes`（含所有 frame 標頭與 codebook） | `STATS` 的 `ratio=` 欄位，另在畫面上印成百分比 |
| 聊天 | 每一則訊息：原始 bytes、上線 bytes、兩者的比 | 顯示在該則訊息旁（starter 的殼已把它畫在泡泡下方） |

報告要有一張「壓縮率分析表」，每個測試檔一列，欄位如下，並附上你們怎麼算出這些數字的程式或指令：

| 欄位 | 意義 |
|---|---|
| 原始 bytes、符號個數 N、符號種類數 K | 檔案大小；切出幾個符號（字元數或 sample 數）；其中有幾種不同的符號 |
| 熵 H（bits／符號） | `H = −Σ p(x) log2 p(x)`，由符號的機率算出；這是這種符號定義下能壓到的理論下限 |
| 平均碼長 L（bits／符號） | `L = Σ p(x) × len(x)`，由你們的 codebook 算出；應滿足 `H ≤ L < H + 1`，不滿足就是程式有錯 |
| 理論壓縮率 `H × N ÷ 8 ÷ 原始 bytes`、純編碼壓縮率 `L × N ÷ 8 ÷ 原始 bytes` | 都不含 codebook |
| codebook bytes | 你們的格式下，codebook 佔多少；佔原檔的百分之幾 |
| **對照：改以 byte 為符號** | 同一個檔案若以 byte 為符號，H 與理論壓縮率各是多少（只要算，不必真的傳） |
| 實際壓縮率 | `huff_encode 的輸出 bytes ÷ 原始 bytes`（含 codebook）與 `wire_bytes ÷ file_bytes`（再含 frame 標頭）各一欄 |

要回答的問題：

1. 實際壓縮率與純編碼壓縮率差多少？差距來自哪裡（codebook、補位、frame 標頭）？哪一種檔案的 codebook 特別大，為什麼？
2. 聊天的短訊息壓縮率是多少？在你們的設計下，訊息要多長壓縮率才會低於 100%？
3. **符號定義的影響**：中文文字以字元為符號，比以 byte 為符號好多少？英文文字呢？為什麼兩者差這麼多？
4. WAV 以 sample 為符號，熵的下限比以 byte 為符號低多少？實際壓縮率有跟著好那麼多嗎？
   出現了幾種不同的 sample 值？codebook 吃掉了多少？提出一個你們認為能改善的方法（不一定要實作）。

## 量測與報告：壓縮之後有沒有比較快？

| 項目 | 要求 |
|---|---|
| 檔案 | 至少 4 個：中文為主的文字檔、英文為主的文字檔、語音或音樂 WAV、一個你們自選的檔案；每個 ≥ 1 MB |
| 模式 | 每個檔案 `--raw`、`--huff` 各跑至少 5 次，報**中位數**，並附上原始數據（CSV） |
| 環境 | (1) 同一台電腦 `127.0.0.1`；(2) 兩台電腦經教室網路或手機熱點。寫明是哪一種網路 |
| 數字 | `file_bytes`、`wire_bytes`、**壓縮率**（見上一節）、`encode_ms`、`decode_ms`、兩端 `total_ms`、換算的有效傳輸速率（MB/s） |
| 圖 | 至少一張：各檔案 raw 與 huff 的總時間對照；一張：WAV 的 sample 值 histogram；一張：文字檔最常見的前 30 個字元與其機率 |

報告必須回答：

1. 哪些情況 Huffman 比較快、哪些比較慢？**比較慢是正常而且很可能的結果**（在 `127.0.0.1` 上傳輸幾乎不花時間，
   編、解碼的 CPU 時間就是淨損失）。照實報告，不要為了「壓縮比較快」去調數字。
2. 算出**損益平衡頻寬**：網路慢到多少 Mbps 以下，壓縮才開始划算？
   （省下的 bytes × 8 ÷ 頻寬 ＝ 省下的傳輸時間；它要大於 `encode_ms + decode_ms`。）
3. 為什麼 WAV 壓得比文字差？從 sample 值的 histogram 與熵解釋。

加分方向（做了寫進報告，沒做不扣分）：`send` 加 `--limit-kbps N` 模擬慢速網路來驗證第 2 題的計算；
把 codebook 存得更省（例如符號遞增時只存差距）；WAV 改用相鄰 sample 的差值當符號（Team 2 的前導）；
聊天改用兩端內建的固定 codebook。

## 最低驗收

評測時老師會用**沒公布過的檔案**（含 1–4 bytes 字元混合的文字檔、帶 BOM 與 CRLF 的文字檔、WAV、空檔案、
單一 byte 重複的檔案；單檔 20 MB 以內）。**第 1、3、4 項在兩台不同的電腦之間驗**，IP 與 port 由老師當場指定；
只能在同一台電腦 `127.0.0.1` 上跑通的，這三項不算過。每一項都是「過／不過」：

| # | 項目 | 怎麼驗 |
|---|---|---|
| 0 | 跨機器連線：IP 與 port 由命令列指定 | 老師指定 port（例如 `5000` 換成 `6123`）與哪一台當監聽端，兩台電腦互連成功，兩端印出對方 IP 與 port |
| 1 | 聊天：中文、emoji、4-byte 字元 round-trip | 兩端互傳，顯示正確；`--raw` 與 `--huff` 都要過 |
| 2 | 半包／黏包仍可還原 | 用測試工具把 frame 切成 1 byte 1 byte 送、或 5 則不停頓連送，訊息數與內容正確 |
| 3 | 大文字檔 `--raw`、`--huff` 傳輸後逐 byte 相同 | `cmp`／`fc.exe /b` 或 SHA-256；`send`／`recv` 指令與聊天中的 `/send` 兩種方式都要過 |
| 4 | WAV `--raw`、`--huff` 傳輸後逐 byte 相同 | 同上，且可播放 |
| 5 | Huffman 邊界：空檔、單一符號檔、256 種 byte 全出現的二進位檔、帶 BOM／CRLF／1–4 bytes 字元的文字、data 前有其他 chunk 的 WAV、8-bit 的 WAV、內容不是 UTF-8 的 `.txt` | 同上；後兩者要能自動退回以 byte 為符號 |
| 5a | 符號定義正確 | `STATS` 的 `sym`：文字檔為 `char`、16-bit WAV 為 `s16`；老師的中文文字檔 `ratio` 要小於 0.55（以 byte 為符號做不到） |
| 6 | 壞輸入不當機、不越界 | length 為 0 或超大、type 不認得、傳到一半斷線、codebook 被改壞、非法 UTF-8；程式回報錯誤並正常結束 |
| 7 | `STATS` 輸出（含 `ratio`）與結束碼符合規格 | 老師的腳本會直接 parse，並用 `wire_bytes ÷ file_bytes` 驗算 `ratio` |
| 7a | 壓縮率合理 | 老師的中文文字檔 `--huff` 的 `ratio` 要小於 1；`--raw` 的 `ratio` 要在 1.0000–1.0100 之間（`wire_bytes` 有照實計入標頭） |
| 8 | `make`、`make test` 在乾淨的 clone 上可重現 | 指定 SHA，依 README 步驟建置 |

比對檔案的指令：

| macOS / Linux | Windows PowerShell |
|---|---|
| `cmp a.wav out/a.wav && echo same` | `fc.exe /b a.wav out\a.wav` |
| `shasum -a 256 a.wav out/a.wav`（Linux 為 `sha256sum`） | `Get-FileHash a.wav, out\a.wav` |

## 跨機器測試須知

平常開發用 `127.0.0.1` 沒問題，但**每週至少要在兩台電腦之間實測一次**，不要等到評測前一天才發現連不上。

| 狀況 | 怎麼辦 |
|---|---|
| 查自己的 IP | macOS `ipconfig getifaddr en0`；Linux `hostname -I`；Windows `ipconfig` 看「IPv4 位址」 |
| 先確認兩台互通 | 在連線端打 `ping <對方 IP>`；不通就不是你們程式的問題，先解決網路 |
| Windows 防火牆 | 監聽端第一次執行會跳出詢問，選「允許存取」（私人與公用網路都勾）；沒跳或按錯了，到「Windows 安全性 → 防火牆 → 允許應用程式通過防火牆」把 `textlink.exe` 加進去 |
| 校園 Wi-Fi 連不到同學的電腦 | 很多校園 Wi-Fi 會隔離同網段的裝置（client isolation）。改用**手機熱點**讓兩台電腦連同一支手機，或用有線網路 |
| WSL | WSL 裡的程式當監聽端時，別台電腦預設連不進來，見 [wsl_c_starter.md](../../docs/tutorials/wsl_c_starter.md) 6-4；評測時 WSL 同學建議當連線端 |
| 一台 Windows、一台 macOS | 必須能互通：這就是為什麼 length 規定 big-endian、文字規定 UTF-8，不可以直接 `send` 一個 C 的 `struct` |
| port 被占用（`Address already in use`） | 換一個 port，或監聽端設 `SO_REUSEADDR`；這也是 port 不能寫死的原因 |

報告中的兩台電腦量測，要寫明兩端的作業系統、網路種類（教室有線／Wi-Fi／手機熱點）與雙方 IP 的網段（例如 `192.168.43.x`）。

## 角色分工（本輪四人組 P+P+D+V、三人組 P+D+V）

- **P（口頭報告）**：評測當天報告。四人組兩位 P 一人講「架構與封包格式、Huffman 設計決策」，一人講「壓縮率分析、量測結果與報告各題的回答」。
- **D（展示整合）**：負責 `main`、命令列介面、把各模組接起來；評測當天現場操作兩台電腦展示三個功能。
- **V（測試驗證）**：負責 `tests/`、`make test`、壞輸入測試、量測腳本與原始數據。
- 角色是「評測當天誰負責什麼」，不是「只有那個人寫那部分」：**每個人都要有 C 程式的 commit**，個人口試會問你自己寫的部分，也會問隊友寫的部分。

## 建議時程

| 週 | 日期 | 目標 |
|---|---|---|
| 1 | 9/21–9/27 | 建 team repo；frame 收發（用 starter 的話就是 TODO 1、2；`send_all`／`recv_all` 殼裡已經有）；`send`／`recv` 的 `--raw` 檔案傳輸能逐 byte 還原；**兩台電腦連線成功**；定 `docs/interface.md` 初稿 |
| 2 | 9/28–10/4（9/28 停課） | 先離線完成 Huffman：`檔案 → 編碼 → 解碼 → 檔案` 逐 byte 相同，含邊界測試。先做 byte（最單純），再加 UTF-8 字元與 16-bit sample；聊天改走 frame |
| 3 | 10/5–10/11 | Huffman 接上傳輸路徑（`--huff`）；壞輸入測試；量測、畫圖、投影片；**10/11 18:00 前登錄 SHA** |

10/5 上課會講 Huffman 實作細節（建樹、位元打包）；第 2 週請先依 9/21 的概念與 MP4 規格動手，不要等到 10/5 才開始。

## starter/：會動的殼＋五個 place holder（建議從這裡開始）

[starter/](starter/) 是由 baseline 的 chat.c 改寫延伸、**已經符合本規格的命令列介面、跨機器連線、聊天畫面、
檔案傳輸流程與 `STATS` 輸出**的 C 程式；frame 標頭、UTF-8 檢查、Huffman 編解碼五個函式是 place holder，留給你們完成。
`make test` 會告訴你每個 place holder 是 `TODO`、`FAIL` 還是 `PASS`。用法與建議順序見 [starter/README.md](starter/README.md)。
把它複製到你們的 team repo 再改；也可以完全不用它、從 baseline 自己寫，只要符合上面的規格。

## python_ref/：純 Python 的完整版（對照用）

[python_ref/textlink.py](python_ref/textlink.py) 是功能完整的 TextLink（聊天、聊天中傳檔、`send`／`recv`、三種符號的 Huffman、`STATS`），
只用 Python 標準函式庫、刻意用高階的寫法，角色和 MP1–MP5 的 `samples_2025-python/` 一樣：**讓你先看到做完的樣子與每一步的中間結果**。

- `python3 python_ref/textlink.py inspect 檔案`：不用連線，直接列出 N、K、熵 H、平均碼長 L、codebook 大小、壓縮率，並對照「改以 byte 為符號」——報告的壓縮率分析表就是這些數字。
- 任何指令加 `--probe`：印出機率最高的符號與它們的 code、區塊各部分的大小、每一個 frame 的 hex。
- 它的 frame 與檔案傳輸格式和 starter 相同，**可以當你們 C 程式的對手**來測（`--raw` 一定互通；`--huff` 要你們採用相同的區塊格式才互通）。

你們要交的仍然是 C：Python 用的 `Counter`、`heapq`、字串切片在 C 裡都沒有，逐行翻譯行不通；要對照的是每一步的輸入與輸出。詳見 [python_ref/README.md](python_ref/README.md)。

## baseline/

`chat.c`＋`Makefile`：課程 TCP／UDP 聊天教學範例，作為傳輸部分的起點（socket 建立、收發執行緒、UTF-8 折行顯示）。
它**沒有**長度前綴、沒有 Huffman、沒有檔案傳輸，這三樣就是你們要做的。
長度前綴的引導見 [../../docs/tutorials/nettcpudp_homework.md](../../docs/tutorials/nettcpudp_homework.md) 作業 3；
半包與黏包的現象見[第 2 週講義第三節](../../lectures/wk02_0914_text-utf8/README.md)。
