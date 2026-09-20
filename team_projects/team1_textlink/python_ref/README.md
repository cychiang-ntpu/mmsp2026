# python_ref/：TextLink 的純 Python 完整版（對照用）

[textlink.py](textlink.py) 是一支**功能完整**的 TextLink：聊天、在聊天中傳檔、`send`／`recv`、三種符號的 Huffman、`STATS`，
命令列與[規格](../README.md)相同，只用 Python 標準函式庫，約 450 行。它的用途和 MP1–MP5 的 `samples_2025-python/` 一樣：
**讓你先看到「做完長什麼樣子」、每一步的中間結果是什麼，再去寫 C。**

> 你們要交的是 **C**。這支 Python 用的是 C 沒有的高階工具（`Counter`、`heapq`、字串切片、任意長度的整數），
> 逐行翻成 C 既不可行也不划算；要對照的是「每一步的輸入與輸出」，不是寫法。

## 三種用法

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 分析一個檔案（不用連線） | `python3 textlink.py inspect ../../../lectures/wk03_0921_team1-kickoff/data/speech_osr_8k.wav` | `python textlink.py inspect ..\..\..\lectures\wk03_0921_team1-kickoff\data\speech_osr_8k.wav` |
| 看中間過程 | 任何指令加 `--probe` | 同左 |
| 當你們 C 程式的對手 | `python3 textlink.py recv 5000 out`，再用你們的 `textlink send 127.0.0.1 5000 檔案` | `python textlink.py recv 5000 out` |

### 1. `inspect`：報告裡「壓縮率分析表」的每一格

```
$ python3 textlink.py inspect speech_osr_8k.wav
speech_osr_8k.wav：538,014 bytes；依副檔名，符號＝s16
符號              N       K        H        L     H×N÷8÷原始   codebook          區塊      壓縮率  還原
s16       268,985  12,343  11.7626  11.7862       73.51%     37,029     433,392   80.55%  逐 byte 相同
byte      538,014     256   6.6531   6.6683       83.16%        512     448,991   83.45%  逐 byte 相同
```

一次列出「照規定的符號」與「改以 byte 為符號」兩種：N（符號個數）、K（種類數）、熵 H、平均碼長 L（檢查 H ≤ L < H + 1）、
理論壓縮率、codebook 佔幾 bytes、整個區塊多大、實際壓縮率，並真的編碼再解碼一次確認逐 byte 相同。
上面這個例子就是規格第 4 題的答案的樣子：以 sample 為符號，下限從 83.2% 降到 73.5%，但 12,343 種符號的 codebook 吃掉 37 KB，實際只到 80.6%。
**你們的 C 程式對同一個檔案算出來的 N、K、H 應該和它一樣**；L 與區塊大小會因為平手規則與 codebook 格式不同而略有差異。

### 2. `--probe`：把中間過程印出來

```
$ python3 textlink.py --probe inspect short.txt
[probe] huff_encode：符號＝char，N=18，K=15，H=3.8366，L=3.8889 bits／符號
              U+591A '多'  次數        2  p=0.1111  理想  3.17 bits  code 0101
              U+5A92 '媒'  次數        2  p=0.1111  理想  3.17 bits  code 0110
              …
          區塊 90 bytes = 檔頭 21 + codebook 60 + 原樣保留 0 + bitstream 9；原始 54 bytes → 166.67%
```

傳輸時加 `--probe`，還會印出每一個 frame 的 type、長度與前 16 bytes 的 hex——拿來和你們的 C 程式送出的 bytes 逐一比對，是找 frame 層錯誤最快的方法。

### 3. 當對手：Python ↔ C 可以互連

frame、FILE_BEGIN／DATA／END 的格式與 [starter](../starter/) 的殼相同，所以只要你們沒有改殼的格式：

- 完成 TODO 1、2 之後：你們的 C `send --raw` → Python `recv`，檔案應該逐 byte 相同；反方向也是。
- 完成 Huffman 之後：如果你們**採用和它相同的區塊格式**（見程式裡 `huff_encode` 的說明），`--huff` 也能互通，
  等於有一個現成的、已知正確的對手可以測。
- 如果你們設計了自己的區塊格式（規格允許，而且 codebook 存得更省是加分方向），`--huff` 就不會互通，這是正常的；
  請把你們的格式寫進 `docs/interface.md`，並用 `--raw` 與它互測 frame 層。

聊天也可以互連：`python3 textlink.py chat server 5000`，你們的 C 程式 `textlink chat client 127.0.0.1 5000`。
Python 版的聊天畫面是一行一行印的（沒有泡泡），指令相同：`/files`、`/send`、`/raw`、`/huff`、`/quit`。

## 程式怎麼讀：七個區塊，由下層到上層

| 區塊 | 做什麼 | 對應到 C 的哪裡 |
|---|---|---|
| 1. 切符號 | bytes → 符號的 list：UTF-8 字元、16-bit sample（逐 chunk 找 WAV 的 data 區）、或 byte | `huffman.c` 的第 0 步 |
| 2. Huffman | `Counter` 統計 → `heapq` 建樹得到每個符號的長度 → 由長度指派 code → 串成位元 → 打包成區塊；解碼反過來，而且每個欄位先檢查再用 | `huffman.c`（TODO 4、5） |
| 3. frame | 4 bytes big-endian 長度＋1 byte type＋payload；`recv_exact` 迴圈收滿 n bytes | `frame.c`（TODO 1、2）、`net.c` 的 `recv_all` |
| 4. 連線 | 監聽（可 `--bind`）／連線（逾時 10 秒）、`TCP_NODELAY` | `net.c` |
| 5. 傳檔 | FILE_BEGIN → FILE_DATA × N → FILE_END → 等對方回報；先寫 `.part` 再改名；`STATS` | `transfer.c` |
| 6. 聊天 | 主執行緒讀鍵盤、另一條執行緒收訊息；收到的文字先驗證是合法 UTF-8、濾掉控制字元 | `chat.c`、`utf8.c`（TODO 3） |
| 7. inspect | 離線分析 | （C 版沒有；你們可以自己加，寫報告會很方便） |

幾個「Python 一行、C 要自己想」的地方，正是這個專題的功課：

| Python | 在 C 裡 |
|---|---|
| `data.decode("utf-8")` 嚴格解碼，非法就丟例外 | 自己看前導 byte、檢查續位元組、overlong、代理區、上限（第 2 週、TODO 3） |
| `collections.Counter(symbols)` | 以符號值為索引的陣列：byte 256 格、sample 65,536 格、字元 0x110000 格 |
| `heapq` 裡放 `(次數, 順序, [底下的符號])` | 節點陣列＋排序好的佇列或 heap；不要在節點裡放 list（第 3 週的 `huffman_trace.c`） |
| `"".join(codes[s] …)` 再 `int(bits, 2).to_bytes()` | bit writer：一個累加器，湊滿 8 bits 就吐出一個 byte |
| `bits[p:p+size] in table`，由短到長試 | bit reader 一次讀 1 bit，沿著樹走，或用 canonical code 的「每種長度第一個 code」來判斷 |
| 整數沒有上限、list 自動長大 | code 可能超過 32 bits；每一塊記憶體的大小都要自己算、自己檢查 |
| 1.2 MB 的中文文字：編碼約 0.08 秒、解碼約 0.3 秒 | C 大約是 2 ms 與 7 ms，快幾十倍——「比較速度」時請用你們的 C 程式量，不要用它 |

## 已知的差別

- 速度比 C 慢幾十倍，`STATS` 裡的 `encode_ms`／`decode_ms` 不能拿來和 C 比。
- 聊天畫面沒有泡泡與進度條；`--probe` 與 `inspect` 是它多出來的。
- 和 C 版一樣：檔名只保留 ASCII 安全字元、整個檔案一次讀進記憶體（上限 64 MiB）、`FILE_END` 沒有 checksum、聊天短訊息每則都帶 codebook（一定會變大）。
