# 第 3 週（2026/9/21）｜Huffman coding、WAV 與 PCM（取樣與量化）、分組與 Team 1 說明

對應：Team 1、MP2（並為 MP3、MP4 鋪路）
時間：13:10–16:00，三節。

> **課前準備（同學）**：在 `mmsp2026` 資料夾打 `git pull` 拿到本週內容；確認第 2 週裝好的 gcc、make、Python 3 還能用。
> 指令一樣分兩欄：左邊 macOS／Linux／WSL，右邊 Windows 的 VSCode PowerShell（`python3` 在 Windows 通常叫 `python`）。
> 本講義的指令都從本週資料夾出發：`cd lectures/wk03_0921_team1-kickoff`。
>
> **上課跟著打指令，請開 [commands.md（一頁版指令表）](commands.md)**：依上課順序列出每一個指令，macOS／Linux 與 Windows PowerShell 各一欄，可以直接複製貼上。

## 本週目標

上完課你應該能：

1. 用 Shannon 的公式算出一組符號的熵，說出它代表什麼，並解釋為什麼「符號怎麼定」會改變熵。
2. 在紙上對一張頻率表做出 Huffman tree、讀出 code、算平均碼長 L，並檢查 H ≤ L < H + 1。
3. 說出 Huffman 建樹在程式裡只需要「一個節點陣列＋一個排序好的佇列」，並看懂每一輪記憶體的變化。
4. 說明聲音變成數字的兩個步驟：取樣（sampling rate、Nyquist、aliasing）與量化（bit depth、量化誤差、SQNR ≈ 6.02n dB）。
5. 用 hex 工具逐欄讀懂一個 WAV 檔的 44 bytes 檔頭，並算出檔案大小與資料率。
6. 說出 Team 1 三個功能、固定的介面、Huffman 的符號定義，以及你這一輪的角色要負責什麼。

## 時程

| 節次 | 時間 | 內容 | 教材／範例 |
|---|---|---|---|
| 1 | 13:10–14:00 | **Huffman coding**：未壓縮的資料有多大、壓縮的分類與常見格式、RLE、熵、Shannon-Fano、Huffman 建樹（逐步示範）、符號的定義與 codebook 成本 | [slides/media_size.html](slides/media_size.html)、[slides/huffman_steps.html](slides/huffman_steps.html)、[examples/huffman_trace.c](examples/huffman_trace.c)、[examples/entropy.c](examples/entropy.c) |
| 2 | 14:10–15:00 | **WAV 與 PCM**：A/D 四步驟、取樣與 aliasing、量化與 SQNR、WAV 檔頭逐欄解讀、MP2 講解 | [slides/a2d_steps.html](slides/a2d_steps.html)、[slides/wav_bmp_viewer.html](slides/wav_bmp_viewer.html)、[examples/alias_demo.py](examples/alias_demo.py)、[examples/quantize_demo.py](examples/quantize_demo.py)、[examples/wav_info.py](examples/wav_info.py)、[data/speech_osr_8k.wav](data/speech_osr_8k.wav) |
| 3 | 15:10–16:00 | **分組與 Team 1 說明**：分組與角色、規格導讀、長度前綴 frame、starter 導覽、時程 | [Team 1 規格](../../team_projects/team1_textlink/README.md)、[starter/](../../team_projects/team1_textlink/starter/)、[examples/chunk_send.py](examples/chunk_send.py) |

先把本週的範例編譯起來：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd examples` | `cd examples` |
| `make` | `mingw32-make` |

會產生 `huffman_trace` 與 `entropy` 兩支程式（Windows 多 `.exe`）。

---

## 第一節｜Huffman coding

### 1.1 為什麼要壓縮、壓縮有哪些種類（8 分鐘）

**先學會估「沒壓縮有多大」。** 任何數位媒體的資料量都是同一個想法：**有幾個樣本 × 每個樣本幾 bits**。

| 媒體 | 樣本是什麼 | 未壓縮的大小 | 位元率（bits／秒） |
|---|---|---|---|
| 文字 | 字元 | 字元數 × 每個字元的 bytes（UTF-8：英數 1、中文 3、emoji 4） | — |
| 音訊 | 每個聲道、每個時間點的振幅 | 取樣率 × bit depth × 聲道數 × 秒數 ÷ 8 | 取樣率 × bit depth × 聲道數 |
| 圖像 | pixel | 寬 × 高 × 每個 pixel 的 bits ÷ 8（全彩 RGB 是 24 bits） | — |
| 影像（視訊） | 每一張圖的每個 pixel | 一張圖的大小 × 每秒幾張（fps）× 秒數 | 寬 × 高 × bits × fps |

**互動計算機**：用瀏覽器打開 [slides/media_size.html](slides/media_size.html)（點兩下即可，不需要網路）。四個分頁各有圖解、可以直接按的範例、
**即時展開的算式**，以及「壓縮後大約多大」的對照長條；另外兩個分頁整理了常見格式與壓縮方法。先按這幾個範例，感受一下量級：

| 範例 | 算式 | 未壓縮 | 實際上常見的 |
|---|---|---|---|
| 一本 50 萬字的中文小說 | 480,000 × 3 + 20,000 × 1 | 約 1.4 MB | zip 後約 45%（無失真） |
| 一首 4 分鐘的 CD 音質歌曲 | 44,100 × 16 × 2 × 240 ÷ 8 | 40.4 MB（1.41 Mb/s） | MP3 128 kb/s：約 3.7 MB |
| 手機拍的 1200 萬畫素照片 | 4000 × 3000 × 24 ÷ 8 | 34.3 MB | JPEG：3–4 MB（約 10:1） |
| 一分鐘 Full HD、30 fps 的影片 | 1920 × 1080 × 24 × 30 × 60 ÷ 8 | 10.4 GB（1.49 Gb/s） | 串流約 5 Mb/s → 約 36 MB（約 300:1） |

**一整本小說比一張照片還小；一分鐘未壓縮的 Full HD 影片，抵得上約 260 首未壓縮的歌。** 家裡 100 Mb/s 的網路連未壓縮的 Full HD 都傳不動（差 15 倍），
所以影像與聲音非壓縮不可，而且要壓得很兇。單位提醒：檔案大小的 KB、MB 習慣用 2¹⁰、2²⁰；位元率的 kb/s、Mb/s 用 10³、10⁶；小寫 b 是 bit、大寫 B 是 byte。

壓縮演算法分兩大類：

| | Lossless（無失真） | Lossy（有失真） |
|---|---|---|
| 解壓縮後 | 與原始資料**完全相同**，一個 bit 都不差 | 犧牲一部分資訊 |
| 丟掉什麼 | 什麼都不丟，只是換一種比較省的表示法 | 人的感官不容易察覺的部分：影像裡細微的顏色變化、聲音裡聽不到的頻率 |
| 例子 | RLE、LZW（GIF、TIFF）、DEFLATE（ZIP、gzip、PNG）、**Huffman** | JPEG、MP3、MPEG |
| 用在 | 文字、程式、不能錯的資料 | 影像、聲音、影片 |

另一種分法是看演算法「靠什麼省」：

| 名稱 | 做法 | 例子 |
|---|---|---|
| Dictionary-based | 用一張固定長度 code 的查找表；一個 code 可以代表「一串」符號 | LZW |
| **Entropy** | 統計每個符號出現的頻率，**常出現的符號用短的 code word、少出現的用長的**；一個符號對應一個 code word | Shannon-Fano、**Huffman** |
| Arithmetic | 同樣靠統計，但把整個檔案編成「一個數」，不是一個符號一個 code | JPEG 的選用步驟 |
| Adaptive | 邊壓縮邊學習資料的特性，隨時調整編碼 | LZW 天生如此；Huffman 也能改成邊讀邊更新頻率 |
| Differential | 不記數值本身，記「與相鄰數值的差」 | 影像、聲音、影片都常用 |

**壓縮率（compression rate）**：原始大小 a、壓縮後大小 b，可以寫成比 **a:b**（例如 2:1），也可以寫成百分比 **b÷a**。
本課程一律用後者：**壓縮率 = 壓縮後 ÷ 原始，越小越好，超過 100% 就是變大了**；Team 1 的 `STATS` 裡 `ratio` 就是這個數。

**常見的格式，各屬於哪一類**（完整版與每種格式裡面用了什麼，見計算機的「常見格式」分頁）：

| 媒體 | 未壓縮 | 無失真 | 有失真 |
|---|---|---|---|
| 文字與一般資料 | TXT、CSV、JSON | ZIP、gzip、7z、bzip2、Zstandard（DOCX、EPUB、APK 其實都是 ZIP） | （不能有失真） |
| 音訊 | WAV、AIFF（PCM） | FLAC、ALAC | MP3、AAC、Opus；電話的 G.711、行動電話的 AMR |
| 圖像 | BMP、RAW | PNG、GIF（最多 256 色） | JPEG、WebP、HEIC、AVIF |
| 影像（視訊） | 幾乎沒有人用 | 幾乎沒有人用 | Motion JPEG、MPEG-2（DVD）、H.264、H.265、VP9、AV1 |

兩個常被搞混的觀念：**codec（資料怎麼壓：H.264、AAC）≠ 容器（把影像、聲音、字幕包成一個檔：MP4、MKV、MOV、AVI）**；
一個 `.mp4` 裡面常見的是 H.264 視訊＋AAC 音訊。WAV 也是容器（RIFF），裡面通常放未壓縮的 PCM（第二節 2.6）。

**真正的格式是把好幾種方法串起來**，而最後一步幾乎都是 entropy coding（計算機的「壓縮方法」分頁有每一條管線）：

```
ZIP／gzip／PNG    LZ77 找重複的字串                                    → Huffman
JPEG              RGB→YCbCr → 色度降取樣 → 8×8 DCT → 量化 → zigzag＋RLE  → Huffman
MP3／AAC          濾波器組＋MDCT → 心理聲學模型 → 量化                   → Huffman
FLAC              線性預測 → 預測誤差                                   → Rice coding（一種 entropy code）
H.264／H.265      畫面內預測或移動補償 → 整數 DCT → 量化                  → 變長碼或自適應算術編碼
電話 G.711        8 kHz 取樣 → μ-law 非均勻量化成 8 bits（64 kb/s）       （沒有 entropy coding）
```

三個重點：(1) **有失真壓縮的「失真」主要發生在「量化」那一步**（JPEG 另外還有色度降取樣）——第二節要講的正是量化；(2) 視訊能壓到幾百分之一，主要靠「相鄰的畫面幾乎一樣」
（移動補償），不是靠 Huffman；(3) **Huffman 不只是文字壓縮**：你每天用的 ZIP、PNG、JPEG、MP3 裡面都有它，第 12 週做 JPEG、Team 3 做 Motion JPEG 時會再遇到。
今天先把這最後一步學起來。

### 1.2 暖身：Run-Length Encoding（4 分鐘）

最簡單的 lossless 壓縮。灰階影像每個 pixel 1 byte（0–255），依「列」的順序一個一個存。假設前 20 個 pixel 是：

```
255 255 255 255 255 255 242 242 242 242 238 238 238 238 238 238 255 255 255 255
```

RLE 不一個一個記，而是記成「（數值 c，連續幾個 n）」：

```
(255,6) (242,4) (238,6) (255,4)
```

原本 20 pixels × 1 byte = 20 bytes。RLE 是 4 組，每組的 c 佔 1 byte，n 要幾 bytes 取決於最長的連續長度 r：

```
b = ⌈ log2(r + 1) ÷ 8 ⌉        r ≤ 255 → 1 byte；r ≤ 65535 → 2 bytes
```

這裡 r = 6，n 用 1 byte 就夠，所以是 4 × 2 = **8 bytes，壓縮率 40%**。RLE 對「大片同色」的影像有效（BMP 支援它），
對聲音幾乎沒用：聲音的 sample 很少連續相同。**RLE 利用的是「重複」，接下來的 entropy coding 利用的是「機率不平均」。**

### 1.3 熵：壓縮的極限在哪裡（10 分鐘）

Claude Shannon 的資訊理論（[[1]](#ref-1)；教科書的整理見 [[5]](#ref-5) 第 5 章）回答了一個問題：無失真壓縮**最多**能壓到多小？
對一串符號 S，第 i 種符號出現的機率是 pᵢ（＝它的次數 ÷ 總數），S 的**熵（entropy）**定義為：

```
H(S) = Σ pᵢ · log2( 1 / pᵢ )        單位：bits／符號
```

它的意義：**不管用什麼方法，平均每個符號至少要 H bits。** 式子裡的 `log2(1/pᵢ)` 是「第 i 種符號理想上該用幾 bits」：
機率 1/2 的符號值 1 bit、機率 1/8 的值 3 bits——越少見的符號，帶的資訊越多，該給它越長的 code。

**例 1：所有符號機率相同。** 一張 256 個 pixel 的影像，每個 pixel 顏色都不一樣，每種顏色的機率都是 1/256：

```
H = Σ(i=0..255) (1/256)·log2(256) = 256 × (1/256) × 8 = 8 bits
```

每個顏色都要 8 bits，和不壓縮一樣：**機率完全平均時，entropy coding 沒得省。**

**例 2：機率不平均。** 同樣 256 個 pixel，但只有 8 種顏色，次數如下：

| 顏色 | black | white | yellow | blue | orange | red | purple | green |
|---|---|---|---|---|---|---|---|---|
| 次數 | 100 | 100 | 20 | 20 | 5 | 5 | 3 | 3 |
| p | 0.3906 | 0.3906 | 0.0781 | 0.0781 | 0.0195 | 0.0195 | 0.0117 | 0.0117 |
| log2(1/p) | 1.356 | 1.356 | 3.678 | 3.678 | 5.678 | 5.678 | 6.415 | 6.415 |
| p·log2(1/p) | 0.530 | 0.530 | 0.287 | 0.287 | 0.111 | 0.111 | 0.075 | 0.075 |

```
H = 0.530 + 0.530 + 0.287 + 0.287 + 0.111 + 0.111 + 0.075 + 0.075 ≈ 2.006 bits／pixel
```

8 種顏色用定長編碼要 3 bits；熵告訴我們平均 **2.006 bits** 就夠。逐項來看：black 理想上用 log2(256/100) ≈ **1.356 bits**、
yellow 用 log2(256/20) ≈ **3.678 bits**。問題來了：**code 的長度必須是整數**，沒有 1.356 bits 這種東西。
怎麼用整數長度盡量逼近這些理想值？這就是 Shannon-Fano 與 Huffman 要解的題。

### 1.4 變長編碼要注意什麼：prefix code（3 分鐘）

如果隨便給變長的 code，例如 A=`0`、B=`01`、C=`1`，收到 `01` 時到底是 B 還是 AC？解不出來。
解法是要求**沒有任何一個 code 是另一個 code 的開頭**（prefix code）。這樣位元流不需要任何分隔符號，從左讀到右就能唯一解碼。
把 code 畫成二元樹（左 0、右 1），只要**所有符號都放在葉節點**，就自動滿足這個條件——所以兩個演算法都是在「建一棵樹」。

### 1.5 Shannon-Fano：由上往下切（5 分鐘）

出處是 Fano 1949 年的技術報告 [[2]](#ref-2)。

1. 把符號依次數由大到小排好。
2. 切成兩堆，讓兩堆的次數總和**盡量接近**；左堆的 code 加一個 `0`、右堆加一個 `1`。
3. 每一堆再照同樣的方法切下去，直到每堆只剩一個符號。

用例 2 的 8 色影像做一次（括號內是該堆的總次數）：

```
{black white yellow blue orange red purple green}(256)
 ├─0─ {black}(100)                                              black  = 0
 └─1─ {white yellow blue orange red purple green}(156)
       ├─0─ {white}(100)                                        white  = 10
       └─1─ {yellow blue orange red purple green}(56)
             ├─0─ {yellow}(20)                                  yellow = 110
             └─1─ {blue orange red purple green}(36)
                   ├─0─ {blue}(20)                              blue   = 1110
                   └─1─ {orange red purple green}(16)
                         ├─0─ {orange red}(10)      orange = 111100   red   = 111101
                         └─1─ {purple green}(6)     purple = 111110   green = 111111
```

平均碼長 L = (100×1 + 100×2 + 20×3 + 20×4 + 5×6 + 5×6 + 3×6 + 3×6) ÷ 256 = 536 ÷ 256 = **2.094 bits**，很接近 H = 2.006。
Shannon-Fano 簡單好懂，但「由上往下切」不保證最佳；下面的 Huffman 保證。

### 1.6 Huffman：由下往上合併（15 分鐘）

David Huffman 1951 年在 MIT 當研究生時，把它當成一門課（Robert Fano 開的資訊理論）的期末報告做出來 [[4]](#ref-4)，1952 年發表 [[3]](#ref-3)。他反過來做：**從最少見的符號開始，由下往上長出一棵樹。**

1. 每種符號是一個節點，數字是次數。全部放進一個**依次數由小到大排好的佇列**。
2. 取出最小的兩個，合併成一個新節點（次數＝兩者相加，兩者當它的左右小孩），把新節點**放回佇列該在的位置**。
3. 重複第 2 步，直到佇列只剩一個節點，它就是樹根。
4. 從根走到每個葉，左分支記 `0`、右分支記 `1`，串起來就是那個符號的 code。

直覺：越早被合併的符號（次數越小）離根越遠，code 越長；每被合併一次，底下所有符號的 code 就多 1 bit。
Huffman 證明了這樣做出來的是**所有 prefix code 裡平均碼長最短的**。

**三種看它動起來的方法**（內容相同，挑適合的用）：

| 方法 | 怎麼開 | 特色 |
|---|---|---|
| 動畫 | 用瀏覽器打開 [slides/huffman_steps.html](slides/huffman_steps.html)（檔案總管裡點兩下即可，不需要網路） | 按「下一步」或鍵盤 → 一格一格走；樹由下往上長、佇列同步更新；可以現場輸入任何文字或頻率表 |
| C 程式 | 見下表 | 把**記憶體裡的節點陣列、排序、佇列**每一輪都印出來；加 `--step` 每一步按 Enter 才繼續 |
| Python | `python3 examples/huffman_demo.py` | 最短，只印每一步合併與結果；手算完對答案用 |

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 8 色影像的例子 | `make trace` | `mingw32-make trace` |
| 一步一步走 | `./huffman_trace --step` | `.\huffman_trace.exe --step` |
| 換成自己的字串 | `./huffman_trace "多媒體多媒多"` | `.\huffman_trace.exe "多媒體多媒多"` |
| 給頻率表 | `./huffman_trace --freq a:5,b:2,c:1` | `.\huffman_trace.exe --freq a:5,b:2,c:1` |

**分鏡：8 色影像的 7 輪合併。** 黃＝這一輪取出的兩個，綠＝新節點；`#8` 是合併出來的內部節點在陣列裡的索引。

```
起始   [green:3] [purple:3] [orange:5] [red:5] [blue:20] [yellow:20] [black:100] [white:100]
第 1 輪 green:3 + purple:3  → #8:6     佇列：[orange:5] [red:5] [#8:6] [blue:20] [yellow:20] [black:100] [white:100]
第 2 輪 orange:5 + red:5    → #9:10    佇列：[#8:6] [#9:10] [blue:20] [yellow:20] [black:100] [white:100]
第 3 輪 #8:6 + #9:10        → #10:16   佇列：[#10:16] [blue:20] [yellow:20] [black:100] [white:100]
第 4 輪 #10:16 + blue:20    → #11:36   佇列：[yellow:20] [#11:36] [black:100] [white:100]
第 5 輪 yellow:20 + #11:36  → #12:56   佇列：[#12:56] [black:100] [white:100]
第 6 輪 #12:56 + black:100  → #13:156  佇列：[white:100] [#13:156]
第 7 輪 white:100 + #13:156 → #14:256  佇列：[#14:256]   ← 只剩一個，它就是根
```

建好的樹（橫著畫：右分支 1 在上、左分支 0 在下）：

```
               ┌─1─ [black:100]
       ┌─1─ (156)
       |       |               ┌─1─ [blue:20]
       |       |       ┌─1─ (36)
       |       |       |       |               ┌─1─ [red:5]
       |       |       |       |       ┌─1─ (10)
       |       |       |       |       |       └─0─ [orange:5]
       |       |       |       └─0─ (16)
       |       |       |               |       ┌─1─ [purple:3]
       |       |       |               └─0─ (6)
       |       |       |                       └─0─ [green:3]
       |       └─0─ (56)
       |               └─0─ [yellow:20]
    (256)
       └─0─ [white:100]
```

| 顏色 | 次數 | 理想 log2(1/p) | Huffman 長度 | code |
|---|---|---|---|---|
| white | 100 | 1.356 | 1 | `0` |
| black | 100 | 1.356 | 2 | `11` |
| yellow | 20 | 3.678 | 3 | `100` |
| blue | 20 | 3.678 | 4 | `1011` |
| green | 3 | 6.415 | 6 | `101000` |
| purple | 3 | 6.415 | 6 | `101001` |
| orange | 5 | 5.678 | 6 | `101010` |
| red | 5 | 5.678 | 6 | `101011` |

**三個數字一起看：**

```
熵           H = 2.0063 bits／pixel        （理論下限）
Huffman      L = Σ pᵢ·lenᵢ = 536 ÷ 256 = 2.0938 bits／pixel     效率 H ÷ L = 95.8%
定長編碼      3 bits／pixel → 768 bits；Huffman 536 bits → 壓縮率 69.8%（約 1.43:1）
```

- **H ≤ L < H + 1 永遠成立**（這裡 2.006 ≤ 2.094 < 3.006）。寫程式時拿它當檢查：算出來不滿足，就是程式有錯。
  唯一的例外是「只有一種符號」：H = 0，但 code 長度不能是 0，所以 L = 1。
- L 比 H 多出來的 0.09 bits，是「長度必須是整數」的代價。black 與 white 理想上都是 1.356 bits，卻只能一個給 1、一個給 2。
- 這個例子裡 Shannon-Fano 與 Huffman 的碼長完全相同（都是 1、2、3、4、6、6、6、6）。一般情況下 Huffman 不會比較差，有時比較好。
- **Huffman code 不是唯一的**：次數相同時先取誰（black 還是 white 拿到 1 bit 的 code）、左右怎麼擺都可以，
  每個符號的長度可能不同，但平均碼長 L 一定相同。所以傳送端與接收端必須用**同一個平手規則**，或者直接把 codebook 傳過去。

**在動畫裡切換另外三個內建例子，各看一個重點：**

| 例子 | 看什麼 |
|---|---|
| 8 種符號機率相同 | H = L = 3 = 定長編碼：機率平均時 Huffman 沒得省（呼應 1.3 的例 1） |
| 次數 64、32、16、8、4、2、1、1 | 機率全是 2 的負次方 → 理想長度剛好都是整數 → **L 恰好等於 H**，效率 100% |
| 只有一種符號 | 樹只有一個葉；code 長度要定為 1，不能是 0，否則解碼端不知道有幾個符號 |

**程式裡長什麼樣子。** `huffman_trace` 只用兩個陣列，沒有用到 `malloc`：

```
node[]   所有節點。前 K 個是葉，後面是合併出來的內部節點，最多 2K−1 個。
         每個節點記 weight、parent、left、right ——「樹」就是陣列裡互相指來指去的索引。
queue[]  還沒被合併的節點的索引，永遠維持 weight 由小到大。
         取最小的兩個 ＝ 拿走最前面兩個；放回新節點 ＝ 插到排序後該在的位置（insertion sort 的一步）。
```

第 3 輪之後記憶體裡是這樣（`parent = -1` 表示還在佇列裡）：

```
idx  symbol       weight  parent  left  right
  0  black           100      -1    -1     -1
  1  blue             20      -1    -1     -1
  2  green             3       8    -1     -1
  3  orange            5       9    -1     -1
  4  purple            3       8    -1     -1
  5  red               5       9    -1     -1
  6  white           100      -1    -1     -1
  7  yellow           20      -1    -1     -1
  8  (internal)        6      10     2      4      ← green + purple
  9  (internal)       10      10     3      5      ← orange + red
 10  (internal)       16      -1     8      9      ← #8 + #9，還在佇列裡
```

要讀出某個葉的 code：從它沿著 `parent` 一路走到根，每一步記下「我是左小孩（0）還是右小孩（1）」，最後把順序倒過來。
例如 green：`#8(0) → #10(0) → #11(0) → #12(1) → #13(0) → #14(1)`，倒過來是 `101000`。

每一輪「插回佇列」最多要挪動整個佇列，K 種符號共約 K² 次搬移。K = 8 無所謂；但 Team 1 的 WAV 以 sample 為符號，
K 可以到幾萬，就得改用 heap，或「先排序一次、再用兩個佇列」的 O(K log K) 做法（第 5 週講實作）。

### 1.7 「符號」是什麼，比演算法重要；還有 codebook 的成本（5 分鐘）

Huffman 是對「符號的機率」編碼。**同一個檔案，符號定得不一樣，熵就不一樣。** 用第 2 週的測試檔算算看：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `./entropy ../../wk02_0914_text-utf8/data/sample_zh_en.txt` | `.\entropy.exe ..\..\wk02_0914_text-utf8\data\sample_zh_en.txt` |

```
file: ../../wk02_0914_text-utf8/data/sample_zh_en.txt  (162 bytes)
[byte]  N=162  K=83  H=6.0120 bits/symbol  ideal=75.15%
[char]  N=105  K=64  H=5.6346 bits/symbol  ideal=45.65%
```

- 以 **byte** 為符號：162 個符號，每個至少 6.01 bits → 最好也只能壓到 75%。
- 以 **UTF-8 字元**為符號：只有 105 個符號（一個中文字算 1 個，不是 3 個），每個 5.63 bits → 可以壓到 46%。

把「多」拆成 `E5`、`A4`、`9A` 三個符號分開統計，等於丟掉了「`E5` 後面常常接 `A4`」這個資訊。
**所以 Team 1 規定：文字以 UTF-8 字元為符號、WAV 以 16-bit sample 為符號**（第二節會看到 sample 是什麼）。
`entropy.c` 與 `entropy.py` 輸出逐 byte 相同，`make check`（Windows `mingw32-make check`）會幫你比對——仍然是「C 實作、Python 對答案」的模式。

**別忘了 codebook。** 解碼端要有同一張 code 表才解得開，這張表也要佔空間。`ABRACADABRA` 11 bytes 經 Huffman 只剩 23 bits（3 bytes），
但加上「A、B、C、D、R 各自的 code」之後，整包反而比原文大。資料越短、符號種類越多，codebook 越不划算——
這正是 Team 1 聊天訊息要你們面對的設計問題，也是報告裡壓縮率一定要「含 codebook」的原因。

**和作業的關係**：MP3 做定長編碼（FLC）＋位元打包；MP4 做 Huffman 編、解碼（以 UTF-8 字元為符號）；
Team 1 要再加上 sample 與 byte 兩種符號，並經由網路傳輸。第 5 週（10/5）講 C 的實作細節（建樹、bit writer／reader）；
**Team 1 第 2 週就要開始寫，請先用今天的觀念與 `huffman_trace.c` 的資料結構動手，不要等到 10/5。**

延伸（不考、知道就好）：arithmetic coding 把整個檔案編成一個數，不受「每個符號整數個 bits」的限制，可以比 Huffman 更接近 H；
adaptive Huffman 邊讀邊更新頻率，不需要先掃一遍、也不需要傳 codebook。

---

## 第二節｜WAV 與 PCM：聲音怎麼變成數字

### 2.1 Analog to Digital：四個步驟（8 分鐘）

先看整條路。你對手機說一句話、它用合成的聲音回答你，聲音走過的是這樣一條路：

```
說話（空氣壓力的變化）→ 麥克風 → 類比訊號 → ADC（類比轉數位：取樣率如 44.1 kHz、解析度如 16 bits）→ 數位訊號
      → 計算機處理（辨識、壓縮、傳輸…）→ DAC（數位轉類比，規格要與 ADC 相符）→ 擴大器 → 喇叭／耳機
```

- **麥克風**把聲音在空氣中造成的「力」轉成「電」。不論哪一種麥克風都有一片振膜（diaphragm）接受氣壓的變化：
  動圈式靠法拉第定律（振膜帶動線圈在磁場裡移動，感應出電流，像一台很小的發電機）；電容式靠兩片極板的間距改變，使極板上的電荷量跟著變。
- 得到的電訊號叫**類比（analog）訊號**，字面上的意思就是用隨時間改變的電壓或電流去「比擬」原本的氣壓。把隨時間改變的量畫出來，就是**波形（waveform）**。
- **digital** 這個字來自 digit，源自拉丁文 digitus，「手指」——手指是用來數數的。數位化就是把訊號變成「數得出來的有限個整數」。

為什麼一定要數位化？麥克風輸出的波形，時間上有無限多個點，每一點的電壓也有無限多種可能的值；而電腦的儲存空間有限
（你的隨身碟 32 GB、記憶體 8 GB）。所以在送進 ADC 之前先做**放大與濾波**（麥克風的訊號很微弱，要先放大；人耳聽不到、或不打算處理的高頻先濾掉，見 2.2），然後：

| 步驟 | 做什麼 | 結果 |
|---|---|---|
| 0 | 麥克風 → 電壓波形 | 連續訊號：時間與振幅都有無限多種可能 |
| 1. **Sampling（取樣）** | 每隔 T 秒量一次電壓 | 離散時間訊號：時間點有限，但振幅仍有無限多種可能 |
| 2. **Quantization（量化）** | 把每個量到的值歸到 M 個等級中最近的一個 | 時間與振幅都有限 |
| 3. **Encoding（編碼）** | 每個等級用固定長度的二進位數表示 | 數位訊號：一串 0 與 1 |

不論是聲音還是影像，類比轉數位都是「取樣＋量化」這兩步：聲音的取樣點在**時間**上等距，影像的取樣點在**空間**上等距。
這樣直接把每個 sample 的量化值存下來的方式叫 **PCM（Pulse Code Modulation）**；CD、電話網路、WAV 檔裡放的都是 PCM。

**互動動畫**：用瀏覽器打開 [slides/a2d_steps.html](slides/a2d_steps.html)（點兩下即可，不需要網路）。上方四個方塊就是上表的四步，
按「下一步」或鍵盤 → 依序在同一張圖上加上取樣點、量化等級、二進位碼；右邊的拉桿可以即時改頻率、取樣率、bit depth 與振幅。
本節後面的每個觀念都可以在上面現場示範：

| 要示範什麼 | 怎麼操作 | 看哪裡 |
|---|---|---|
| aliasing（2.2） | 按「aliasing：f=5000」 | 紅色取樣點同時落在紫色虛線的 3000 Hz 弦波上；把取樣率往上拉過 10,000 Hz，紫線消失 |
| bit depth 與量化誤差（2.3） | 停在第 ③ 步，拉 bit depth | 虛線等級變密、橘色誤差變短；下方誤差圖的色帶是 ±Δ/2 |
| clipping（2.3） | 按「2 bits」，或把振幅拉到 1.0 | 紅框警告有幾 % 的 sample 超出範圍；誤差圖的長條頂到上下緣 |
| 每多 1 bit 多 6 dB（2.4） | 第 ③ 步，bit depth 一格一格加 | 右邊「SQNR（實際量）」每次約 +6 dB，與 6.02m 兩列對照 |
| 音量太小浪費 bit depth（2.4） | 按「音量太小」 | 只用到中間兩三個等級，右邊的 SQNR 大幅下降；再把 bit depth 拉高，看要多幾 bits 才補得回來 |
| 編碼（2.1、2.6） | 第 ④ 步，切換「編碼」 | 同一個等級在「符號位＋大小」與「二補數」下的 bits 不同；負數差最多 |

用 4 bits 編碼的例子：最高位是正負號，後 3 bits 是大小，可以表示 −7 到 +7：

```
[0111] → (−1)^0 × (1×2² + 1×2¹ + 1×2⁰) = +7        [0110] → +6        [1010] → (−1)^1 × (0×4 + 1×2 + 0×1) = −2
一段波形取樣、量化、編碼之後：0000 0101 0101 0110 0111 0111 0110 0100 1010 1101 1110 1111 …
```

（這個「符號位＋大小」的寫法容易懂；實際的 16-bit WAV 用的是二補數，見 2.6。）

二補數的例子（M = 8 bits，可表示 −128 到 127）：五個 sample 值 −120、100、0、1、127 存成

```
−120 → 1000 1000      100 → 0110 0100      0 → 0000 0000      1 → 0000 0001      127 → 0111 1111
```

**反方向：數位轉類比（DAC）。** 知道每個 sample 用幾 bits、取樣率是多少之後，電路每隔 1/f_s 秒把輸出電位換成下一個 sample 的值，
得到一條階梯狀的波形；再用低通濾波器把「階梯的稜角」（不連續造成的高頻）濾掉，就還原成平滑的類比訊號，送去推動喇叭。
DAC 就是 ADC 的反推；兩邊的取樣率與 bit depth 必須一致，否則聲音的快慢、音高都會不對。

三種常見的規格：

| 用途 | 取樣率 | bit depth |
|---|---|---|
| CD | 44.1 kHz | 16 bits |
| 語音辨識 | 16 kHz | 16 bits |
| 電話 | 8 kHz | 16 bits（傳輸時常再壓成 8 bits） |

取樣率越低，高頻越少，聲音越「悶」；電話聽起來就是這樣。

### 2.2 Sampling：取樣率、Nyquist、aliasing（10 分鐘；頻譜複製那一段課後讀）

取樣定理的原始文獻是 Nyquist 1928 [[8]](#ref-8) 與 Shannon 1949 [[9]](#ref-9)。

先複習幾個量：頻率 f（Hz ＝ 每秒幾個週期）與週期 T 互為倒數，`T = 1/f`；寫成弦波要換成角頻率 `ω = 2πf`（radians／秒）。
振幅對應**響度**，頻率對應**音高**；複雜的波形可以拆成許多不同頻率的弦波相加（Fourier transform，第 7 週）。

取樣就是每隔 T 秒取一個值，把連續訊號 x_c(t) 變成數列：

```
x[n] = x_c(nT) = x_c(n / f_s)          T：取樣週期（sampling period）；f_s = 1/T：取樣率（sampling rate）
```

x_c(t) 的 t 是連續的實數，有無限多個；x[n] 的 n 是整數，**數得出來**。例如 1 秒鐘的語音用 f_s = 16,000 Hz 取樣，
就是 n = 0, 1, 2, …, 15999 共 16,000 個取樣值（sample value），第 n 個值對應到原本世界的時刻 n/16000 秒。
再一個例子：音叉敲出 440 Hz 的 A4，x_c(t) = cos(2π·440·t)，用 f_s = 8000 Hz 取樣：

```
x[n] = cos(2π · 440 · n/8000) = cos( (11/100)·π·n )          每 200 個 sample（11 個週期）才重複一次
```

取樣率越高，能描述的訊號頻寬越寬、波形越細緻：取樣之後最高只能表示到 f_s/2。同一句話分別用 48 kHz、16 kHz、8 kHz 錄，
整段看起來幾乎一樣；**放大到 5 毫秒**才看得出差別——那 5 ms 裡分別有 240、80、40 個 sample，一根一根的，取樣率越高排得越密。
[wav_bmp_viewer.html](slides/wav_bmp_viewer.html) 的「放大 5 毫秒」就是在看這件事。

**Nyquist 取樣定理**：要保證不失真，取樣率必須**大於訊號最高頻率的兩倍**：

```
f_s > 2 · f_max          對單一頻率 f 的弦波，取樣率的下限 r = 2f
```

（要「大於」而不只是「等於」：f_s 剛好等於 2f 時，每個週期只取到兩點，運氣不好兩點都落在過零點，取到的全是 0。）

- `f_s / 2` 叫 **Nyquist frequency**：這個取樣率「能正確表示的最高頻率」，是取樣系統的性質。
- `2 · f_max` 叫 **Nyquist rate**：這個訊號「需要的最低取樣率」，是訊號的性質。兩個名詞不要搞混。
- 人耳聽到約 20 kHz，所以 CD 用 44.1 kHz；電話只保留約 3.4 kHz 以下的語音，所以 8 kHz 就夠。

**取樣率不夠會怎樣？Aliasing。** 以 f_s = 8000 Hz 取樣兩個弦波：

```
情況 1：f = 3000 Hz < 4000 Hz      x₁[n] = sin(2π · 3000 · n/8000)                                   沒問題
情況 2：f = 5000 Hz > 4000 Hz      x₂[n] = sin(2π · 5000 · n/8000) = sin(2πn − 2π · 3000 · n/8000)
                                        = −sin(2π · 3000 · n/8000) = −x₁[n]
```

5000 Hz 的弦波取樣之後，得到的數列和 3000 Hz 的**一模一樣，只差正負號**。取樣完成之後，再也沒有任何方法分辨它原本是 3000 還是 5000 Hz：
高頻「冒充（alias）」成了低頻。跑跑看，並用耳朵聽：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 alias_demo.py` | `python alias_demo.py` |

```
   n    x1[n] = sin(2π·3000·n/fs)    x2[n] = sin(2π·5000·n/fs)     x1 + x2
   1       0.707107                    -0.707107                0.000000
   2      -1.000000                     1.000000                0.000000
   原始頻率 f     取樣後聽到的頻率
      3900 Hz         3900 Hz
      4100 Hz         3900 Hz  ← aliasing
      5000 Hz         3000 Hz  ← aliasing
      7000 Hz         1000 Hz  ← aliasing
      8500 Hz          500 Hz  ← aliasing
```

程式會寫出 `tone_3000.wav` 與 `tone_5000_aliased.wav`，兩個檔播放起來音高完全相同。
規律：超過 f_s/2 的頻率會以 f_s/2 為軸「折回來」。影像也一樣：取樣不足會出現原本沒有的條紋（摩爾紋）。

修過訊號與系統的同學可以這樣理解：取樣＝乘上週期為 T 的脈衝串，頻譜因此以 Ω_s = 2π/T 為間隔不斷複製：

```
x_s(t) = x_c(t) · Σ_k δ(t − kT)      ⟷      X_s(jΩ) = (1/T) · Σ_k X_c( j(Ω − kΩ_s) )
```

Ω_s ≥ 2Ω_N 時，這些複製的頻譜互不重疊，用低通濾波器就能取回原訊號；否則相鄰的頻譜疊在一起，就是 aliasing。
實務上 A/D 之前一定先放一個 anti-aliasing 低通濾波器，把 f_s/2 以上的成分先濾掉。

### 2.3 Quantization：bit depth 與量化誤差（7 分鐘）

量化要求每個 sample 用固定的 bits 數表示，這個數叫 **sample size**，也叫 **bit depth**；它限制了每個 sample 能表示得多精確。

```
m bits → L = 2^m 個量化等級（quantization levels）        8 bits → 256；16 bits → 65,536；32 bits → 4,294,967,296
均勻量化（linear／uniform quantization）：   Q(x[n]) = Δ · ⌊ x[n]/Δ + 1/2 ⌋        Δ：量化階距（step size）
量化誤差（quantization error／noise）：     e[n] = x[n] − Q(x[n])                  在沒有超出範圍時，|e[n]| ≤ Δ/2
```

影像也是同一回事：每個 pixel 的 R、G、B 各用 8 bits，就是各量化成 256 個等級；bit depth 越大，顏色變化越細緻，檔案也越大。
量化誤差可以看成疊加在訊號上的一個隨機雜訊，所以也叫量化雜訊。

**寫成程式時的樣子。** 先把振幅正規化到 −1.0 ≤ x[n] ≤ 1.0，用 M bits 量化就是「乘上 2^(M−1)、四捨五入成整數、再限制在能表示的範圍內」：

```
q̃[n] = ROUND( 2^(M−1) · x[n] )                                   ← 整數，可能的值是 −2^(M−1) … +2^(M−1)，共 2^M + 1 個
q̂[n] = max{ min{ q̃[n], 2^(M−1) − 1 }, −2^(M−1) }                 ← M bits 只放得下 2^M 個：把 +2^(M−1) 壓回 2^(M−1) − 1
x̂[n] = q̂[n] / 2^(M−1)                                            ← 還原回 −1 到 1 之間；量化誤差 e[n] = x̂[n] − x[n]
```

這和上面的 Q(x) = Δ⌊x/Δ + ½⌋ 是同一件事，只是 Δ = 1/2^(M−1)：q̂ 是「第幾個等級」（存進檔案的整數），x̂ = q̂·Δ 是它代表的值。
中間那一行就是 clipping 的來源：M = 16 時，+1.0 會算出 32768，但 16 bits 最大只到 32767。
（誤差寫成 x̂ − x 或 x − x̂ 只差正負號，算功率與 SQNR 時沒有差別。）
另一種常見的慣例是乘上 **2^(M−1) − 1**（16 bits 就是 32767）而不是 2^(M−1)：這樣 ±1.0 都不會超出範圍、不需要 clip，代價是階距大了一點點。
MP2 的參考實作用的是這一種（2.8）；兩種都有人用，重點是**編碼端與解碼端要用同一種**。

**bit depth 不夠，最先壞掉的是小聲的地方。** 8 bits 在 −1.0 到 +1.0 之間只有 256 個等級，每一級約 0.008。
一句話剛開始、音量還很小的那一段（振幅只有 ±0.05），16 bits 有三千多個等級可以用，8 bits 只剩十幾個——
波形變成一階一階的，不再連續。用 [wav_bmp_viewer.html](slides/wav_bmp_viewer.html) 把放大的位置拖到小聲的地方，
bit depth 從 16 降到 8、6、4，可以看到也聽到這件事；降到 4 bits 時，小聲的部分會整個被量化成 0 而消失。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 quantize_demo.py` | `python quantize_demo.py` |

3-bit 量化（8 個等級，Δ = 0.25）的前幾個 sample：

```
    n      x[n]    Q(x[n])     e[n] = x − Q(x)
    1    0.3387    0.2500     0.0887
    2    0.6374    0.7500    -0.1126
    3    0.8607    0.7500     0.1107
    4    0.9823    0.7500     0.2323      ← 誤差超過 Δ/2 = 0.125！
    5    0.9877    0.7500     0.2377      ← 同上
```

第 4、5 個 sample 的誤差為什麼特別大？8 個等級是 −1.00、−0.75、…、+0.50、**+0.75**，最大只到 0.75，
0.98 沒有對應的等級，只能被壓在 0.75——這叫 **clipping（overload）**。去年 MP2 拿 95 分的樣本，評語提醒的就是這一類問題：
「觀察在極值（弦波最高點）是否發生 overflow」。程式會另外寫出 `out_16bit.wav`、`out_8bit.wav`、`out_4bit.wav`，
依序播放，4-bit 的「沙沙聲」就是量化雜訊。

也有不均勻的量化（A-law、μ-law）：小訊號用細的階距、大訊號用粗的階距，電話網路（G.711 [[15]](#ref-15)）用它把 13～14 bits 的線性 PCM 壓成 8 bits。本課程只做均勻量化。

### 2.4 分貝、SNR 與 SQNR（8 分鐘；推導課後讀，課堂上看實測的表）

「量化誤差當成均勻分布的雜訊、功率是 Δ²/12」這個模型出自 Bennett 1948 [[10]](#ref-10)；量化理論的完整回顧見 [[11]](#ref-11)。

**訊號雜訊比（SNR）**＝有用訊號的功率 ÷ 雜訊的功率。因為數字範圍很大，習慣取對數，用**分貝（dB）**表示。
dB 沒有單位，它描述的是兩個量的**相對**大小：

```
dB = 10 · log10( P / P₀ )          P、P₀：兩個訊號的功率（power／intensity）
dB = 20 · log10( V / V₀ )          V、V₀：兩個訊號的振幅（電壓、聲壓…）
```

兩個定義是同一件事。功率、電壓、電阻的關係是 `P = V²/R`，假設兩個訊號的 R 相同：

```
10·log10( P/P₀ ) = 10·log10( (V²/R) / (V₀²/R) ) = 10·log10( (V/V₀)² ) = 20·log10( V/V₀ )
```

記兩個數字就夠：功率加倍 ≈ +3 dB；振幅加倍 ≈ +6 dB。

雜訊來自量化時，就叫 **SQNR（signal-to-quantization-noise ratio）**。它有兩種算法，都要會：

**(a) 由 bit depth 直接估：最大訊號振幅 ÷ 最大量化誤差。** n bits 的 sample 值範圍是 −2^(n−1) 到 2^(n−1) − 1，
最大量化誤差是半個等級：

```
SQNR = 20·log10( 2^(n−1) / (1/2) ) = 20·log10( 2^n ) = 20n·log10(2) ≈ 6.02 n  dB
```

**每多 1 bit，SQNR 多 6 dB。** 8 bits ≈ 48 dB、16 bits ≈ 96 dB。這個數字也就是該 bit depth 的 **dynamic range（動態範圍）**：
能表示的最大振幅與最小振幅的比。它描述的是「振幅變化能刻畫得多細」，不是訊號值的範圍。

**(b) 照定義用功率實際量（MP2 要你算的是這個）：**

```
SQNR = 10·log10( P / P₀ )        P = (1/N)·Σ x²[n]（訊號功率）        P₀ = (1/N)·Σ e²[n]（量化雜訊功率）
```

對一個**振幅為 A 的弦波**（A = 1 是滿刻度），P = A²/2；量化誤差若均勻分布在 ±Δ/2 之間，P₀ = Δ²/12，代入得：

```
SQNR ≈ 6.02 n + 1.76 + 20·log10(A)  dB
```

比 (a) 多出來的 1.76 dB 來自「弦波的功率」與「均勻分布雜訊的功率」各自的常數。`quantize_demo.py` 實際量出來的數字：

```
 bits   階數 L=2^n      量到的 SQNR    6.02n    6.02n+1.76
   4           16        23.18 dB    24.08        25.84
   8          256        49.58 dB    48.16        49.92
  12        4,096        74.63 dB    72.24        74.00
  16       65,536        98.54 dB    96.32        98.08
```

- bits 數多的時候（6 bits 以上），實測值與 6.02n + 1.76 只差 1 dB 左右（上面只節錄四列；程式印出的完整表裡差最多的是 13 bits：78.89 對 80.02，差 1.13 dB）；bits 數很少時（2–4 bits）「誤差均勻分布」的假設不成立，偏差較大。
- 再跑一次 `python3 quantize_demo.py 0.5`：振幅減半，每一列的 SQNR 都少 **6 dB**（20·log10(0.5) = −6.02）。
  **錄音音量太小，等於白白浪費 bit depth。**

### 2.5 未壓縮的資料有多大（1 分鐘：1.1 已經算過）

```
資料率 = 取樣率 × bit depth × 聲道數
CD：44,100 × 16 × 2 = 1,411,200 bits/s ≈ 1.41 Mb/s
一分鐘：1,411,200 × 60 = 84,672,000 bits = 10,584,000 bytes ≈ 10.09 MB
```

（投影片上寫 1.35 Mb/s，是用 2²⁰ 去除的結果；位元率的 M 習慣上是 10⁶，所以是 1.41 Mb/s。檔案大小的 10.09 MB 才是用 2²⁰。）
對照：一張 1024×768 的 RGB 影像是 1024 × 768 × 3 = 2,359,296 bytes（2.25 MB）；一分鐘 720×480、每秒 30 張的影片是
14,929,920,000 bits ≈ 1.74 GB。第一節的[計算機](slides/media_size.html)可以換任何規格來算；這一節我們知道了公式裡的「取樣率」與「bit depth」是怎麼來的。

### 2.6 WAV 檔：把 PCM 包起來（7 分鐘）

各欄位的定義以 [[13]](#ref-13) 為準（該頁同時收錄 Microsoft／IBM 的原始規格文件）。

WAV 是 **RIFF** 容器：開頭 12 bytes，後面接一串 **chunk**；每個 chunk ＝ 4 bytes 的 id ＋ 4 bytes 的大小 ＋ 內容
（內容長度是奇數時補 1 byte）。最常見的 WAV 只有 `fmt ` 與 `data` 兩個 chunk，檔頭剛好 44 bytes：

| 位移 | bytes | 欄位 | 說明 |
|---|---|---|---|
| 0 | 4 | `"RIFF"` | 容器的記號 |
| 4 | 4 | ChunkSize | 檔案大小 − 8 |
| 8 | 4 | `"WAVE"` | 這個 RIFF 裝的是 WAV |
| 12 | 4 | `"fmt "` | 格式 chunk 的 id（注意結尾有一個空白） |
| 16 | 4 | Subchunk1Size | PCM 為 16 |
| 20 | 2 | AudioFormat | 1 ＝ PCM（未壓縮） |
| 22 | 2 | NumChannels | 1 單聲道、2 雙聲道 |
| 24 | 4 | SampleRate | 取樣率 f_s |
| 28 | 4 | ByteRate | SampleRate × BlockAlign |
| 32 | 2 | BlockAlign | NumChannels × BitsPerSample ÷ 8（一個時間點所有聲道共幾 bytes） |
| 34 | 2 | BitsPerSample | bit depth |
| 36 | 4 | `"data"` | 資料 chunk 的 id |
| 40 | 4 | Subchunk2Size | PCM 資料的 bytes 數 |
| 44 | … | PCM sample | 雙聲道時左右交錯：L R L R … |

用第 2 週學的 hex 工具打開本週的語音檔（Open Speech Repository 的測試語音，8 kHz、16-bit、單聲道）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `hexdump -C -n 48 ../data/speech_osr_8k.wav` | `Format-Hex ..\data\speech_osr_8k.wav \| Select-Object -First 5` |

```
00000000   52 49 46 46 96 35 08 00 57 41 56 45 66 6D 74 20  RIFF.5..WAVEfmt
00000010   10 00 00 00 01 00 01 00 40 1F 00 00 80 3E 00 00  ........@....>..
00000020   02 00 10 00 64 61 74 61 72 35 08 00 69 FC DE FA  ....datar5..i...
```

逐欄讀（**所有數字都是 little-endian：低位的 byte 在前**）：

```
52 49 46 46   "RIFF"
96 35 08 00   0x00083596 = 538,006 = 檔案大小 538,014 − 8
57 41 56 45   "WAVE"          66 6D 74 20   "fmt "          10 00 00 00   16
01 00         AudioFormat = 1（PCM）                        01 00         NumChannels = 1
40 1F 00 00   0x1F40 = 8000 Hz                              80 3E 00 00   0x3E80 = 16000 bytes/s
02 00         BlockAlign = 2                                10 00         BitsPerSample = 16
64 61 74 61   "data"          72 35 08 00   0x00083572 = 537,970 bytes ＝ 268,985 個 sample ＝ 33.6 秒
69 FC         第一個 sample = 0xFC69 = −919（16-bit 二補數）
```

要特別留意的三件事：

1. **WAV 是 little-endian，Team 1 的 frame 長度是 big-endian。** 同樣是「4 bytes 的整數」，8000 在 WAV 裡是 `40 1F 00 00`，
   在 big-endian 裡是 `00 00 1F 40`。不管哪一種，C 裡都要用位移一個 byte 一個 byte 組，不要把 `int` 的記憶體直接 `fwrite`／`send` 出去。
2. **16-bit sample 是有號的二補數（−32768 到 32767）；8-bit sample 卻是無號的（0 到 255，128 代表 0）。** 這是歷史包袱，MP2 會踩到。
3. **data 不一定從第 44 byte 開始。** `fmt ` 與 `data` 之間可能夾著 `LIST` 等其他 chunk。要讀別人的 WAV，得一個 chunk 一個 chunk 走；
   Team 1 的 WAV 功能就得這樣做。

**用瀏覽器打開真實的檔案**：[slides/wav_bmp_viewer.html](slides/wav_bmp_viewer.html)（點兩下即可，不需要網路）。

| 分頁 | 可以做什麼 |
|---|---|
| WAV | 內建一段真實的語音（8 kHz）與一段真實的音樂（44.1 kHz），可以**播放**；檔頭的每個 byte 依欄位上色，滑鼠移到欄位表的某一列，對應的 bytes 會框起來；用公式驗算出檔案大小；看整段與放大 5 ms 的波形；**換取樣率（÷2、÷4、÷8，可選要不要先做 anti-aliasing 濾波）與 bit depth（16 到 2 bits）再聽一次**，並顯示位元率與 SNR |
| BMP | 內建一張真實的照片，由檔案裡的 bytes 直接**畫出來**；54 bytes 的檔頭逐欄解讀；滑鼠移到圖上，顯示那個 pixel 的 R、G、B 與它在檔案裡的位置；**換解析度與每個顏色的 bits**，看馬賽克與色帶 |
| 兩者 | 右上角可以選自己電腦裡的 .wav 或 .bmp（檔案只在你的瀏覽器裡處理，不會上傳） |

建議現場做的三件事：(1) 音樂 ÷4、選「直接每 N 個取 1 個」，聽 aliasing 造成的怪聲，再切回「先低通濾波」比較；
(2) 語音的 bit depth 從 16 一路降到 4，聽量化雜訊怎麼出現、小聲的地方怎麼消失；(3) BMP 每個顏色降到 3 bits，看海面與雲出現色帶——那就是看得見的量化誤差。

BMP 是第 11 週的主題，這裡先看一眼，因為它和 WAV 是同一個想法：**一小段檔頭（說明書）＋一個接一個的樣本**。
差別在三個細節：pixel 的 3 bytes 順序是 B、G、R；高度為正時，資料由圖的最下面一列開始存；每一列的 bytes 數要補到 4 的倍數。

[wav_info.py](examples/wav_info.py) 把上面的事自動做完，還會印出 sample 值的 histogram：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 wav_info.py ../data/speech_osr_8k.wav` | `python wav_info.py ..\data\speech_osr_8k.wav` |

### 2.7 把兩節接起來：sample 的 histogram 與 Huffman（3 分鐘）

```
  sample 值：最小 -8054、最大 10751；用掉了 12,343 種不同的值（最多 65,536 種）
  以 sample 為符號：H = 11.763 bits／sample → 理論壓縮率 73.5%；但 codebook 要記 12,343 種符號
  以 byte   為符號：H = 6.653 bits／byte   → 理論壓縮率 83.2%
    -8192 ~  -4097 | #                                                      1,922
    -4096 ~     -1 | ##################################################   140,157
        0 ~   4095 | ###########################################          120,829
     4096 ~   8191 | ##                                                     5,866
```

- 語音的 sample 值**高度集中在 0 附近**（大部分時間很小聲），機率很不平均——這正是 Huffman 能利用的。
- 第一節 1.7 的結論在這裡再出現一次：**以 sample 為符號（73.5%）比以 byte 為符號（83.2%）好**，因為一個 sample 的高、低兩個 byte 其實是相關的。
- 但符號種類有 12,343 種，codebook 很大，會把省下來的空間吃掉一大塊。Team 1 的報告要你們量出這件事。
- 1.1 提過的 **differential encoding** 在聲音上特別有效：相鄰 sample 的「差」比 sample 本身小得多、分布更集中。
  這是 Team 2 的伏筆，Team 1 列為加分方向。

### 2.8 MP2 講解：Function Generator——用程式模擬一台 A/D 轉換器（6 分鐘；細節表課後讀）

**MP2 的程式，就是 2.1 那四個步驟的軟體模擬。** 真實世界裡，連續的訊號來自麥克風；在 MP2 裡，它來自一條數學式。
數學式可以在任何時刻 t 算出值，所以它扮演的正是「連續訊號」；接下來你的程式對它取樣、量化、編碼，和一顆 A/D 晶片做的事一模一樣：

| A/D 的步驟（2.1） | 真實世界 | MP2 的程式裡 |
|---|---|---|
| 0. 連續訊號 x_c(t) | 麥克風輸出的電壓 | 波形的數學式，例如 `A·sin(2π·f·t)`：任何 t 都算得出值 |
| 1. 取樣 x[n] = x_c(nT) | 每隔 T = 1/f_s 秒量一次 | `for (n = 0; n < N; n++) { t = (double)n / fs; x = wave(t); … }` |
| 2. 量化 Q(x) | 歸到 2^m 個等級中最近的一個 | `q = rint(x * full_scale);`（full_scale = 2^(m−1) − 1，所以階距 Δ = 1/full_scale） |
| 3. 編碼 | 每個等級寫成 m bits 的二進位 | 把整數 q 以 little-endian 寫進檔案（8-bit 要 +128） |
| 包裝 | — | 前面加上 44 bytes 的 WAV 檔頭（2.6） |
| 評估品質 | 量不到：真實世界沒有「量化前的真值」 | **程式裡兩個都有**：e = q/full_scale − x，所以能照定義算出 SQNR（2.4 的 (b)） |

最後一列是模擬最有價值的地方：真的錄音時，你永遠不知道量化前的精確值；但在程式裡 x 與 Q(x) 都在手上，
可以**實際量出** bit depth、振幅與 SQNR 的關係，親手驗證「每多 1 bit 多 6 dB」「音量減半少 6 dB」。
建議寫完之後自己做這幾個實驗（用 [a2d_steps.html](slides/a2d_steps.html) 先預測結果，再用你的程式驗證）：

| 實驗 | 怎麼做 | 應該看到 |
|---|---|---|
| bit depth | 參數 `8000 bits 1 sine 440 1.0 0.5`，其中 bits 用 8、16、32 | 參考實作：51.0、98.2、194.3 dB。每多 8 bits 多約 48 dB（6.02 × 8） |
| 振幅 | 16 bits，amp 用 1.0、0.5、0.25 | 98.2、92.9、85.2 dB：每減半**大約**少 6 dB（不會剛好，因為量化誤差不是真的隨機）；檔案大小完全不變 |
| 波形 | 16 bits、amp 0.8，換四種波形 | sine 96.7、square 96.3、triangle 94.4、sawtooth 94.4 dB：波形不同，訊號功率與誤差的分布都不同。**思考題**：square 且 amp = 1.0 時參考實作印出 `SQNR = inf dB`，為什麼？你的 C 程式這時候會怎樣？（±1 × 32767 剛好是整數，量化誤差全是 0） |
| aliasing | fs = 8000，freq 用 3000 與 5000 | 參考實作會拒絕 freq ≥ fs/2；把檢查拿掉，兩個 WAV 聽起來是同一個音高（2.2） |
| 資料量 | 把 fs、bits、channels 各自加倍 | 8,044 → 16,044 bytes：data 區剛好加倍，檔頭固定 44 bytes。就是 1.1 的公式 |

規格與細節：MP2 要你用 C 做：**產生波形 → 量化 → 算 SQNR → 寫成 WAV**。
規格見 [samples_2025-C/mini_project_2/README.md](../../samples_2025-C/mini_project_2/README.md) 連到的作業規定；
今年的命令列與 Python 參考實作 [wavegen.py](../../samples_2025-python/mini_project_2/wavegen.py) 完全相同：

```
./mp2 fs bits channels kind freq amp seconds out.wav
      fs        取樣率（Hz）               bits      8／16／32
      channels  1／2                       kind      sine／square／sawtooth／triangle
      freq      波形頻率（Hz，要小於 fs/2） amp       振幅 0.0–1.0（相對於滿刻度）
      seconds   長度（秒）                  out.wav   輸出檔；SQNR 印在螢幕上：SQNR = 49.123456 dB
```

先看正確答案長什麼樣（需要 numpy：`pip install numpy`）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../../../samples_2025-python/mini_project_2/wavegen.py 8000 16 1 sine 440 0.8 0.5 ref.wav` | `python ..\..\..\samples_2025-python\mini_project_2\wavegen.py 8000 16 1 sine 440 0.8 0.5 ref.wav` |
| `python3 wav_info.py ref.wav` | `python wav_info.py ref.wav` |

**自動測試比兩件事：你的 WAV 與參考的 WAV 逐 byte 相同、SQNR 在容差內。** 所以下面每一點都會決定過不過：

| 重點 | 說明 |
|---|---|
| 檔頭 | 就是 2.6 那 44 bytes，欄位一個都不能錯；`ChunkSize`、`Subchunk2Size` 要照實際資料量算 |
| 波形 | t = n ÷ fs，n = 0 … round(seconds × fs) − 1。四種波形的式子看 wavegen.py 的 `waveform()`；「題目表格裡列的波形都要有」 |
| 量化 | 滿刻度 full_scale：8 bits → 127、16 bits → 32767、32 bits → 2147483647；`q = rint(x × full_scale)` |
| 捨入 | 參考用的是 **round half to even**（0.5 捨入到最近的偶數）。C 的 `rint()` 預設就是這個；`round()` 是四捨五入，碰到剛好 .5 的值會差 1 → WAV 就不同了 |
| 8-bit | 存成無號數：寫檔前要 **+128**；16、32 bits 直接寫二補數，little-endian |
| 雙聲道 | 同一個值寫兩次（L、R 相同） |
| SQNR | 照 2.4 (b) 的定義：x 是量化前的值，e = q ÷ full_scale − x；用 `double` 累加，不要用 `float` |
| 極值 | amp = 1.0 時 x × full_scale 剛好到上限，確認你的整數型別放得下（去年 95 分樣本的評語） |
| 寫檔 | `fopen(path, "wb")`；Windows 用文字模式會把 `0A` 變成 `0D 0A`，WAV 就壞了 |

去年的扣分原因幾乎都在上表：SQNR 算錯（分母用錯、用了量化後的訊號當分子）、振幅大小錯、少了某種波形、SQNR 沒有照規定輸出。
本機自己測：在你的作業 repo 裡打 `bash ../mmsp2026/tools/ci/run_tests.sh mp2`（做法同第 2 週 2.4）。**MP1–MP5 統一截止 10/23（五）18:00。**

---

## 第三節｜分組與 Team 1 說明

### 3.1 分組與角色（10 分鐘）

- 全班 41 人分成 **11 組**：8 組四人、3 組三人。名單與角色現場公告（投影）。三輪 Team Project 每次重新排組。
- 每個人三輪各擔任一次 **P（口頭報告）、D（展示整合）、V（測試驗證）**。Team 1 四人組是 P＋P＋D＋V，三人組是 P＋D＋V。
- 同組組員可以互換角色，**今天下課前向老師登記**；之後兩輪的角色會依此排定，已經做過的角色不會再排到。
- 角色決定的是「評測當天誰負責什麼」，不是「只有那個人寫那部分」。**每個人都要有 C 程式的 commit**；
  個人口試（佔個人分數的一半）會問你自己寫的部分，也會問隊友寫的部分。
- 今天下課前每組要完成：互留聯絡方式、決定誰建 team repo、約好第一次討論的時間。

建 team repo（一組一個，由一位組員建立）：

1. GitHub 上建立 **private** repo，名稱 `mmsp2026-team1-gXX`（XX 是組別，例如 `g03`）。
2. Settings → Collaborators 加入所有組員、老師與助教。
3. 把 [starter/](../../team_projects/team1_textlink/starter/) 資料夾**裡面的內容**複製到 repo 根目錄，做第一個 commit。
4. 依規格補上必備檔案：`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`（先建空檔，之後每週更新）。

### 3.2 Team 1 要做什麼：規格導讀（15 分鐘）

完整規格在 [team_projects/team1_textlink/README.md](../../team_projects/team1_textlink/README.md)，**以它為準**；這裡帶你抓重點。

**一句話**：做一支叫 `textlink` 的程式，兩台電腦用 TCP 互傳文字、大文字檔與 WAV 檔；可以選擇原樣送，或先用 Huffman 壓縮再送；
收端還原後必須與原檔**逐 byte 相同**。最後用數字回答：**壓縮率是多少？壓縮之後有沒有比較快？**

| 功能 | 內容 | 用到今天的哪一段 |
|---|---|---|
| 1. 文字聊天 | 任何 UTF-8 字元（1–4 bytes、emoji）；長度前綴；`--raw`／`--huff` 可切換；收到的文字要檢查是合法 UTF-8 | 第 2 週 UTF-8；1.6、1.7 |
| 2. 大文字檔 | ≥ 1 MB；**以 UTF-8 字元為符號**做 Huffman；還原後逐 byte 相同（含 BOM、CRLF） | 1.3–1.7 |
| 3. WAV 檔 | ≥ 1 MB 的 16-bit PCM；**以 16-bit sample 為符號**：解析 RIFF 找到 data 區、對 sample 值做 histogram 再編碼；檔頭原樣保留 | 2.6、2.7 |

其他檔案，或內容與副檔名不符（`.txt` 不是合法 UTF-8、`.wav` 不是 16-bit PCM）時，退回以 byte 為符號，保證任何檔案都能還原。

**固定的部分（各組相同，評測照這個跑）：**

```
textlink chat server <port> [--bind <ip>] [--raw|--huff]      textlink recv <port> <outdir> [--bind <ip>]
textlink chat client <ip> <port> [--raw|--huff]               textlink send <ip> <port> <file> [--raw|--huff]
```

- **IP 與 port 一律由命令列指定，不可以寫死。評測是兩台不同的電腦互連**，只能在 `127.0.0.1` 跑通不算過。
- 聊天畫面裡也要能傳檔：`/files` 列出 `.txt` 與 `.wav`、`/send <編號或路徑>` 傳給對方，對方自動接收並存到 `received/`。
- 封包外框固定（見 3.3）；`send`／`recv` 結束前各在 stderr 印一行 `STATS …`，含 `sym`、`wire_bytes`、`ratio`、`encode_ms`／`decode_ms`，
  老師的腳本會直接讀它。
- 各組自訂的部分（codebook 怎麼存、FILE_BEGIN 裡放什麼）要寫進 `docs/interface.md`，寫到別組只看文件就能寫出互通的程式。

**報告要回答的問題**（規格的「壓縮率」與「量測」兩節）：每個測試檔的 N、K、H、L、codebook 大小與實際壓縮率；
以字元（或 sample）為符號，比以 byte 為符號好多少；WAV 的 codebook 吃掉了多少；本機與兩台電腦之間，壓縮比較快還是比較慢；
網路慢到多少 Mbps 以下壓縮才划算（損益平衡頻寬）。**「壓縮反而比較慢」是正常而且很可能的結果，照實報告。**

**最低驗收**：評測時老師用沒公布過的檔案，每一項都是「過／不過」，包括：跨機器連線、1–4 bytes 字元的聊天、半包與黏包、
文字檔與 WAV 的 raw／huff 傳輸逐 byte 相同、各種邊界檔案（空檔、單一符號、帶 BOM 與 CRLF 的文字、data 前有其他 chunk 的 WAV、8-bit WAV）、
壞輸入不當機（length 為 0 或超大、傳到一半斷線、codebook 被改壞、非法 UTF-8）、`STATS` 與結束碼。

時間：**10/11（日）18:00 前登錄 repo URL 與 commit SHA；10/12 評測**（報告＋展示＋口試）。9/28 停課，實際只有三週。

### 3.3 長度前綴：把上週的黏包問題解掉（10 分鐘）

上週的實驗：5 則訊息連續 `send`，對方只看到一顆泡泡。原因是 **TCP 是位元組串流，沒有「一則訊息」的概念**。
解法是每則訊息前面先講「我有多長」。Team 1 規定的外框（frame）：

```
+----------------------+-----------+----------------------+
| length：4 bytes      | type：1   | payload：length−1    |
| big-endian，無號整數  | byte      | bytes                |
+----------------------+-----------+----------------------+
length = type 與 payload 的總 bytes 數；合法範圍 1 到 16,777,216（16 MiB）
例：type = 0x01（TEXT_RAW）、payload = "abc" → 00 00 00 04 01 61 62 63
```

接收端永遠做同樣的兩步：**先收滿 5 bytes 的標頭 → 算出 payload 長度 → 再收滿那麼多 bytes**。
「收滿 n bytes」要自己寫迴圈（`recv_all`），因為 `recv` 一次可能只給你 1 byte（半包），也可能給你兩則（黏包）；
多的留在 TCP 緩衝區裡，下一次再拿。三個一定要記得的細節：

1. **位元組順序**：規格是 big-endian。`00 00 00 04` 要用位移組回來：`(b0<<24)|(b1<<16)|(b2<<8)|b3`；不要 `memcpy` 到 `int`
   ——那是「這台電腦的順序」，Windows 與 Mac 互連、或換一種 CPU 就錯。（對照 2.6：WAV 是 little-endian。）
2. **length 是對方說的，不能相信**：收到 0、或 `FF FF FF FF`（4 GB）時要拒絕並關閉連線，**絕對不可以照著它去 `malloc`**。
3. `send` 也一樣可能只送出一部分，要有 `send_all`。

用 [chunk_send.py](examples/chunk_send.py) 故意搗蛋，測你們的接收端（先開你們的 `textlink chat server 5000`）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 chunk_send.py 127.0.0.1 5000` | `python chunk_send.py 127.0.0.1 5000` |

它先把 5 個 frame 黏在一起一次送出，再把 1 個含 emoji 的 frame 切成 1 byte 1 byte 送。對方應該看到 5 則＋1 則，內容完整。
上週的 chat.c 做不到；你們的 frame 層做對了就可以。V 角色請從它開始，繼續加更多搗蛋的送法。

### 3.4 starter：會動的殼＋五個 place holder（10 分鐘）

[team_projects/team1_textlink/starter/](../../team_projects/team1_textlink/starter/) 是從上週的 chat.c 改寫延伸、
**已經符合規格的命令列、跨機器連線、聊天畫面、檔案傳輸流程與 `STATS`** 的 C 程式；「編碼」的部分全部挖空，留給你們寫：

| # | 檔案 | 函式 | 要做什麼 |
|---|---|---|---|
| TODO 1、2 | `src/frame.c` | `frame_pack_header`、`frame_parse_header` | 3.3 的標頭打包與解析（約 10 行） |
| TODO 3 | `src/utf8.c` | `utf8_validate` | 第 2 週的 RFC 3629 合法性檢查 |
| TODO 4、5 | `src/huffman.c` | `huff_encode`、`huff_decode` | 今天第一節＋符號的定義（字元／sample／byte） |

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 到 starter | `cd ../../../team_projects/team1_textlink/starter` | `cd ..\..\..\team_projects\team1_textlink\starter` |
| 編譯 | `make` | `mingw32-make` |
| 離線測試 | `make test` | `mingw32-make test` |
| 試跑 | `./textlink chat server 5000` | `.\textlink.exe chat server 5000` |

- `make test` 不需要網路，直接測五個函式，每一項顯示 `PASS`／`FAIL`／`TODO`。現在是 **46 個 TODO**（make 顯示 `Error 1` 是正常的）；
  全部寫完是 82 個 PASS，其中包含「以字元／sample 為符號，真的比以 byte 為符號壓得小」。
- 現在執行聊天，畫面上方有一行黃字列出還沒做的項目；送訊息會顯示「沒有送出：frame 標頭尚未實作」，不會當掉。
- **建議順序**：TODO 1、2（做完 `/raw` 聊天與 `--raw` 傳檔就通了，馬上找隊友的電腦做一次跨機器連線，把防火牆與 Wi-Fi 的問題先解決）
  → TODO 3 → TODO 4、5（先只跑 `make test`，離線的 round-trip 全過了再接上網路；先做 byte，再加字元與 sample）。
- 殼的任何部分都可以改，也可以完全不用它，只要命令列、frame 外框與 `STATS` 符合規格。

**先看「做完的樣子」：純 Python 的完整版。** [python_ref/textlink.py](../../team_projects/team1_textlink/python_ref/textlink.py)
功能完整、只用標準函式庫、刻意用高階寫法（`Counter` 統計、`heapq` 建樹、字串切片解碼），和 MP 的 Python 參考實作是同一個角色：

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 到 python_ref | `cd ../python_ref` | `cd ..\python_ref` |
| 分析一個檔案 | `python3 textlink.py inspect ../../../lectures/wk03_0921_team1-kickoff/data/speech_osr_8k.wav` | `python textlink.py inspect ..\..\..\lectures\wk03_0921_team1-kickoff\data\speech_osr_8k.wav` |
| 看中間過程 | 同上，在 `inspect` 前面加 `--probe` | 同左 |

`inspect` 會列出以 sample 為符號與以 byte 為符號兩種情況下的 N、K、H、L、codebook 大小與壓縮率（對這個語音檔是 80.55% 對 83.45%），
就是報告要交的那張表。它的格式和 starter 相同，所以**可以當你們 C 程式的對手**：你們寫的 `send`，送給它的 `recv`，檔案逐 byte 相同才算對。
你們要交的仍然是 C；Python 的寫法在 C 裡沒有對應的東西，要對照的是「每一步的輸入與輸出」。

**建議時程**（規格裡有完整版）：

| 週 | 日期 | 目標 |
|---|---|---|
| 1 | 9/21–9/27 | 建 team repo；TODO 1、2；`--raw` 檔案傳輸逐 byte 還原；**兩台電腦連線成功**；`docs/interface.md` 初稿 |
| 2 | 9/28–10/4（停課） | 離線完成 Huffman（三種符號）並通過 `make test`；聊天改走 frame；TODO 3 |
| 3 | 10/5–10/11 | 接上傳輸（`--huff`）；壞輸入測試；量測、畫圖、投影片；**10/11 18:00 前登錄 SHA** |

### 3.5 收尾（5 分鐘）

- 跨機器連線常見的坑（規格「跨機器測試須知」）：Windows 防火牆要允許、**校園 Wi-Fi 常會隔離同網段的裝置，改用手機熱點**、
  WSL 當監聽端別台連不進來。**每週至少在兩台電腦之間實測一次**，不要等到評測前一天。
- 與 MP4 的關係：觀念相同、程式不同。可以互相討論演算法，但 **MP4 仍須各自獨立完成**；團隊程式沿用了誰的 MP4 程式要寫在 `CONTRIBUTIONS.md`。
- 下週 9/28 停課。下次上課 10/5：定長編碼與 Huffman 的 C 實作（bit writer／reader、codebook），對應 MP3、MP4。

## 範例程式（examples/、slides/、data/）

| 檔案 | 用途 | 執行 |
|---|---|---|
| [commands.md](commands.md) | **上課跟著打的指令（一頁版）**：依上課順序、兩種作業系統各一欄，含常見錯誤訊息的處理 | 上課時開著 |
| [slides/media_size.html](slides/media_size.html) | 未壓縮資料量與位元率的計算機：文字、音訊、圖像、影像四個分頁（圖解、範例、即時算式、與壓縮後的對照），另有「常見格式」與「壓縮方法」兩個分頁 | 瀏覽器開啟 |
| [slides/huffman_steps.html](slides/huffman_steps.html) | Huffman 建樹的逐步動畫：樹由下往上長、佇列同步更新；可輸入任何文字或頻率表 | 瀏覽器開啟；鍵盤 ← → |
| [slides/a2d_steps.html](slides/a2d_steps.html) | A/D 四步驟的互動動畫：取樣 → 量化 → 編碼；可調頻率、取樣率、bit depth、振幅；會畫出 aliasing 的低頻弦波、量化誤差與即時 SQNR | 瀏覽器開啟；鍵盤 ← → |
| [slides/wav_bmp_viewer.html](slides/wav_bmp_viewer.html) | 打開真實的 WAV 與 BMP：播放、顯示、檔頭逐 byte 解讀、用公式驗算大小；換取樣率／bit depth／解析度再聽、再看；可選自己的檔案 | 瀏覽器開啟 |
| [slides/samples.js](slides/samples.js)、[make_samples_js.py](examples/make_samples_js.py) | 網頁內建的真實檔案（data/ 裡三個檔的 base64）與產生它的腳本 | `python3 examples/make_samples_js.py`（從本週資料夾） |
| [huffman_trace.c](examples/huffman_trace.c) | 在終端機印出每一輪的排序、佇列與節點陣列（記憶體）；`--step` 逐步執行 | `make trace`／`mingw32-make trace` |
| [huffman_demo.py](examples/huffman_demo.py) | 同一件事的 Python 精簡版；`--bytes` 改以 byte 為符號、`--freq` 給頻率表 | `python3 huffman_demo.py "字串"` |
| [entropy.c](examples/entropy.c)、[entropy.py](examples/entropy.py) | 算一個檔案以 byte、以 UTF-8 字元為符號的熵；兩版輸出逐 byte 相同 | `make check`／`mingw32-make check` |
| [alias_demo.py](examples/alias_demo.py) | 3000 Hz 與 5000 Hz 以 8 kHz 取樣後完全相同；寫出兩個 WAV 用耳朵聽 | `python3 alias_demo.py` |
| [quantize_demo.py](examples/quantize_demo.py) | 2–16 bits 量化的 SQNR 與兩條公式對照；寫出 16／8／4-bit 的 WAV | `python3 quantize_demo.py [振幅]` |
| [wav_info.py](examples/wav_info.py) | 逐 chunk 解讀 WAV、印出 fmt 各欄位、sample 值的 histogram 與熵 | `python3 wav_info.py 檔案.wav` |
| [chunk_send.py](examples/chunk_send.py) | 對 TextLink 送黏包與 1 byte 1 byte 的半包，測接收端 | `python3 chunk_send.py IP port` |
| [python_ref/textlink.py](../../team_projects/team1_textlink/python_ref/textlink.py) | Team 1 的純 Python 完整版：`inspect` 分析檔案、`--probe` 看中間過程、可與 C 版互連 | `python3 textlink.py inspect 檔案` |
| [data/speech_osr_8k.wav](data/speech_osr_8k.wav) | 真實語音，8 kHz、16-bit、單聲道，33.6 秒 | 給 wav_info 與 hex 工具用 |
| [data/music_sousa_44k.wav](data/music_sousa_44k.wav) | 真實音樂（管樂進行曲），44.1 kHz、16-bit、單聲道，6 秒 | 取樣率的差別要寬頻的聲音才聽得出來 |
| [data/earth_256.bmp](data/earth_256.bmp) | 真實照片，256×256、24-bit 未壓縮 BMP，196,662 bytes | 給檢視器與 hex 工具用 |

Python 的示範程式只用標準函式庫，不需要安裝任何套件（MP2 的參考實作 wavegen.py 才需要 numpy）。

## 回家作業／下週前要做的事

1. **手算一次 Huffman**：對 `MISSISSIPPI RIVER`（含空白，共 17 個字元）做頻率表、建樹、寫出 code，算 H、L，檢查 H ≤ L < H + 1；
   用 `huffman_trace` 或動畫對答案（平手時的選擇不同，code 可能不同，但 L 要一樣）。
2. **讀懂 `huffman_trace.c`**：說得出 `node[]` 與 `queue[]` 各存什麼、`queue_insert` 為什麼能維持排序、code 是怎麼由 `parent` 走出來的。
3. **MP2**：先跑 wavegen.py 看正確輸出，用 hex 工具對照 2.6 的表逐欄讀自己產生的檔頭；再開始寫 C。繼續 MP1。
4. **Team 1 第 1 週目標**（見 3.4 的時程）：建 repo、完成 TODO 1、2、**兩台電腦連線成功**，用 `chunk_send.py` 確認半包與黏包都收得對。
5. 每組在 `TEAM_LOG.md` 記下第一次討論的日期、出席者與分工。

## 素材來源與授權

本週放進 repo 的第三方素材只有下列三個檔案（以及由它們產生的 `slides/samples.js`），都可以自由散布；詳見 [data/README.md](data/README.md)。

| 檔案 | 來源 | 授權 |
|---|---|---|
| `data/speech_osr_8k.wav` | **Open Speech Repository**，American English，`OSR_us_000_0010_8k.wav`，<https://www.voiptroubleshooter.com/open_speech/> | 網站聲明可自由用於測試、研究、開發等用途，可複製、修改、納入網站；**條件是註明來源為「Open Speech Repository」** |
| `data/music_sousa_44k.wav` | John Philip Sousa〈Comrades of the Legion〉（1920），"The President's Own" United States Marine Band 演奏；取自 [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:John_Philip_Sousa_-_Comrades_of_the_Legion.ogg)，截取第 20–26 秒並轉成單聲道 WAV | 公有領域（美國聯邦政府機構的作品） |
| `data/earth_256.bmp` | The Blue Marble，NASA／Apollo 17 任務組員攝於 1972 年（AS17-148-22727）；取自 [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:The_Earth_seen_from_Apollo_17.jpg)，縮成 256×256 並轉成 BMP | 公有領域（NASA 的作品） |

講義中的數學式、RLE 與 8 色影像的算例、未壓縮檔案大小的算例，整理自下列教科書的投影片，文字為重新撰寫；
取樣與量化的敘述另整理自授課教師自己的兩份講義。各互動網頁中「壓縮後大約多大」與各格式的位元率是常見設定下的典型值，不是規格書上的數字。

## 參考

編號的文獻都逐筆查證過（2026/9/21）：有 DOI 的，作者、刊名、卷期、頁碼與 [Crossref](https://www.crossref.org/) 的登錄資料一致，點 DOI 連結就能核對；
標準與規格則連到發布單位自己的網頁（RFC Editor、ITU、W3C、Unicode、Microsoft）。部分論文全文要從校內網路或圖書館帳號才能下載。

**教科書**

- <a id="ref-5"></a>[5] T. M. Cover and J. A. Thomas, *Elements of Information Theory*, 2nd ed., Wiley-Interscience, 2006. ISBN 978-0-471-24195-9. [doi:10.1002/047174882X](https://doi.org/10.1002/047174882X)
  ——第 2 章（熵）、第 5 章 Data Compression（prefix code、Kraft 不等式、H ≤ L < H + 1、Huffman code 的最佳性）。本講義 1.3、1.4、1.6。
- <a id="ref-6"></a>[6] K. Sayood, *Introduction to Data Compression*, 5th ed., Morgan Kaufmann, 2017. ISBN 978-0-12-809474-7. [doi:10.1016/C2015-0-06248-7](https://doi.org/10.1016/C2015-0-06248-7)
  ——Huffman coding、字典式壓縮（LZ77／LZW）、量化、各種影音壓縮標準的入門。本講義 1.1、1.6、1.7。
- <a id="ref-7"></a>[7] J. Burg, *The Science of Digital Media*, Pearson Prentice Hall, 2009. ISBN 978-0-13-243580-2.
  ——第 1 章 Digital Data Representation and Communication 的 Analog to Digital Conversion、Data Storage、Compression Methods 三節；
  本講義 1.1–1.5、2.2–2.5 的數學式、RLE 與 8 色影像的算例出自這裡。

**原始論文**

- <a id="ref-1"></a>[1] C. E. Shannon, "A Mathematical Theory of Communication," *Bell System Technical Journal*, vol. 27, no. 3, pp. 379–423, July 1948, [doi:10.1002/j.1538-7305.1948.tb01338.x](https://doi.org/10.1002/j.1538-7305.1948.tb01338.x)；
  vol. 27, no. 4, pp. 623–656, Oct. 1948, [doi:10.1002/j.1538-7305.1948.tb00917.x](https://doi.org/10.1002/j.1538-7305.1948.tb00917.x)。——熵的定義（1.3）。
- <a id="ref-2"></a>[2] R. M. Fano, "The Transmission of Information," Technical Report No. 65, Research Laboratory of Electronics, MIT, Mar. 1949.
  掃描檔：<https://archive.org/details/fano-tr65.7z>。——Shannon-Fano coding（1.5）。
- <a id="ref-3"></a>[3] D. A. Huffman, "A Method for the Construction of Minimum-Redundancy Codes," *Proceedings of the IRE*, vol. 40, no. 9, pp. 1098–1101, Sept. 1952.
  [doi:10.1109/JRPROC.1952.273898](https://doi.org/10.1109/JRPROC.1952.273898)。——Huffman coding（1.6）；只有四頁，值得讀原文。
- <a id="ref-4"></a>[4] G. Stix, "Encoding the 'Neatness' of Ones and Zeroes"（Profile: David A. Huffman）, *Scientific American*, vol. 265, no. 3, pp. 54–58, Sept. 1991.
  [doi:10.1038/scientificamerican0991-54](https://doi.org/10.1038/scientificamerican0991-54)。——1951 年期末報告那段故事的出處（1.6）。
- <a id="ref-8"></a>[8] H. Nyquist, "Certain Topics in Telegraph Transmission Theory," *Transactions of the AIEE*, vol. 47, no. 2, pp. 617–644, Apr. 1928.
  [doi:10.1109/T-AIEE.1928.5055024](https://doi.org/10.1109/T-AIEE.1928.5055024)。
- <a id="ref-9"></a>[9] C. E. Shannon, "Communication in the Presence of Noise," *Proceedings of the IRE*, vol. 37, no. 1, pp. 10–21, Jan. 1949.
  [doi:10.1109/JRPROC.1949.232969](https://doi.org/10.1109/JRPROC.1949.232969)。——取樣定理（2.2）。
- <a id="ref-10"></a>[10] W. R. Bennett, "Spectra of Quantized Signals," *Bell System Technical Journal*, vol. 27, no. 3, pp. 446–472, July 1948.
  [doi:10.1002/j.1538-7305.1948.tb01340.x](https://doi.org/10.1002/j.1538-7305.1948.tb01340.x)。——量化雜訊模型（2.3、2.4）。
- <a id="ref-11"></a>[11] R. M. Gray and D. L. Neuhoff, "Quantization," *IEEE Transactions on Information Theory*, vol. 44, no. 6, pp. 2325–2383, 1998.
  [doi:10.1109/18.720541](https://doi.org/10.1109/18.720541)。——量化理論的回顧（2.3、2.4）。
- <a id="ref-12"></a>[12] 1.1 提到的字典式壓縮與 JPEG：
  J. Ziv and A. Lempel, "A Universal Algorithm for Sequential Data Compression," *IEEE Trans. Information Theory*, vol. 23, no. 3, pp. 337–343, 1977, [doi:10.1109/TIT.1977.1055714](https://doi.org/10.1109/TIT.1977.1055714)（LZ77）；
  T. A. Welch, "A Technique for High-Performance Data Compression," *Computer*, vol. 17, no. 6, pp. 8–19, June 1984, [doi:10.1109/MC.1984.1659158](https://doi.org/10.1109/MC.1984.1659158)（LZW）；
  G. K. Wallace, "The JPEG Still Picture Compression Standard," *Communications of the ACM*, vol. 34, no. 4, pp. 30–44, Apr. 1991, [doi:10.1145/103085.103089](https://doi.org/10.1145/103085.103089)。

**檔案格式、標準與規格**

- <a id="ref-13"></a>[13] P. Kabal, "Wave File Specifications," McGill University：<https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html>
  ——WAV 各欄位的說明，並收錄原始規格 IBM／Microsoft *Multimedia Programming Interface and Data Specifications 1.0*（1991）。本講義 2.6。
- [14] Microsoft Learn, "BITMAPINFOHEADER structure (wingdi.h)"：<https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader>——BMP 檔頭（2.6 的檢視器）。
- <a id="ref-15"></a>[15] ITU-T Rec. G.711, "Pulse code modulation (PCM) of voice frequencies," Nov. 1988：<https://www.itu.int/rec/T-REC-G.711>——電話的 8 kHz、8-bit A-law／μ-law（1.1、2.3）。
- [16] ITU-T Rec. T.81, "Information technology – Digital compression and coding of continuous-tone still images – Requirements and guidelines," Sept. 1992：<https://www.itu.int/rec/T-REC-T.81>（JPEG）；
  ITU-T Rec. H.264, "Advanced video coding for generic audiovisual services," 第一版 May 2003：<https://www.itu.int/rec/T-REC-H.264>。
- [17] IETF RFC：[RFC 1951](https://www.rfc-editor.org/rfc/rfc1951)（DEFLATE，＝LZ77＋Huffman）、[RFC 9639](https://www.rfc-editor.org/rfc/rfc9639)（FLAC）、[RFC 6716](https://www.rfc-editor.org/rfc/rfc6716)（Opus）、
  [RFC 3629](https://www.rfc-editor.org/rfc/rfc3629)（UTF-8）、[RFC 9293](https://www.rfc-editor.org/rfc/rfc9293)（TCP：byte stream，沒有訊息邊界，3.3）；
  W3C, *Portable Network Graphics (PNG) Specification (Third Edition)*：<https://www.w3.org/TR/png-3/>。

**授課教師的講義與延伸閱讀**

- 江振宇，DSP 2026 課堂講義〈第三部分：語音信號的表示（補充教材）〉：
  <https://github.com/cychiang-ntpu/dsp2026/blob/master/docs/lectures/dsp2026_lecture_notes.md>（本講義 2.1–2.3 關於麥克風、ADC 四步驟、量化數學式與 DAC 的敘述出處）。
- 2023 講義：[Chapter 2 Digital Data (Signal) Representation](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023/tree/master/Chapter-2)
  （A/D 的投影片 SpeechA2D、理想取樣的數學、量化與 SQNR；含上課影片連結）。
- 維基百科（入門用，不是引用依據）：[WAV](https://zh.wikipedia.org/wiki/WAV)、[A-law](https://en.wikipedia.org/wiki/A-law_algorithm)、[μ-law](https://en.wikipedia.org/wiki/%CE%9C-law_algorithm)。
- 本 repo：[Team 1 規格](../../team_projects/team1_textlink/README.md)、[starter/README.md](../../team_projects/team1_textlink/starter/README.md)、
  [MP2 樣本與評語](../../samples_2025-C/mini_project_2/README.md)、[MP4 樣本](../../samples_2025-C/mini_project_4/README.md)。
