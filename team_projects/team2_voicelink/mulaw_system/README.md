# mulaw_system/：μ-law 系統（你們寫）——在 codec 表裡加 μ-law，`--codec mulaw`，位元率減半

[← 教學總覽](../tutorial/TUTORIAL.md)｜[← 規格](../README.md)｜上一個：[tutorial 第 6 步 live](../tutorial/step6_live/README.md)｜下一個：[adpcm_huffman_system](../adpcm_huffman_system/README.md)

> 這個資料夾給的 [`voicelink.c`](voicelink.c) 是**框架**：tutorial 第 6 步的完整程式（raw PCM、live、過測試 1–7）＋ 一張 codec 表（目前只有 pcm）。
> 它現在就編得過、跑得動；你們要做的是把 μ-law 加進表裡。程式裡搜尋 **`★ 第 8 步`** 就是要動手的四個地方。零件在 [m8_mulaw](../tutorial/m8_mulaw/README.md)。
> 繳交：team repo 照 `team2_voicelink/` 的結構，這個資料夾就叫 `mulaw_system/`，程式就叫 `voicelink.c`（助教照這個路徑批改）。

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/mulaw_system` | `cd team_projects\team2_voicelink\mulaw_system` |
| `make` | `mingw32-make` |
| `S=../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `$S = "..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav"` |

**目標**：送端把每個 16-bit sample 壓成 8 bits（一包 320 bytes，type 0x04），收端解回 16-bit 再進 buffer；SQNR ≥ 30 dB、`bytes` 減半；**過測試 8**，而且和 `python_ref` 互通（它的 send 對你的 recv、你的 send 對它的 recv）。

**用到的模組**：m8（兩張表與 encode／decode 原封不動搬進來）、m6（新的 type 0x04 與長度 332）。

### 要加的東西（照順序，約 40 行；對應程式裡的 ★ 第 8 步 (1)–(4)）

1. **搬 m8 的東西**：`#include <math.h>`、`#define MU 255.0`、兩張表 `mulaw_enc[65536]`／`mulaw_dec[256]`、`clampl`、`mulaw_init()`。放在 CODECS 表前面。
2. **寫 frame 版的兩個函式**（m8 是整個檔案一次做，這裡一次 320 個）：

   ```c
   static void mulaw_encode(const int16_t *pcm, uint8_t *out) { for (int i = 0; i < FRAME_SAMPLES; i++) out[i] = mulaw_enc[pcm[i] + 32768]; }
   static void mulaw_decode(const uint8_t *in, int16_t *pcm)  { for (int i = 0; i < FRAME_SAMPLES; i++) pcm[i] = mulaw_dec[in[i]]; }
   ```
3. **表裡加一列**：`{ "mulaw", 0x04, FRAME_SAMPLES, mulaw_encode, mulaw_decode },`（320 bytes；規格的 type 表）。
4. **`main` 開頭叫一次 `mulaw_init()`**（建表幾毫秒）。
5. 其他地方**一行都不用改**——這張表就是為了這一刻。`--codec mulaw` 自動被 `codec_by_name` 認得；recv 看到 type 0x04 自動查到 `mulaw_decode`；STATS 自動印 `codec=mulaw`。

### 為什麼這樣就對

- **PLC 在解碼之後的 16-bit 上做**：收到的包先 `decode` 成 320 個 int16 再 `jb_put`，buffer 與 PLC 不知道 codec 存在。
- **收端以第一個合法 AUDIO 的 type 為準**（tutorial 第 5 步就有），之後 type 不同的當壞封包；`recv` 不需要 `--codec`。
- **編碼端與解碼端要用同一條曲線**（規格）。你的表和 Python 版用同一條公式、同一種取整（`lrint` = Python 的 `round`），所以互測 SQNR 一樣 ≈ 37 dB。
- 驗收看 **SQNR** 不看逐 byte（有失真）：測試 8 算 `10·log10(Σx² / Σ(x − y)²)`。

### 怎麼測（測試 8 跑三趟：pcm 一趟當 bytes 基準、mulaw 乾淨一趟、mulaw 加 `--loss 0.05` 一趟）

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../tests/run_mock_tests.py "./voicelink send" "python3 ../python_ref/voicelink.py recv" $S --only 8` | `python ..\tests\run_mock_tests.py ".\voicelink.exe send" "python ..\python_ref\voicelink.py recv" $S --only 8` |
| `python3 ../tests/run_mock_tests.py "python3 ../python_ref/voicelink.py send" "./voicelink recv" $S --only 8` | `python ..\tests\run_mock_tests.py "python ..\python_ref\voicelink.py send" ".\voicelink.exe recv" $S --only 8` |
| 自己對自己全套：`… "./voicelink send" "./voicelink recv" $S --only 1,2,3,4,5,6,7,8` | 同左 |
| 用耳朵：`./voicelink recv 5004 m.wav` ＋ `./voicelink send 127.0.0.1 5004 $S --codec mulaw` | `.\voicelink.exe recv 5004 m.wav` ＋ `.\voicelink.exe send 127.0.0.1 5004 $S --codec mulaw` |
| live：`./voicelink call 5001 <對方IP> 5001 --codec mulaw`（對方可以是 pcm：兩邊 codec 可以不同，各看對方的 type） | 同左 |

第一列測你的**編碼**（Python 解）、第二列測你的**解碼**（Python 編）。

**看到什麼才算對**

```
send：...speech_osr_16k.wav（537970 samples → 1682 包）→ 127.0.0.1:5004，ssrc=0x...，codec=mulaw，每 20 ms 一包 332 bytes
STATS role=send codec=mulaw frames=1682 bytes=558424 duration_ms=33640 pace=20     ← 558424 / 1096664 = 0.509
codec=mulaw（type 0x04）
STATS role=recv buffer_ms=60 plc=repeat codec=mulaw frames_out=1682 received=1682 lost=0 ... concealed=0 ...
[PASS] 8 mulaw    長度 1075940 bytes（輸入 1075940）；SQNR 37.6 dB（≥ 30.0）；bytes 558424 / pcm 1096664 = 0.509（≈ 一半 ±2%）；...
```

SQNR 37.6 dB（規格 ≥ 30；m8 量的也是 37–38 dB）、bytes 剛好一半（header 不會縮，所以是 332/652 = 0.509 不是 0.5）。

**常見錯誤**

- 表的索引忘了 `+ 32768`：負的 sample 變成負索引 → 讀到陣列外面，有時當掉、有時只是雜音。
- 忘了叫 `mulaw_init()` → 兩張表全是 0，解出來一片靜音、SQNR 0 dB。
- 編碼端 `round` 解碼端 `floor`（或反過來）→ SQNR 掉到 30 dB 以下；兩邊用同一條曲線、同一種取整。
- 表裡 `payload` 寫成 `FRAME_BYTES`（640）→ 封包 652 bytes 但只有前 320 bytes 有東西，python_ref 的 recv 當壞封包。
- Linux 連結時 `undefined reference to log` → 少了 `-lm`（這裡的 Makefile 已經有）。

### 這一步在評分裡

**11/23 demo：μ-law 做到（測試 8 PASS、現場切 `--codec mulaw` 通話）+10。** 它也是 Huffman 的前提（Huffman 疊在 μ-law 的 8-bit 碼字上；在 [adpcm_huffman_system](../adpcm_huffman_system/README.md) 做），而且是程式碼實質審查（最多 30）一定會看的部分：表有沒有用對、有沒有和 python_ref 互通、註解說不說得出為什麼。

---
