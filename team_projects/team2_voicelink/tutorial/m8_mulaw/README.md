## m8_mulaw.c：μ-law 壓成 8 bits 再解回來，SQNR 多少？

[← 教學總覽](../TUTORIAL.md)｜上一個：[m7 環形 buffer](../m7_ringbuffer/README.md)｜下一個：[第 1 步 raw](../step1_raw/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m8_mulaw` | `cd team_projects\team2_voicelink\tutorial\m8_mulaw` |
| `make`（編出 `./m8_mulaw`（Windows `.\m8_mulaw.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m8_mulaw.c`](m8_mulaw.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


**看什麼**：第 9 週 2.2 的公式照抄，建兩張表（`enc[65536]`、`dec[256]`），之後每個 sample 查一次表。bytes 剛好一半；SQNR ≈ 37 dB（規格 ≥ 30）。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m8_mulaw ../../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav back.wav` | `.\m8_mulaw.exe ..\..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav back.wav` |

```
537970 個 sample：16-bit 是 1075940 bytes → μ-law 是 537970 bytes（比例 0.500）
前 5 個 sample：原始   -919  -1085  -1314  -1256  -1049
               μ-law     80     76     72     73     77（byte）
               解回    -900  -1094  -1325  -1264  -1042
SQNR 37.6 dB（規格 ≥ 30）OK
```

**改一個地方**：`MU` 改成 1（幾乎是線性 8-bit），SQNR 掉到 20 dB 以下；聽 `back.wav` 和原檔的差別。

| 模組 | 零件 | 在 Part B 的哪裡 |
|---|---|---|
| m1 | wav.h 讀寫 | step1 的 `load_wav_16k_mono`、`wav_write` |
| m2 | udp.h 送收 | step1 的 `cmd_send`／`cmd_recv` |
| m3 | clock.h 絕對時刻 | step1 送端的節奏、step3 的播放時鐘 |
| m4、m5 | audio.h callback | step7 的 `audio_cb` |
| m6 | header 打包／解析 | step2 的 `pack_header`／`parse_header` |
| m7 | 環狀 buffer＋時鐘 | step3 的 `jb_put`／`jb_tick` |
| m8 | μ-law 查表 | step6 的 `mulaw_init`／`mulaw_encode`／`mulaw_decode` |

---
