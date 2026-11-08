## m9_adpcm.c：ADPCM 壓成 4 bits 再解回來——只送「差」、刻度自己調

[← 教學總覽](../TUTORIAL.md)｜上一個：[m8 μ-law](../m8_mulaw/README.md)｜下一個：[第 1 步 raw](../step1_raw/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m9_adpcm` | `cd team_projects\team2_voicelink\tutorial\m9_adpcm` |
| `make`（編出 `./m9_adpcm`（Windows `.\m9_adpcm.exe`）） | `mingw32-make` |
| `S=../../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `$S = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav"` |

程式：[`m9_adpcm.c`](m9_adpcm.c)（約 130 行，每一行都有註解）。原理的圖文版在第 9 週 [audio_coding.md](../../../../lectures/wk09_1102_midterm-speech/audio_coding.md)（DPCM 方塊圖），Python 對照是 [python_ref/voicelink.py](../../python_ref/voicelink.py) 的 `adpcm_encode`／`adpcm_decode`（**同一張表、同樣的初值**，所以互通）。

### 這個模組的目的

- **做什麼**：把一個 WAV 用 ADPCM 壓成 4 bits／sample（bytes 四分之一）再解回來，算 SQNR；可以逐 sample 印出編碼器在想什麼。
- **為什麼要先做它**：功能 4 的第二個 codec（C2，必做 15 分）。它和 μ-law 是**完全不同的想法**：μ-law 是「每個 sample 自己砍 bits」，ADPCM 是「利用相鄰 sample 很像、只送差」。這支程式把演算法在檔案上做對、讓你用 `--trace` 看到每一步；你們的作業是把它接進 step7 的封包（每包 320 個 sample 獨立編、type 0x05、過測試 9）。
- **在 Part B 的哪裡用到**：沒有現成的 step——這是你們要自己加進 step7 的 `--codec adpcm`。
- **做完要能回答**：為什麼預測要用「重建值」不用原始值？step 為什麼要自己調？每包開頭為什麼歸零、代價是什麼？

---

### 1. 三個字母，三個想法

ADPCM = **A**daptive **D**ifferential **PCM**。

| 字母 | 想法 | 沒有它會怎樣 |
|---|---|---|
| **D**（差分） | 語音相鄰兩個 sample 很像（16 kHz 下 1/16000 秒聲音變不了多少），所以不送 sample 本身，送「和預測值的差」d = s − pred。差比較小，用的 bits 比較少 | 就是普通 PCM，4 bits 只能切 16 階，整段都是雜音 |
| **PCM**（量化） | d 用 4 bits 量化：1 bit 正負 ＋ 3 bits 大小 | — |
| **A**（自適應） | 量化的刻度 step 不固定：剛才的差很大 → step 放大；很小 → step 縮小。兩邊用同一張表算，**step 不用送** | 實驗 B 會讓你聽到 |

**預測值**這裡用最簡單的：pred = 上一個 sample 的**重建值**。（真正的 G.726 用更複雜的預測器，想法一樣。）

### 2. 最重要的一句話：預測要用「重建值」，不是原始值

解碼端手上**只有**重建值（它沒有原始的 s）。如果編碼端偷用原始的 s 當下一個預測，兩邊的 pred 就不一樣，
每個 sample 的量化誤差會一路累積，幾百個 sample 後完全走掉。
所以編碼端裡面藏著一個解碼器：編完一個 4-bit 碼，馬上用它把自己的 pred 推到「解碼端會得到的值」。
程式裡就是 `adpcm_encode_one` 最後呼叫 `adpcm_reconstruct`——**編碼端與解碼端呼叫同一個函式**，兩邊永遠同步。

第 9 週的 DPCM 方塊圖（[dpcm_block.svg](../../../../lectures/wk09_1102_midterm-speech/figs/dpcm_block.svg)）畫的就是這件事：編碼端右下角那一塊和解碼端一模一樣。

### 3. 一個 sample 怎麼編（跟著 `--trace` 看）

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m9_adpcm $S back.wav --trace 8` | `.\m9_adpcm.exe $S back.wav --trace 8` |

```
     n  原始 s    預測  殘差 d   step    碼    重建   誤差
     0    -919       0    -919    724  -101    -995     76
     1   -1085    -995     -90    157  -010   -1092      7
     2   -1314   -1092    -222    143  -110   -1323      9
     3   -1256   -1323      67    253  +001   -1229    -27
     4   -1049   -1229     180    230  +011   -1029    -20
     5   -1024   -1029       5    209  +000   -1003    -21
     6   -1146   -1003    -143    190  -011   -1168     22
     7   -1167   -1168       1    173  +000   -1147    -20
```

一行一行讀（第 2 行，n = 1）：

1. **預測** −995：上一個 sample 的重建值（不是原始的 −919！）。
2. **殘差** d = −1085 − (−995) = −90。
3. **step** 157：目前的刻度。
4. **碼** `-010`：第 1 個字元是正負（d < 0 → `-`），後三位是「三把尺」量 |d| = 90 的結果：
   90 ≥ 157？否 → 0；90 ≥ 78（step/2）？是 → 1，剩 12；12 ≥ 39（step/4）？否 → 0。得到 `010`。
5. **重建**：delta = step/8 + step/2 = 19 + 78 = 97；pred = −995 − 97 = −1092。（step/8 是「半階修正」：量化區間的中點，不是下緣。）
6. **調 step**：碼的大小 `010` = 2，查 `ADPCM_INDEX[2] = −1` → index 從 32 變 31 → 下一個 step = 143（第 3 行的 step 欄）。
7. **誤差** −1085 − (−1092) = 7。這個碼只花了 4 bits。

第 1 行（n = 0）是每一包的開頭：pred 從 0 開始、step 用大的 724，一步跳到 −995（訊號附近）；第 2 個 sample 起 step 換成 157 再自己調。
之後看 step 欄：差大（第 3 行 −222，碼 110）step 就被推大（143 → 253）；差小（第 6 行 5，碼 000）step 就縮（209 → 190）。這就是 **A**。

### 4. 跑完整個檔案，看三個數字

```
537970 個 sample：16-bit 是 1075940 bytes → ADPCM 是 268985 bytes（比例 0.250）
每 320 個 sample 把預測器歸零一次（= Team 2 的一包）；step 自適應（89 格的表）
SQNR 25.2 dB（規格 ≥ 20）OK
```

- **比例 0.250**：每個 sample 4 bits = 半個 byte，兩個 sample 包成一個 byte（先到的在高 4 bits，規格這樣定）。到了 step7，一包 640 → 160 bytes；含 12 bytes header 是 172/652 ≈ 0.264（測試 9 看這個）。
- **SQNR 25 dB**：比 μ-law 的 37 dB 低 12 dB，但位元率只有它的一半（64 kb/s 對 128 kb/s）。這就是壓縮的取捨：報告的 codec 比較表要把這兩個數字並排。
- 7 秒的母音檔 `aeueo-16kHz.wav` 只有 21.4 dB——母音振幅大、變化快，差分比較不划算。**SQNR 和內容有關**，所以驗收用老師的語音檔。

### 5. 邊做邊學：四個實驗

**實驗 A：不歸零。** `--frame 0`：整個檔案連續編碼、預測器從頭到尾不重置。
SQNR 跳到約 35 dB（和 μ-law 差不多了！）。那為什麼 Team 2 要每包歸零？
因為網路會掉包：連續編碼時掉了一包，解碼端的 pred 和 step 就和編碼端對不上，之後**每一包**都是錯的，直到下一次重置。
每包歸零 = 一包只靠自己就能解，掉一包只壞一包（PLC 補一包就好）。代價是每包開頭幾個 sample 要從 0「追」上訊號，SQNR 少 10 dB。
**報告要寫這個取捨。**

**實驗 B：關掉自適應。** `--fixed-step 100`：step 永遠 100，變成普通 DPCM。
SQNR 掉到個位數——大聲的地方一步最多只能走 100 + 50 + 25 + 12 = 187，追不上訊號（叫 slope overload，聽起來像悶住）。
換 `--fixed-step 2000`：大聲追得上了，但小聲的地方最小一步 250，安靜段全是雜音（granular noise）。
**沒有一個固定的 step 兩邊都好**，所以 step 要自己調。

**實驗 C：換初值。** 把 `ADPCM_INDEX_FIRST` 從 48 改成 20（step 50）再跑老師的語音檔：每包開頭追不上訊號，SQNR 從 25 掉到 21 dB。
改成 70（step 7132）：SQNR 反而升到 29 dB——那為什麼不用 70？把同一個檔案的音量除以 30（−30 dB，小聲講話）再試：
48 還有 20 dB，70 只剩 −0.4 dB（開頭那一步跳太遠、整包都在震盪）。**沒有一個初值對大聲、小聲都最好**，48／32 是兩邊都 ≥ 20 dB 的折衷。
而且 **python_ref 用的就是這兩個數，你們的程式要互通就不能改**——這也是「編碼器與解碼器要事先約好的東西」（side information）的例子。

**實驗 D：故意用原始值預測。** 在 `adpcm_encode_one` 裡，`adpcm_reconstruct(st, nibble)` 之後加一行 `st->pred = s;`（編碼端偷看答案）。
重跑：編碼端自己覺得誤差很小，但解碼端（沒有這一行）解出來的 SQNR 崩掉。這就是第 2 節那句話的實證。做完記得改回來。

### 6. 搬進 step7（這才是 C2 的 15 分）

m9 是「檔案進、檔案出」；step7 要的是「每包 320 個 sample 進、160 bytes 出」。要做的事：

1. 把兩張表、`adpcm_state_t`、`adpcm_reconstruct`、`adpcm_encode_one` 搬進 step7（可以直接複製）。
2. 寫兩個 frame 版的函式：
   - `adpcm_encode_frame(const int16_t pcm[320], uint8_t out[160])`：`state_reset`、第 0 個 sample 後把 index 設成 `ADPCM_INDEX0`、320 次 `adpcm_encode_one`、兩個 nibble 包一個 byte（高 4 bits 先）。
   - `adpcm_decode_frame(const uint8_t in[160], int16_t pcm[320])`：同樣的歸零規則、320 次 `adpcm_reconstruct`。
   m9 的主迴圈裡 `i % frame == 0`／`== 1` 那兩行就是這個規則。
3. send：`--codec adpcm` 時 type = 0x05、payload 160 bytes；STATS 的 `codec=adpcm`、`bytes` 會是 pcm 的 0.264 倍。
4. recv：type 0x05 → `adpcm_decode_frame` 解成 320 個 int16 **再**丟進 jitter buffer；PLC 不用改。
5. 驗收：`python ../../tests/run_mock_tests.py "./voicelink send" "./voicelink recv" $S --only 9`，再和 python_ref 互跑（它的 send 對你們的 recv、反過來）。SQNR ≥ 20 dB、bytes 0.264 ± 2%、`--loss 0.05` 仍正常結束。

**常見錯誤**：
- 忘了第 2 個 sample 要把 index 換成 32（python_ref 有，你沒有 → 互通時 SQNR 崩）。
- nibble 包 byte 的順序反了（低 4 bits 先）→ 和 python_ref 互通時全是雜音。
- 解碼端用 `(int16_t)` 強制轉型前沒夾在 −32768…32767 → 爆音。`adpcm_reconstruct` 裡有 `clampi`，不要刪。
- 最後一包不足 320 個 sample：規格說補 0 再編（每包永遠 160 bytes）。

### 7. 做完要能回答（11/16 抽問、11/23 口試）

1. 為什麼預測要用重建值？（實驗 D）
2. step 為什麼要自己調？固定小 step 和固定大 step 各壞在哪？（實驗 B）
3. 每包歸零的好處與代價？（實驗 A）
4. 為什麼 step 不用送給對方？（兩邊用同一張表、看同一個碼）
5. ADPCM 的 SQNR 比 μ-law 低 12 dB，為什麼還要用它？（位元率一半——codec 比較表）
