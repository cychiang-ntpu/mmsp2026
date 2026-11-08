## 第 6 步｜step6_mulaw.c：功能 4 語音壓縮——`--codec mulaw`，位元率減半

[← 教學總覽](../TUTORIAL.md)｜上一個：[第 5 步 robust](../step5_robust/README.md)｜下一個：[第 7 步 live（官方起點）](../step7_live/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/step6_mulaw` | `cd team_projects\team2_voicelink\tutorial\step6_mulaw` |
| `make`（編出 `./step6_mulaw`（Windows `.\step6_mulaw.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`step6_mulaw.c`](step6_mulaw.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


**目標**：送端把每個 16-bit sample 壓成 8 bits（一包 320 bytes，type 0x04），收端解回 16-bit 再進 buffer；SQNR ≥ 30 dB、`bytes` 減半；過測試 8。

**用到的模組**：m8（兩張表與 encode／decode 原封不動搬進來）、m6（新的 type 0x04 與長度 332）。

**要加的東西（為什麼）**

- **μ-law（G.711 的想法，μ = 255）**：第 9 週 2.2 與 `examples/mulaw_demo.py` 的那條曲線，照抄：

  ```
  壓縮  y = sign(x)·ln(1 + μ|x|) / ln(1 + μ)       x = s / 32768，在 [-1, 1)
  量化  q = clamp(round(y × 128), -128, 127)        256 階均勻量化 → 存成 1 byte = q + 128
  展開  x' = sign(q)·((1 + μ)^(|q|/128) − 1) / μ    s' = round(x' × 32768)
  ```
  小訊號的階距小、大訊號的階距大，所以 SQNR 幾乎不隨音量變（線性 8-bit 小聲時每小 10 dB 就掉 10 dB）。
- **查表**：65536 個可能的 sample 各算一次 → `enc[65536]`（1 byte 一格，64 KB）；256 個 byte 各算一次 → `dec[256]`。
  程式一開始用 `log`／`pow` 建表（幾毫秒），之後每個 sample 只查一次表。`lrint` 是 C99 的四捨五入（.5 取偶數，和 Python 的 `round` 一樣）。
- **編碼端與解碼端要用同一條曲線**（規格）。你的 C 表和 Python 版的表是同一條公式算的，所以互測 SQNR 一樣 ≈ 37 dB。
- **收端以第一個合法 AUDIO 的 type 為準**，之後 type 不同的當壞封包；`recv` 不需要 `--codec` 參數。
- **PLC 在解碼之後的 16-bit 上做**：收到的包先解碼成 320 個 int16 再 `jb_put`，buffer 與 PLC 一行都不用改——這就是把 codec 放在 buffer 外面的好處。
- 驗收改看 **SQNR**（名詞解釋）：輸出不會逐 byte 相同（有失真），測試 8 算 `10·log10(Σx² / Σ(x − y)²)`。

**程式碼**（`diff ../step5_robust/step5_robust.c step6_mulaw.c`）

```c
#define T_MULAW   0x04
#define PKT_MULAW (HDR_LEN + FRAME_SAMPLES)       /* 332 */
static uint8_t mulaw_enc[65536];                  /* 索引 = sample + 32768 */
static int16_t mulaw_dec[256];

static void mulaw_init(void) {
    for (long sv = -32768; sv <= 32767; sv++) {
        double x = (double)sv / 32768.0;
        double y = log(1.0 + MU * fabs(x)) / log(1.0 + MU);  if (x < 0) y = -y;     /* 壓縮 */
        long q = clampl(lrint(y * 128.0), -128, 127);                                 /* 量化 */
        mulaw_enc[sv + 32768] = (uint8_t)(q + 128);
    }
    for (int b = 0; b < 256; b++) {
        double q = (double)(b - 128) / 128.0;
        double x = (pow(1.0 + MU, fabs(q)) - 1.0) / MU;  if (q < 0) x = -x;          /* 展開 */
        mulaw_dec[b] = (int16_t)clampl(lrint(x * 32768.0), -32768, 32767);
    }
}
static void mulaw_encode(const int16_t *pcm, uint8_t *out) { for (i...) out[i] = mulaw_enc[pcm[i] + 32768]; }
static void mulaw_decode(const uint8_t *in, int16_t *pcm)  { for (i...) pcm[i] = mulaw_dec[in[i]]; }
/* parse_header 多一行 */  if (h->type == T_MULAW && len == PKT_MULAW) return 1;
/* send：先湊齊 320 個 sample（最後一包補 0），再依 codec 填 payload */
if (use_mulaw) mulaw_encode(frame, pkt + HDR_LEN); else pcm_to_bytes(frame, FRAME_SAMPLES, pkt + HDR_LEN);
udp_send(&u, pkt, pkt_len);                        /* 332 或 652 */
/* recv：先解碼再進 buffer */
if (!have_codec) { codec_type = h.type; have_codec = 1; } else if (h.type != codec_type) { n_bad++; continue; }
if (h.type == T_MULAW) mulaw_decode(buf + HDR_LEN, frame); else bytes_to_pcm(buf + HDR_LEN, FRAME_SAMPLES, frame);
jb_put(&jb, h.seq, frame, now);
```

**怎麼測**（測試 8 要跑三趟：pcm 一趟當 bytes 的基準、mulaw 乾淨一趟、mulaw 加 `--loss 0.05` 一趟）

| macOS / Linux | Windows PowerShell |
|---|---|
| `S=../../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `$S = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav"` |
| `python3 ../../tests/run_mock_tests.py "./step6_mulaw send" "python3 ../../python_ref/voicelink.py recv" $S --only 8` | `python ..\..\tests\run_mock_tests.py ".\step6_mulaw.exe send" "python ..\..\python_ref\voicelink.py recv" $S --only 8` |
| `python3 ../../tests/run_mock_tests.py "python3 ../../python_ref/voicelink.py send" "./step6_mulaw recv" $S --only 8` | `python ..\..\tests\run_mock_tests.py "python ..\..\python_ref\voicelink.py send" ".\step6_mulaw.exe recv" $S --only 8` |
| 用耳朵：`./step6_mulaw recv 5004 m.wav` ＋ `./step6_mulaw send 127.0.0.1 5004 $S --codec mulaw` | `.\step6_mulaw.exe recv 5004 m.wav` ＋ `.\step6_mulaw.exe send 127.0.0.1 5004 $S --codec mulaw` |

第一列測我們的**編碼**（Python 解）、第二列測我們的**解碼**（Python 編）。如果手上的 `python_ref` 還沒有 `--codec mulaw`
（舊版本），就用第四列自己送自己收，再用這行 Python 算 SQNR：

```
python3 -c "import sys;sys.path.insert(0,'../tests');from run_mock_tests import *;print(sqnr_db(read_pcm('$S'),read_pcm('m.wav')))"
```

**看到什麼才算對**

```
send：...speech_osr_16k.wav（537970 samples → 1682 包）→ 127.0.0.1:5004，ssrc=0x...，codec=mulaw，每 20 ms 一包 332 bytes
STATS role=send codec=mulaw frames=1682 bytes=558424 duration_ms=33640 pace=20     ← 558424 / 1096664 = 0.509
codec=mulaw（type 0x04）
STATS role=recv buffer_ms=60 plc=repeat codec=mulaw frames_out=1682 received=1682 lost=0 ... concealed=0 ...
[PASS] 8 mulaw    長度 1075940 bytes（輸入 1075940）；SQNR 37.6 dB（≥ 30.0）；bytes 558424 / pcm 1096664 = 0.509（≈ 一半 ±2%）；...
```

SQNR 37.6 dB（規格 ≥ 30；`mulaw_demo.py` 量的也是 36–38 dB）、bytes 剛好一半（header 不會縮，所以是 332/652 = 0.509 不是 0.5）。

**常見錯誤**

- 表的索引忘了 `+ 32768`：負的 sample 變成負索引 → 讀到陣列外面，有時當掉、有時只是雜音。
- 編碼端 `round` 解碼端 `floor`（或反過來）→ SQNR 掉到 30 dB 以下；兩邊用同一條曲線、同一種取整。
- recv 的 STATS 還印 `codec=pcm` → 測試 8 的「STATS codec=mulaw」那一項 FAIL。
- Linux 連結時 `undefined reference to log` → 少了 `-lm`（Makefile 的 LIBS 已經有）。

---
