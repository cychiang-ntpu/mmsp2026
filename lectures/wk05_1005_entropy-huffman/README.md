# 第 5 週（2026/10/5）｜位元打包、定長編碼、Huffman 的 C 實作、Team 1 收尾

對應：MP3、MP4、Team 1（10/11 18:00 登錄 SHA、10/12 評測）
時間：13:10–16:00，三節。上次上課是兩週前（9/28 停課）。

> **課前準備（同學）**：在 `mmsp2026` 資料夾打 `git pull` 拿到本週內容；第 3 週的 `examples` 要還能 `make`。
> 把第 3 週的回家作業（手算 `MISSISSIPPI RIVER` 的 Huffman）帶來，第一節一開始就對答案。
> 指令一樣分兩欄：左邊 macOS／Linux／WSL，右邊 Windows 的 VSCode PowerShell（`python3` 在 Windows 通常叫 `python`）。
> 本講義的指令都從本週資料夾出發：`cd lectures/wk05_1005_entropy-huffman`。
>
> **上課跟著打指令，請開 [commands.md（一頁版指令表）](commands.md)**。
>
> **這一週的位置**：第 3 週講了「為什麼」（熵、建樹、符號的定義），今天講「怎麼寫成 C」。
> 第 3 週的每一個觀念今天都會再出現一次，但這次旁邊放的是程式碼與 bytes。

## 本週目標

上完課你應該能：

1. 從回家作業接回來：說出為什麼同一張頻率表會做出不同的 code、但平均碼長 L 一定相同，以及這件事為什麼決定了 MP4 的自動測試「只比 bin 的大小、不比 codebook」。
2. 用紙筆把一串 code 塞進 bytes（高位先填、最後補 0），再讀回來；說出「解碼端怎麼知道該停」的三種做法（記符號數、加 EOF 符號、記有效位元數），以及 MP3／MP4 用哪一種、Team 1 用哪一種。
3. 寫出一個 bit writer 與 bit reader（一個累加器＋一個計數器，各十行以內），並說出「位元順序兩端不一致」是什麼樣的 bug。
4. 做出 MP3 的定長編碼：排序規則、7-bit code、`codebook.csv` 的每一欄、EOF；說出它和 Huffman **只差「code 怎麼指派」**，其餘的程式可以共用。
5. 說出 K 很大時的建樹做法（先排序一次＋兩個佇列）與 canonical Huffman 的指派規則，並解釋為什麼 Team 1 的 codebook 只要傳「符號＋長度」。
6. 用 `codebook_check.py` 檢查自己的 codebook（prefix code、Kraft、H ≤ L < H + 1、bin 大小），讀懂 CI 的每一條錯誤訊息；Team 1 剩一週：把 `--huff` 接上傳輸、量測、寫報告。

## 時程

| 節次 | 時間 | 內容 | 教材／範例 |
|---|---|---|---|
| 1 | 13:10–14:00 | **從作業到 bytes**：回家作業對答案；bit writer／reader；解碼端怎麼知道該停；MP3 的定長編碼逐欄看 | [examples/bitio_trace.c](examples/bitio_trace.c)、[data/mississippi_river.txt](data/mississippi_river.txt)、[flc_codec.py](../../samples_2025-python/mini_project_3/flc_codec.py) |
| 2 | 14:10–15:00 | **Huffman 的 C 實作**：大 K 的建樹（兩個佇列）；由長度指派 canonical code；解碼的三種寫法；MP4 規格與 CI 逐條 | [examples/huffman_build.c](examples/huffman_build.c)、[examples/codebook_check.py](examples/codebook_check.py)、[huffman_codec.py](../../samples_2025-python/mini_project_4/huffman_codec.py) |
| 3 | 15:10–16:00 | **Team 1 收尾**：codebook 的二進位格式寫進 `interface.md`；壞輸入；`--huff` 接上傳輸；量測與報告的表；10/11 登錄、10/12 評測怎麼跑 | [Team 1 規格](../../team_projects/team1_textlink/README.md)、[starter/src/huffman.c](../../team_projects/team1_textlink/starter/src/huffman.c)、[python_ref/textlink.py](../../team_projects/team1_textlink/python_ref/textlink.py) |

先把本週的範例編譯起來：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd examples` | `cd examples` |
| `make` | `mingw32-make` |

會產生 `bitio_trace` 與 `huffman_build` 兩支程式（Windows 多 `.exe`）。

---

## 第一節｜從作業到 bytes：位元打包與定長編碼

### 1.1 回家作業對答案：code 可以不同，L 不能不同（8 分鐘）

`MISSISSIPPI RIVER`（含空白共 17 個字元）的頻率表：I 5、S 4、P 2、R 2、M 1、E 1、V 1、空白 1。用第 3 週的程式對答案：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `../../wk03_0921_team1-kickoff/examples/huffman_trace "MISSISSIPPI RIVER"` | `..\..\wk03_0921_team1-kickoff\examples\huffman_trace.exe "MISSISSIPPI RIVER"` |

```
熵 H = 2.6987 bits／符號        平均碼長 L = 46 ÷ 17 = 2.7059        H ≤ L < H + 1 成立，效率 99.7%
長度：I 2、S 2、P 3、R 3、M 4、E 4、V 4、空白 4       （定長編碼要 3 bits → 51 bits；Huffman 46 bits）
```

你算出來的 code 很可能和它不一樣：次數相同的符號（M、E、V、空白都是 1；P、R 都是 2）先取誰、左右怎麼擺，
都會改變每個符號拿到的 code，甚至改變個別符號的長度。但**不管怎麼平手，Σ 次數 × 長度 = 46 bits 一定相同**——
Huffman 證明的是「平均碼長最短」，不是「這一組 code」。把這件事記住，它今天會出現三次：

- **MP4 的自動測試**不比 `codebook.csv` 的內容（比不得），只比 (1) `encoded.bin` 的大小和 Python 版相同、(2) 用 Python 的 decoder 解得開你的 bin＋codebook、(3) 你的 decoder 解得開 Python 的 bin＋codebook（2.4）。
- **Team 1** 的接收端拿不到你的樹，只拿得到 codebook；所以 codebook 怎麼存、兩端怎麼重建同一張表，是今天第二節的主題（2.2）。
- **MP3** 反過來：定長編碼沒有平手問題，所以 CI 要求 `codebook.csv` 與 `encoded.bin` 和 Python 版**逐 byte 相同**（1.4）。

### 1.2 從 code 到 bytes：bit writer 與 bit reader（15 分鐘）

第 3 週的 ABRACADABRA（A=`0`、B=`110`、C=`100`、D=`101`、R=`111`）編碼後是這一串：

```
0 110 111 0 100 0 101 0 110 111 0        共 23 bits
```

檔案與網路只認 byte。把它切成 8 個一組、最後不滿 8 個就**補 0**：

```
01101110  10001010  1101110[0]            3 bytes：6E 8A DC   （[0] 是補位）
```

這就是「位元打包」。約定：**一個 byte 裡先填最高位（MSB first）**，和 Python 參考實作 `int(bits, 2).to_bytes(…, "big")` 相同。
（DEFLATE／ZIP 是反過來先填最低位。哪一種都可以，但**兩端必須一致**：寫錯順序時 bin 的大小完全正確、每一個 byte 都是錯的，
這是 MP3、MP4 與 Team 1 最常見的 bug，`codebook_check.py --bits` 看得出來，1.4。）

C 裡沒有「位元串」這種東西，要自己寫一個 **bit writer**：一個累加器 `acc`＋一個計數器 `nbits`，三個動作：

```c
/* 放進一個 code（數值 code、長度 len）：接到 acc 的尾巴，湊滿 8 個就吐出最高的 8 bits */
acc = (acc << len) | code;  nbits += len;
while (nbits >= 8) { out[n_out++] = (uint8_t)(acc >> (nbits - 8)); nbits -= 8; acc &= (1ULL << nbits) - 1; }
/* 結束：剩 1–7 個 bit 就左移補 0 湊成最後一個 byte */
if (nbits) out[n_out++] = (uint8_t)(acc << (8 - nbits));
```

`acc` 用 `uint64_t`：裡面最多留 7 個 bit，再放進一個最長 56 bits 的 code 也不會溢位（Team 1 的 python_ref 把 code 長度上限訂為 56，就是為了這個）。
**bit reader** 更短：`cur` 是目前這個 byte、`bit` 是讀到第幾位，每次回傳 `(cur >> (7 - bit)) & 1`，讀完 8 個就載入下一個 byte；
**資料用完時要回傳 −1**，呼叫端一定要檢查——「不可以讀超過 `in[in_len-1]`」就是從這裡做起。

看它動起來：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `./bitio_trace` | `.\bitio_trace.exe` |

```
  符號 code     acc        nbits  輸出的 bytes
  A      0        0          1
  B      110      0110       4
  R      111      0110111    7
  A      0        (空)       0      → 湊滿 8 bits，輸出 6E
  C      100      100        3
  …
  flush：acc 剩 7 bits，補 1 個 0 → 最後一個 byte DC
  結果：11 個符號、23 bits → 3 bytes：6E 8A DC
```

下半段用 bit reader 一個 bit 一個 bit 讀回來，**從根出發：0 往左、1 往右，走到葉就輸出一個符號、回到根**（這就是解碼的第一種寫法，2.3）。
自己換一組 code 試：`./bitio_trace "多媒體多媒多" --codes 多:0,媒:10,體:11`（給的不是 prefix code 時它會拒絕）。

### 1.3 解碼端怎麼知道該停（7 分鐘）

把上面的程式加上 `--no-stop`：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `./bitio_trace --no-stop` | `.\bitio_trace.exe --no-stop` |

```
  0→A 110→B 111→R 0→A 100→C 0→A 101→D 0→A 110→B 111→R 0→A 0→A
  解出 12 個符號
  ★ 多解出 1 個：補位的 0 剛好是某個符號的 code。
```

補位的那個 `0` 被解成了第 12 個 A。bit reader 自己分不出「資料」和「補位」，**解碼端一定要另外知道該停在哪裡**。三種做法：

| 做法 | 多付出什麼 | 誰在用 |
|---|---|---|
| (a) 區塊裡記「共有 N 個符號」 | 固定幾個 bytes（N 用 8 bytes 可到 1.8 × 10¹⁹） | Team 1 python_ref（檔頭的 `n_syms`）；也可以改記原始 bytes 數 |
| (b) 多一個 **EOF 符號**，次數算 1，編進 codebook，最後寫它一次 | codebook 多一列；它和真的符號搶 code，可能把別人擠長 1 bit | **MP3、MP4**（規格規定；`"EOF"` 那一列） |
| (c) 記有效位元的總數（或最後一個 byte 用了幾 bits） | 固定幾個 bytes | DEFLATE 用類似的做法 |

(b) 的好處是串流式——不用先知道全長就能解；代價在 1.4 會看到。不論哪一種，**停止條件來自區塊裡記載的數字，不是「bitstream 讀完了」**。
壞資料也從這裡防：宣稱的 N 比 bitstream 可能裝的還多、或 bitstream 讀完了 N 還沒到，都要回報錯誤（Team 1 的 `huff_decode`，3.2）。

### 1.4 MP3：定長編碼（FLC）逐欄看（15 分鐘）

Huffman 是「常見的短、少見的長」；**定長編碼（fixed-length coding）是「每個符號都一樣長」**：K 種符號只要 ⌈log2 K⌉ bits。
它是 1.3 例 1 的那條底線——機率平均時 Huffman 和它一樣；機率不平均時它比 Huffman 差，但實作最單純，所以 MP3 先做它，把 1.2 的 bit writer、1.3 的 EOF、
CSV 的格式、Makefile 與 CI 全部先打通，MP4 再換上 Huffman。

MP3 的規定（[作業規定](https://hackmd.io/@ntpu-ce-mmsp/mmsp-2025-mini-project-3)；行為以 [flc_codec.py](../../samples_2025-python/mini_project_3/flc_codec.py) 為準）：符號 = UTF-8 字元，**code 固定 7 bits**（最多 128 種含 EOF），
依出現次數**由少到多**排、次數相同時依 UTF-8 的 bytes 長度再依 bytes 值；依序給 0、1、2、…；EOF 排最後。對回家作業的字串跑一次：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../../../samples_2025-python/mini_project_3/flc_codec.py encode ../data/mississippi_river.txt mr3.csv mr3.bin` | `python ..\..\..\samples_2025-python\mini_project_3\flc_codec.py encode ..\data\mississippi_river.txt mr3.csv mr3.bin` |
| `cat mr3.csv` | `type mr3.csv` |

```
" ",1,0.0588235,0000000        ← 欄位：符號（雙引號包起來）、次數、機率（小數 7 位）、7-bit code（沒有引號）
"E",1,0.0588235,0000001
"M",1,0.0588235,0000010
"V",1,0.0588235,0000011
"P",2,0.1176471,0000100
"R",2,0.1176471,0000101
"S",4,0.2352941,0000110
"I",5,0.2941176,0000111
"EOF",0,0.0000000,0001000      ← EOF 次數記 0、機率 0，但 bin 的最後要寫它一次
```

`mr3.bin` 是 18 個 code × 7 bits = 126 bits → **16 bytes**；原文 17 bytes，壓縮率 94%。看起來沒省多少，因為英文一個字元本來就只有 1 byte（8 → 7 bits）。
換成第 2 週的中英文混合檔 `sample_zh_en.txt`（162 bytes）：FLC 後 **93 bytes（57%）**——中文字 3 bytes 變 7 bits，**這是「符號定義」的功勞，不是演算法的**（第 3 週 1.7）；
同一個檔案用 Huffman 是 76 bytes（47%），兩者的差才是「code 怎麼指派」的功勞。

**自動測試怎麼測 MP3**（[tools/ci/run_tests.sh](../../tools/ci/run_tests.sh) 的 `test_mp3`，和老師評分用的是同一支）：

1. `./mp3 encode in.txt codebook.csv encoded.bin` → `codebook.csv` 與 `encoded.bin` 都要和 Python 版**逐 byte 相同**。所以排序規則、機率的小數位數、跳脫寫法（`"\n"`、`"\t"`、`"\r"`、`"` 寫成 `""`）、
   換行一律 `\n`（Windows 上要用 `"wb"` 開檔，不然會多出 `\r`）、全部照 Python 版。
2. `./mp3 decode encoded.bin codebook.csv out.txt` → 和原文逐 byte 相同（含 `\r\n`；開頭的 BOM 不算內容，encode 時跳過，見第 2 週 1.5）。
3. 執行檔要叫 `mp3`、放在個人 repo 的 `mp3/`，有 Makefile 就 `make mp3`，沒有就 `gcc -Wall -Wextra -std=c99 -o mp3 *.c -lm`（[hw-template](../../tools/ci/hw-template/mp3/README.md)）。

去年的評語（[樣本](../../samples_2025-C/mini_project_3/README.md)）幾乎都不是演算法：Makefile 的 recipe 開頭不是 Tab（`missing separator`）、Makefile 沒指定正確的 `.c`、decoder 沒做完、「沒有達到壓縮的效果」（bin 和原文一樣大：每個符號寫成了 1 byte，沒有真的打包）。
寫到一半先用 [codebook_check.py](examples/codebook_check.py) 自我檢查（2.5）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 codebook_check.py mr3.csv mr3.bin --bits` | `python codebook_check.py mr3.csv mr3.bin --bits` |

它會把 bin 的位元流依 codebook 切開、標出 EOF 與補位——bit order 反了、忘了寫 EOF、多寫了一個 byte，都一眼看得到。

### 1.5 從 FLC 到 Huffman：同一條管線，只換一步（5 分鐘）

```
讀檔（跳 BOM）→ 切成 UTF-8 字元 → 統計次數 → [ 指派 code ] → 寫 codebook.csv → bit writer 逐符號寫 code → 寫 EOF → flush → encoded.bin
                                          FLC：排序後依序給 0、1、2、…（1.4）
                                          Huffman：建樹、由樹讀出 code（第 3 週 1.6、今天 2.1–2.2）
```

MP3 與 MP4 的 encoder 只有方括號那一步不同；decoder 也只差「查表的方式」（FLC 每 7 bits 查一次；Huffman 逐 bit 走樹，2.3）。
**把共用的部分寫成函式**（讀檔、切字元、統計、CSV 的跳脫、bit writer／reader），MP4 就只剩建樹；Team 1 再加上「符號 = sample／byte」兩種切法，中間全部共用。
這也是 starter 的 `huffman.c` 註解裡「三種符號只差怎麼切出符號、怎麼寫回 bytes」的意思。

---

## 第二節｜Huffman 的 C 實作

### 2.1 K 很大時怎麼建樹：先排序一次，再用兩個佇列（12 分鐘）

第 3 週的 `huffman_trace.c` 用一個排序好的佇列：每合併一次就把新節點**插回該在的位置**，一次最多搬整個佇列，K 種符號約 K² 次。
K = 8 無所謂；Team 1 的 WAV 以 16-bit sample 為符號，那個真實語音檔有 **K = 12,343** 種（`python_ref inspect` 印的數字），1.5 億次搬移；
最多 65,536 種就是 43 億次。要換做法。

兩個佇列的做法（[huffman_build.c](examples/huffman_build.c)）：

1. 葉節點先依次數**排序一次**（`qsort`，O(K log K)）→ 佇列 **Q1**。
2. 合併出來的新節點依序放到另一個佇列 **Q2** 的尾巴，**不排序**。關鍵觀察：每一輪的新節點是「目前最小的兩個相加」，
   而目前最小的兩個不會比上一輪最小的兩個小，所以**新節點的次數只會越來越大（或相同）**，Q2 自然就是由小到大。
3. 每一輪「最小的兩個」只可能在 **Q1 的頭或 Q2 的頭**：比較兩個頭、取小的那個，做兩次。
   每輪 O(1)，整個建樹 O(K)；加上一開始的排序，O(K log K)。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `./huffman_build` | `.\huffman_build.exe` |
| `make trace`（8 色影像） | `mingw32-make trace` |

```
步驟 1｜葉依 weight 排序一次（qsort）→ Q1；Q2 一開始是空的
  Q1：[C:1] [D:1] [B:2] [R:2] [A:5]      Q2：(空)
步驟 2｜每一輪：比較 Q1 的頭與 Q2 的頭，取小的那個，做兩次 → 合併 → 放到 Q2 的尾巴
  第 1 輪 C:1 + D:1 → #5:2         Q1：[B:2] [R:2] [A:5]    Q2：[#5:2]
  第 2 輪 B:2 + R:2 → #6:4         Q1：[A:5]                Q2：[#5:2] [#6:4]
  第 3 輪 #5:2 + #6:4 → #7:6       Q1：[A:5]                Q2：[#7:6]
  第 4 輪 A:5 + #7:6 → #8:11       Q1：(空)                 Q2：[#8:11]
```

節點的資料結構和第 3 週一樣：一個 `Node` 陣列（前 K 個是葉），`parent`／`left`／`right` 是陣列索引，不用 `malloc` 單一節點、不用指標。
8 色影像跑出來 L 仍是 536 ÷ 256 = 2.0938——換了做法、平手規則也不同（yellow 與 blue 的長度對調了），**L 不變**（1.1）。
另一個常見做法是 binary heap（取最小 O(log K)），`huffman_demo.py` 的 `heapq` 就是；C 裡要自己寫 push／pop 各十幾行，兩個佇列的版本更短、也不容易寫錯。

兩個數量要用對型別：次數加總（根的 weight）是 N，64 MiB 的檔案以 byte 為符號 N 就超過 `int`；用 `uint64_t`。
統計表以符號值為索引：byte 256 格、sample 65,536 格、字元 0x110000 格（`calloc`，不放 stack）——第 3 週 `entropy.c` 的做法。

### 2.2 只留長度：canonical Huffman（10 分鐘）

樹建好之後，`huffman_build` 不是沿著樹走出 code，而是只記每個葉的**深度（= code 長度）**，然後用一條公開的規則重新指派 code：

```
依（長度，符號值）排序；第一個 code = 0；下一個 = 上一個 + 1；長度變長 d 位時，先左移 d 位再 + 1。
```

8 色影像（長度 1、2、3、4、6、6、6、6）：

```
white  1  0            yellow 4  1110
black  2  10           orange 6  111100
blue   3  110          red    6  111101
                       purple 6  111110
                       green  6  111111
```

這叫 **canonical Huffman code**（[[18]](#ref-18)）。它仍然是 prefix code、每個符號的長度和原來一樣，所以 L 一樣；多出來的好處是：

- **codebook 只要傳「符號＋長度」**，不用傳 code：兩端照同一條規則重建就得到同一張表。Team 1 以 sample 為符號、K = 12,343 時，
  每個符號 2 bytes＋長度 1 byte，codebook 約 37 KB；若把 code 字串也存進去，會多一倍以上。
  這就是 starter `huffman.c` 註解裡的做法 (b)，也是 python_ref 用的格式（符號依序遞增、每組「符號＋長度」）；DEFLATE（ZIP、PNG）也是這樣傳 Huffman 表的（第 3 週參考 [17] 的 RFC 1951，3.2.2 節）。
- 解碼可以不走樹：同一長度的 code 是連號的，記下「每個長度的第一個 code 與符號數」就能查（2.3 的第二種寫法）。
- 第 3 週的 `huffman_demo.py` 第 3 段印的 code 其實就是 canonical 的——那時說「black 是 10 而不是 11，但長度相同」，現在知道為什麼了。

MP4 的 `codebook.csv` 要印出 code 字串，所以用樹走出來的或 canonical 的都可以；**Team 1 的 codebook 用二進位傳，請用 canonical**。

### 2.3 解碼的三種寫法（10 分鐘）

| 寫法 | 怎麼做 | 什麼時候用 |
|---|---|---|
| (1) 逐 bit 走樹 | 從根出發，0 往左、1 往右，到葉就輸出、回到根（`bitio_trace` 的做法）。樹可以由 codebook 的 code 字串「種」出來：每個 code 從根往下走，沒有的分支就新增節點 | **MP4**：codebook 給的是 code 字串，這樣最直接；K 小的時候最好寫 |
| (2) canonical 查表 | 逐 bit 累積成數值 `v` 與長度 `l`；每多 1 bit 檢查 `v` 是否落在長度 `l` 的區間 `[first[l], first[l] + count[l])`，是就查出符號 | **Team 1**：只有長度沒有 code 時，不必真的建樹；記憶體只要每個長度兩個數字 |
| (3) 查表一次讀多個 bit | 先看接下來的 8–16 bits，查一張 2⁸–2¹⁶ 格的表直接得到「符號＋長度」 | JPEG、ZIP 的解碼器；本課程不要求 |

不論哪一種，**停止條件來自區塊裡的數字**（1.3）：MP4 解到 EOF 就停；Team 1 解到第 N 個符號就停。
兩個一定要防的壞輸入：走到不存在的分支（code 字串 ≠ 位元流，或 bit order 反了）、資料用完了符號數還沒到。
它們不是「不會發生」——Team 1 評測第 6 項就是故意改壞 codebook 丟給你（3.2）。

### 2.4 MP4 的規格與自動測試逐條（10 分鐘）

[作業規定](https://hackmd.io/@ntpu-ce-mmsp/mmsp-2025-mini-project-4)；行為以 [huffman_codec.py](../../samples_2025-python/mini_project_4/huffman_codec.py) 為準。對回家作業的字串跑一次：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../../../samples_2025-python/mini_project_4/huffman_codec.py encode ../data/mississippi_river.txt mr4.csv mr4.bin` | `python ..\..\..\samples_2025-python\mini_project_4\huffman_codec.py encode ..\data\mississippi_river.txt mr4.csv mr4.bin` |
| `cat mr4.csv` | `type mr4.csv` |

```
"S",4,0.222222222222222,"00",2.169925001442313     ← 欄位：符號、次數、機率（15 位）、code（有引號）、資訊量 log2(1/p)（15 位）
"I",5,0.277777777777778,"10",1.847996906554950
"R",2,0.111111111111111,"010",3.169925001442313
" ",1,0.055555555555556,"0110",4.169925001442312
"EOF",1,0.055555555555556,"0111",4.169925001442312  ← EOF 次數算 1，和真的符號一起建樹
…
entropy = 2.8583 bits/symbol, huffman average = 2.8889
```

列依（長度，code）排序。N = 17 + 1（EOF）= 18、K = 9；bin 是 52 bits → **7 bytes**（原文 17 bytes，41%；但 codebook 有 9 列，整包還是比原文大——第 3 週 1.7 的 codebook 成本）。
注意 EOF 的代價：沒有 EOF 時 L = 2.7059（1.1），加了 EOF 變 2.8889。

**自動測試怎麼測 MP4**（`run_tests.sh` 的 `test_mp4`）：

| # | 測什麼 | 為什麼可以這樣測 |
|---|---|---|
| 1 | 你的 `encoded.bin` 大小 = Python 版的大小 | 平手規則不同 code 會不同，但 Σ 次數 × 長度一定相同（1.1）→ 總 bits 相同 → 補位後的 bytes 相同 |
| 2 | **Python 的 decoder** 解你的 bin＋你的 codebook → 等於原文 | 你的 codebook 必須是一張合法的 prefix code、格式要讓 Python 讀得懂（符號欄的跳脫、code 有引號、`"EOF"`） |
| 3 | **你的 decoder** 解 Python 的 bin＋Python 的 codebook → 等於原文 | 你的 decoder 不可以依賴「自己 encoder 的習慣」：要照 CSV 裡的 code 解，不能自己重建樹再假設 code 相同 |

第 2、3 項合起來就是「交叉解碼」：**encode 與 decode 之間只能靠 codebook.csv 溝通**，不能靠全域變數、不能靠「我知道我的樹長什麼樣」。
Team 1 也一樣——encode 與 decode 在兩台不同的電腦上。

常見的 bug 清單（按去年與本學期 Slack 上出現的頻率）：bit order 反了（bin 大小對、內容全錯）；忘了寫 EOF 或 EOF 沒算進建樹；
Windows 用 `"w"` 開 CSV 多出 `\r`；BOM 當成一個字元統計；只有一種符號時 code 長度給了 0；最後一個 byte 的補位被解成符號（1.3）；
次數相同時排序不穩定導致每次執行結果不同（不算錯，但很難 debug——加一個固定的平手規則）。

### 2.5 寫到一半先檢查：codebook_check.py（5 分鐘）

[examples/codebook_check.py](examples/codebook_check.py) 讀一張 MP3 或 MP4 格式的 `codebook.csv`（自動分辨），有 bin 就一起看：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 codebook_check.py mr4.csv mr4.bin --bits` | `python codebook_check.py mr4.csv mr4.bin --bits` |

```
mr4.csv：MP4 Huffman 格式，K = 9 種符號（含 EOF），N = 18 個
  Kraft Σ 2^(−len) = 1.000000；H = 2.8583 bits／符號；L = 2.8889；位元流 52 bits → 預期 bin 7 bytes
  mr4.bin：7 bytes，解出 17 個符號後遇到 EOF，之後補位 4 bits
  位元流（| 是 code 的邊界）：
    1101→M | 10→I | 00→S | 00→S | 10→I | … | 010→R | 0111→EOF | 0000（補位）
  ✅ codebook 沒有問題
```

它檢查的每一條都對應今天的一個觀念：欄位格式（1.4、2.4）、沒有重複的符號與 code、**prefix code**（第 3 週 1.4）、
**Kraft 不等式 Σ 2^(−len) ≤ 1**（[[5]](#ref-5) 第 5 章：這組長度做得出 prefix code 的充要條件；Huffman 的樹是滿的，應該剛好 = 1）、
**H ≤ L < H + 1**（第 3 週 1.6；不成立就是建樹或次數欄有錯）、預期的 bin 大小、bit order、EOF、補位。
它不是 decoder，也不會幫你寫 MP4；它是把 CI 的 ❌ 翻譯成「哪一個觀念錯了」。

`make check` 做的就是「C 實作、Python 對答案」：`huffman_build --file --eof --summary` 自己建樹算出 N、K、H、L、總 bits，
和 `codebook_check.py --summary` 從 Python 版的 codebook 算出來的那一行逐 byte 比對——兩邊的樹不同、code 不同，**數字必須相同**。

---

## 第三節｜Team 1 收尾：剩一週

本週是 Team 1 規格「建議時程」的第 3 週：**Huffman 接上傳輸路徑（`--huff`）、壞輸入測試、量測、畫圖、投影片，10/11（日）18:00 前登錄 SHA，10/12 評測。**
先確認上一週的目標：`make test` 離線的 Huffman round-trip（三種符號）是不是都 PASS 了？沒有的話，今天前兩節的東西就是你們缺的。

### 3.1 把區塊格式寫進 interface.md（10 分鐘）

`huff_encode` 產生的那一塊資料要「自己帶 codebook、可以獨立解碼」（starter `huffman.c` 的註解）。今天學的東西剛好填滿每一格——以 python_ref 的格式為例（你們可以不同，但每一欄都要寫進 `docs/interface.md`）：

| 欄位 | 大小 | 內容 | 今天的哪一節 |
|---|---|---|---|
| 符號種類 | 1 byte | 0 byte／1 char／2 s16 | 第 3 週 1.7 |
| 原始長度 | 8 bytes，big-endian | 解碼端拿來和 `max_out` 比，也是停止條件的另一種 | 1.3 (a) |
| 符號個數 N | 8 bytes | **解碼的停止條件**：解到第 N 個就停 | 1.3 (a) |
| 符號種類數 K | 4 bytes | 後面 codebook 有幾組 | 2.1 |
| codebook | K 組「符號＋長度」 | 符號：byte 1、char 4、s16 2 bytes；長度 1 byte；**不存 code，兩端用 canonical 規則重建** | 2.2 |
| （s16）保留的 bytes | 4＋n＋4＋m | data 區前後不是 sample 的 bytes 原樣帶著 | 第 3 週 2.6 |
| bitstream | ⌈Σ 次數 × 長度 ÷ 8⌉ bytes | bit writer 的輸出，MSB first，最後補 0 | 1.2 |

K = 0（空輸入）、K = 1（只有一種符號、長度 1）、長度超過 56、符號值不合法（char 超過 U+10FFFF、重複）都要在格式裡有明確的答案。
**encode 和 decode 常常是不同人寫的**：先在紙上把這張表畫好、兩個人各自照表寫，才接得起來。

### 3.2 壞輸入：把 in 當成陌生人填的表單（10 分鐘）

`huff_decode` 的每一步都先問兩個問題：「剩下的 bytes 夠不夠讀這個欄位？」「這個數字合理嗎？」（starter TODO 5 的清單）。對照今天的內容：

- 宣稱的原始長度 > `max_out`、宣稱的 K 大到 codebook 放不進 `in_len`、長度 0 或 > 56、符號重複 → `TL_ERR_DATA`，**先檢查再 `malloc`**。
- codebook 的長度做不出 prefix code（Kraft > 1）→ `TL_ERR_DATA`：canonical 重建時 code 會「溢出」該長度的位數，那一刻就能發現。
- bitstream 讀完了 N 還沒到；或走到不存在的分支 → `TL_ERR_DATA`（2.3）。
- 全部通過之後，解出來的 bytes 數要等於宣稱的原始長度。

`make test` 對每個通過的 round-trip 都會再做兩件事：把區塊截成一半再解、`max_out` 給「原始長度 − 1」再解，都必須回傳錯誤。
V 角色請從第 3 週的 `chunk_send.py` 延伸：**改掉區塊的第一個 byte、中間換一個 byte、只留 1 個 byte、長度欄填 `FF FF FF FF`**，程式可以回報錯誤，不可以當掉、不可以卡住。
評測第 6 項「codebook 被改壞」就是這些。

### 3.3 接上 `--huff`、量測、報告的表（12 分鐘）

接線很短：`transfer.c` 已經把流程寫好——`huff_encode(整個檔案, 依副檔名決定 sym)`；回傳 `TL_ERR_DATA` 就改用 `SYM_BYTE` 再呼叫一次（內容不是 UTF-8 的 `.txt`、不是 16-bit PCM 的 `.wav` 自動退回）；
輸出切成多個 `FILE_DATA` frame；對方收齊後 `huff_decode(整塊, max_out = 宣稱的原始大小)`。**今天就接，不要等全部做完**：先用 byte 符號在兩台電腦之間傳一個 WAV 逐 byte 相同，再換 s16。

報告的「壓縮率分析表」每一欄今天都有工具可以算：

| 欄位 | 從哪裡來 |
|---|---|
| N、K、H、L | 你們的 C 程式印出來（`huffman_build --summary` 的那一行就是範本）；用 `python_ref inspect` 對答案（語音檔：s16 符號 K = 12,343） |
| 理論壓縮率 H × N ÷ 8 ÷ 原始 bytes、純編碼壓縮率 L × N ÷ 8 ÷ 原始 bytes | 不含 codebook |
| codebook bytes | 3.1 的格式算得出來：K × （符號 bytes ＋ 1） |
| 實際壓縮率（`huff_encode` 輸出 ÷ 原始）、傳輸壓縮率（`wire_bytes ÷ file_bytes`） | `STATS` 的 `ratio`；再含 frame 標頭 |
| 對照：以 byte 為符號 | 同一個檔案再跑一次 `SYM_BYTE`，只要算不必傳 |

報告要回答的四題（規格「壓縮率」一節）與量測要求（4 個檔案各 ≥ 1 MB、`--raw`／`--huff` 各 5 次取中位數、`127.0.0.1` 與兩台電腦、損益平衡頻寬）不再重複；
提醒一件事：**在 `127.0.0.1` 上 `--huff` 比 `--raw` 慢是正常的**，編、解碼的 CPU 時間就是淨損失；照實報告，算出網路慢到多少 Mbps 以下壓縮才划算。

### 3.4 10/11 登錄、10/12 評測怎麼跑（10 分鐘）

- **10/11（日）18:00**：登錄 repo URL ＋ 完整 commit SHA（不接受可移動的 tag 或 ZIP）。repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、`docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`（[course_plan](../../docs/course_plan.md)）。
  `CONTRIBUTIONS.md` 要寫團隊程式沿用了誰的 MP4；**MP4 本身仍須各自獨立完成**。
- **10/12 三節課內**，每組約 12 分鐘：P 報告（架構與封包格式、Huffman 設計決策；壓縮率分析與量測）→ D 用兩台電腦現場跑規格「最低驗收」的 0–8 項，
  port 與哪一台當監聽端由老師當場指定，檔案用沒公布過的（含 BOM／CRLF／1–4 bytes 字元的文字、WAV、空檔、單一符號檔）→ V 展示 `make test` 與壞輸入測試 → 個人口試。
- 口試會問你自己寫的部分，也會問隊友寫的部分。今天的內容就是題庫：bit writer 的 `acc` 為什麼要 `& mask`、補位的 0 為什麼不會被解成符號、
  兩個佇列為什麼不用排序、canonical 為什麼只要傳長度、`huff_decode` 收到 K = 2,000,000 時第一件事做什麼。
- 評測前一天才發現兩台電腦連不上是去年最多組出事的地方：**今天回去就在兩台電腦之間傳一次 `--huff`**（校園 Wi-Fi 會隔離裝置，改手機熱點；WSL 當連線端）。

### 3.5 收尾（3 分鐘）

- 個人作業：**MP1–MP5 統一截止 10/23（五）18:00**。MP3 今天的內容就夠寫完；MP4 的 encoder 這週做、decoder 下週；MP5 10/19 講。
- 下週 10/12：Team 1 評測，三節全用；10/19：Team 2 開題（VoiceLink：即時語音）、聲音 A/D 複習與 STFT、MP5 講解、補驗時段 1。
- 期中考 11/2 第一節，範圍到第 7 週；今天的 1.2–1.4、2.1–2.2 都在範圍內（手算題：打包、canonical、Kraft）。

## 回家作業／下週前要做的事

1. **手算**：把 1.1 的 `MISSISSIPPI RIVER` 用你自己的 code 打包成 bytes（寫出十六進位），再手動解回前 5 個符號；用 `bitio_trace "MISSISSIPPI RIVER" --codes …` 對答案。
   再用 canonical 規則對同一組長度重新指派 code，確認 L 不變。
2. **MP3 做完**：個人 repo 的 `mp3/` push 上去，`bash ../mmsp2026/tools/ci/run_tests.sh mp3` 全綠。
3. **MP4 的 encoder**：`codebook_check.py codebook.csv encoded.bin --bits` 沒有 ❌；能的話連 decoder 一起，`run_tests.sh mp4` 全綠。
4. **Team 1**（10/11 18:00 登錄 SHA）：`docs/interface.md` 有 3.1 那張表；`make test` 全部 PASS（目標 82 個）；兩台電腦之間 `--huff` 傳一個 WAV 逐 byte 相同；量測的 CSV 與三張圖；`slides.pdf`。
5. `TEAM_LOG.md` 記下這一週的討論與分工；每個人都要有 C 程式的 commit。

## 範例程式（examples/、data/）

| 檔案 | 用途 | 執行 |
|---|---|---|
| [commands.md](commands.md) | **上課跟著打的指令（一頁版）**：依上課順序、兩種作業系統各一欄 | 上課時開著 |
| [bitio_trace.c](examples/bitio_trace.c) | bit writer 把 code 塞進 byte 的每一步（acc、nbits、吐出的 byte）、補位；bit reader 逐 bit 走樹讀回來；`--no-stop` 看補位的 0 被解成符號 | `./bitio_trace`、`./bitio_trace --no-stop`、`./bitio_trace "字串" --codes A:0,B:10,…` |
| [huffman_build.c](examples/huffman_build.c) | K 很大時的建樹（先排序一次＋兩個佇列，逐輪印出 Q1、Q2）、只留長度、canonical code、H／L／Kraft；`--file` 讀整個檔案、`--eof` 加 EOF 符號、`--summary` 一行數字 | `make trace`、`./huffman_build "字串"`、`./huffman_build --file 檔案 --eof` |
| [codebook_check.py](examples/codebook_check.py) | 檢查 MP3／MP4 的 `codebook.csv`（格式、prefix code、Kraft、H ≤ L < H + 1、預期 bin 大小），有 bin 就依 codebook 解一遍、`--bits` 印出位元流與補位 | `python3 codebook_check.py codebook.csv encoded.bin --bits` |
| [Makefile](examples/Makefile) | `make`、`make trace`、`make check`（C 的 huffman_build 與 Python 參考實作＋codebook_check 的 N、K、H、L、bits 逐 byte 相同）、`make clean` | — |
| [data/mississippi_river.txt](data/mississippi_river.txt) | 第 3 週回家作業的字串（17 bytes，沒有換行），給 MP3／MP4 的參考實作與 `make check` 用 | — |
| [flc_codec.py](../../samples_2025-python/mini_project_3/flc_codec.py)、[huffman_codec.py](../../samples_2025-python/mini_project_4/huffman_codec.py) | MP3、MP4 的 Python 參考實作（對答案用；CSV 格式以它為準） | `python3 … encode in.txt codebook.csv encoded.bin` |
| [run_tests.sh](../../tools/ci/run_tests.sh) | MP1–MP5 的自動測試（本機與 GitHub Actions 同一支） | `bash ../mmsp2026/tools/ci/run_tests.sh mp3 mp4` |

Python 的示範程式只用標準函式庫，不需要安裝任何套件。第 3 週的 `huffman_trace`、`entropy`、`huffman_steps.html` 今天仍會用到。

## 參考

編號接續[第 3 週講義的參考](../../lectures/wk03_0921_team1-kickoff/README.md#參考)（[1]–[17] 已逐筆查證）；本週新增的 [18]–[21] 依標準書目寫出，**本次未連上 Crossref 逐筆查證**，引用前請自行核對 DOI。

- [[3]](../../lectures/wk03_0921_team1-kickoff/README.md#ref-3) D. A. Huffman, "A Method for the Construction of Minimum-Redundancy Codes," *Proc. IRE*, 1952。——建樹（2.1）。
- <a id="ref-5"></a>[[5]](../../lectures/wk03_0921_team1-kickoff/README.md#ref-5) T. M. Cover and J. A. Thomas, *Elements of Information Theory*, 2nd ed., 2006。——第 5 章：Kraft 不等式、H ≤ L < H + 1（2.5）。
- [[6]](../../lectures/wk03_0921_team1-kickoff/README.md#ref-6) K. Sayood, *Introduction to Data Compression*, 5th ed., 2017。——Huffman 的實作、canonical code、解碼（2.2、2.3）。
- [[17]](../../lectures/wk03_0921_team1-kickoff/README.md#參考) IETF [RFC 1951](https://www.rfc-editor.org/rfc/rfc1951), "DEFLATE Compressed Data Format Specification," 1996。——3.2.2 節：用 code 長度定義 Huffman 表（canonical）；位元順序是 LSB first（1.2 的對照）。
- <a id="ref-18"></a>[18] E. S. Schwartz and B. Kallick, "Generating a Canonical Prefix Encoding," *Communications of the ACM*, vol. 7, no. 3, pp. 166–169, Mar. 1964. [doi:10.1145/363958.363991](https://doi.org/10.1145/363958.363991)。——canonical Huffman 的出處（2.2）。
- [19] J. van Leeuwen, "On the Construction of Huffman Trees," *Proc. 3rd International Colloquium on Automata, Languages and Programming (ICALP)*, Edinburgh, 1976, pp. 382–410。——「先排序一次＋兩個佇列」O(K log K) 的做法（2.1）。
- [20] D. S. Hirschberg and D. A. Lelewer, "Efficient Decoding of Prefix Codes," *Communications of the ACM*, vol. 33, no. 4, pp. 449–459, Apr. 1990. [doi:10.1145/75577.75583](https://doi.org/10.1145/75577.75583)。——解碼的三種寫法（2.3）。
- [21] A. Moffat and A. Turpin, "On the Implementation of Minimum Redundancy Prefix Codes," *IEEE Transactions on Communications*, vol. 45, no. 10, pp. 1200–1207, Oct. 1997. [doi:10.1109/26.634683](https://doi.org/10.1109/26.634683)。——canonical code 的編碼與解碼實作（2.2、2.3）。
- 本 repo：[MP3 樣本與評語](../../samples_2025-C/mini_project_3/README.md)、[MP4 樣本](../../samples_2025-C/mini_project_4/README.md)、[github_actions_ci.md](../../docs/tutorials/github_actions_ci.md)（Q6：MP4 為什麼不比 codebook）、
  [Team 1 規格](../../team_projects/team1_textlink/README.md)、[starter/src/huffman.c](../../team_projects/team1_textlink/starter/src/huffman.c)、[python_ref/textlink.py](../../team_projects/team1_textlink/python_ref/textlink.py)。
- 2023 講義：https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023
