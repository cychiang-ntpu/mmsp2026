## m8_mulaw.c：μ-law 壓成 8 bits 再解回來，SQNR 多少？

[← 教學總覽](../TUTORIAL.md)｜上一個：[m7 環形 buffer](../m7_ringbuffer/README.md)｜下一個：[m9 ADPCM](../m9_adpcm/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m8_mulaw` | `cd team_projects\team2_voicelink\tutorial\m8_mulaw` |
| `make`（編出 `./m8_mulaw`（Windows `.\m8_mulaw.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |
| `S=../../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `$S = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav"` |

程式：[`m8_mulaw.c`](m8_mulaw.c)（75 行，每一行都有註解）。原理的圖文版在第 9 週 [audio_coding.md](../../../../lectures/wk09_1102_midterm-speech/audio_coding.md)，Python 對照在 [mulaw_demo.py](../../../../lectures/wk09_1102_midterm-speech/examples/mulaw_demo.py)。

### 這個模組的目的

- **做什麼**：把一個 WAV 用 μ-law 壓成 8 bits／sample 再解回來，算 bytes 比例與 SQNR。
- **為什麼要先做它**：功能 4 的第一個 codec（C1，必做 15 分）。先在檔案上把編碼表、解碼表、SQNR 做對，step6 只是把它套進 send（編）與 recv（解）；接下來的 m9 ADPCM 與你們自己寫的 Huffman，量 SQNR 的方法也是這一支。
- **在 Part B 的哪裡用到**：step6 的 `mulaw_init`／`mulaw_encode`／`mulaw_decode`。
- **做完要能回答**：bytes 為什麼剛好一半？SQNR 37 dB 是什麼意思？為什麼小聲的地方 μ-law 比線性 8-bit 清楚？

---

### 1. 先想一個問題：16 bits 砍成 8 bits，怎麼砍？

一個 sample 是 16-bit（−32768 … 32767）。最笨的砍法是**線性**：直接丟掉低 8 bits，只留高 8 bits（256 階、每階 256）。
試試看會怎樣：大聲的地方（sample 幾千、幾萬）256 的誤差聽不出來；但是語音大部分時間是**小聲**的（sample 幾十、幾百），
誤差 256 比訊號本身還大，整段變成沙沙聲。

μ-law 的想法：**小聲的地方刻度密、大聲的地方刻度疏**。像一把對數刻度的尺，0 附近一格很細、越遠一格越寬。
這樣小聲時誤差小（聽得清楚），大聲時誤差大但相對於訊號還是小（耳朵本來就是聽「比例」的）。

### 2. 公式（三步）

把 sample 正規化成 x = s / 32768（落在 −1 … 1），μ = 255：

| 步驟 | 公式 | 做什麼 |
|---|---|---|
| 壓縮 | y = sign(x) · ln(1 + μ·\|x\|) / ln(1 + μ) | 把 −1…1 「擠」成另一個 −1…1，但 0 附近被拉開、兩端被壓扁 |
| 量化 | q = round(y × 128)，夾在 −128…127；存成 byte = q + 128 | 在「擠過」的軸上均勻切 256 階 |
| 展開 | x' = sign(q) · ((1 + μ)^(\|q\|/128) − 1) / μ；s' = round(x' × 32768) | 解碼端反過來算回 16-bit |

**為什麼先壓縮再均勻量化等於「對數刻度的尺」**：均勻切 y 軸，對應回 x 軸就是不均勻的格子——0 附近密、兩端疏。
自己代兩個數驗證：x = 0.01 → y ≈ 0.23（佔了 23% 的刻度！）；x = 0.5 → y ≈ 0.88。小聲的 1% 振幅拿到了將近四分之一的碼。

### 3. 程式怎麼做：查表，不要每個 sample 都算 log

`log`、`pow` 很慢，而且每秒有 16000 個 sample。但 16-bit 只有 65536 種可能的輸入、byte 只有 256 種——所以程式開頭先把答案全算好：

```c
static uint8_t enc[65536];   /* 索引 = sample + 32768（把 −32768…32767 平移到 0…65535）*/
static int16_t dec[256];
```

`mulaw_init()` 兩個迴圈把兩張表填滿（約 1 ms），之後：

```c
bytes[i] = enc[w.pcm[i] + 32768];   /* 編碼 = 查一次表 */
back[i]  = dec[bytes[i]];           /* 解碼 = 查一次表 */
```

step6 原封不動用這兩張表；live 模式每 20 ms 要編 320 個 sample，查表一瞬間就完成。

### 4. 跑一次，看三個數字

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m8_mulaw $S back.wav` | `.\m8_mulaw.exe $S back.wav` |

```
537970 個 sample：16-bit 是 1075940 bytes → μ-law 是 537970 bytes（比例 0.500）
前 5 個 sample：原始   -919  -1085  -1314  -1256  -1049
               μ-law     80     76     72     73     77（byte）
               解回    -900  -1094  -1325  -1264  -1042
SQNR 37.6 dB（規格 ≥ 30）OK
解回來的聲音存到 back.wav
```

- **比例 0.500**：每個 sample 2 bytes → 1 byte，一定剛好一半。到了 step6，一包 640 bytes → 320 bytes；但 header 的 12 bytes 不會縮，所以測試 8 看的是 (12+320)/(12+640) ≈ 0.509。
- **前 5 個 sample**：−919 編成 byte 80、解回 −900，誤差 19（約 2%）。注意 −1085 與 −1049 解回 −1094 與 −1042：相近的值被編成不同的 byte，刻度在這一帶大約 50 一格。
- **SQNR 37.6 dB**：訊號功率是誤差功率的 10^3.76 ≈ 5800 倍。一般說 30 dB 以上「聽不太出來」；規格門檻 30。

聽：用播放器開 `back.wav` 和原檔比，正常音量下幾乎分不出來。

### 5. 邊做邊學：三個實驗（每個 2 分鐘）

**實驗 A：把 μ 改成 1。** `#define MU 255.0` 改成 `1.0`、重編、重跑。
ln(1 + x)/ln 2 幾乎是直線，等於退化成**線性 8-bit**。SQNR 會掉到 20 dB 以下，聽 `back.wav` 的安靜段落有明顯沙沙聲。
這就是「為什麼要對數刻度」的答案。

**實驗 B：把音量除以 10 再編。** 在編碼那一行前面加 `int16_t s = w.pcm[i] / 10;` 然後用 `s` 查表、SQNR 也對 `s` 算。
μ-law 的 SQNR 幾乎不變（還是 30 多 dB）；同一件事對實驗 A 的線性版做，SQNR 會再掉 20 dB。
**結論：μ-law 的 SQNR 對音量不敏感**，這是它被電話系統（G.711）採用幾十年的原因。

**實驗 C：看一眼表。** 在 `mulaw_init` 後面加幾行印 `dec[128]、dec[129]、dec[130]`（0 附近）與 `dec[253]、dec[254]、dec[255]`（最大聲）：
0 附近相鄰兩格差只有幾個單位，最大聲那邊相鄰兩格差超過 1000。這就是「對數刻度的尺」長什麼樣。

### 6. 搬進 step6 要注意的事

1. `enc`／`dec` 兩張表、`mulaw_init()` 原封不動；程式開頭叫一次 `mulaw_init()`。
2. send：`--codec mulaw` 時每包 320 個 sample 查 `enc` → 320 bytes，封包 type 改成 0x04。
3. recv：看 type 是 0x04 就用 `dec` 解成 320 個 int16，**再**丟進 jitter buffer——PLC（repeat／zero）在解碼後的 16-bit 上做，所以 PLC 一行都不用改。
4. 測試 8 要 SQNR ≥ 30 dB、bytes 比例 ≈ 0.509（含 header）；`python_ref` 用的是同一條公式，所以你們的 send 對它的 recv、它的 send 對你們的 recv 都要通。

### 7. 做完要能回答（11/9 抽問）

1. bytes 為什麼剛好一半？header 有沒有變小？
2. SQNR 37 dB 是「訊號功率是誤差的幾倍」？
3. 為什麼小聲的地方 μ-law 比線性 8-bit 清楚？（提示：實驗 A、C）
4. 為什麼可以用查表取代算 log？表有幾格？

---

| 模組 | 零件 | 在 Part B 的哪裡 |
|---|---|---|
| m1 | wav.h 讀寫 | step1 的 `load_wav_16k_mono`、`wav_write` |
| m2 | udp.h 送收 | step1 的 `cmd_send`／`cmd_recv` |
| m3 | clock.h 絕對時刻 | step1 送端的節奏、step3 的播放時鐘 |
| m4、m5 | audio.h callback | step7 的 `audio_cb` |
| m6 | header 打包／解析 | step2 的 `pack_header`／`parse_header` |
| m7 | 環狀 buffer＋時鐘 | step3 的 `jb_put`／`jb_tick` |
| m8 | μ-law 查表 | step6 的 `mulaw_init`／`mulaw_encode`／`mulaw_decode` |
| m9 | ADPCM 編解碼 | 你們要自己加進 step7 的 `--codec adpcm`（C2） |
