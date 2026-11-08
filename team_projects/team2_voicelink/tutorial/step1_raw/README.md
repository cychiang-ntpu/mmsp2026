## 第 1 步｜step1_raw.c：把 WAV 切成 20 ms 一包用 UDP 送出去

[← 教學總覽](../TUTORIAL.md)｜上一個：[m8 μ-law](../m8_mulaw/README.md)｜下一個：[第 2 步 header](../step2_header/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/step1_raw` | `cd team_projects\team2_voicelink\tutorial\step1_raw` |
| `make`（編出 `./step1_raw`（Windows `.\step1_raw.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`step1_raw.c`](step1_raw.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


**目標**：`send` 每 20 ms 送一包 640 bytes、`recv` 收到什麼就照到達順序寫成 WAV——先讓聲音「過得去」。

**用到的模組**：m1（讀寫 WAV）、m2（`udp_open`／`udp_send`／`udp_recv`）、m3（`sleep_until_ms` 的絕對時刻）。

**要加的東西（為什麼）**

- **frame**：20 ms = 320 samples = 640 bytes（16 kHz、16-bit）。語音在 20 ms 內近似穩定，掉一包損失小；也是所有語音 codec 的自然單位（名詞解釋「frame」）。
- **照真實時間送**：第 k 包在 `t0 + 20k` 送，不是「送一包 sleep 20」。`sleep` 只保證「至少睡這麼久」，Windows 預設一次 15.6 ms，
  `sleep(20)` 常變 31 ms；1000 包下來送端慢了好幾秒。用**絕對時刻**（`clock.h` 的 `sleep_until_ms`）誤差就不會累積：

  ```
  t0        t0+20      t0+40      t0+60        ← 每包的「截止時刻」是固定的格子
  |---------|---------|---------|---------|
  送 k=0     送 k=1     送 k=2     送 k=3       某包晚了 3 ms 也沒關係，下一包還是對準格子
  ```
- **UDP 的三件事**：`udp_open`（開 socket 並 bind）、`udp_set_peer`（記下寄給誰）、`udp_send`／`udp_recv`。
  沒有 connect／accept：兩邊各自 bind 自己的 port、互相指定對方（名詞解釋「UDP」）。`udp_recv` 有逾時，所以「2 秒沒封包就結束」
  只是把 `timeout_ms` 設成 2000。
- **收端的可長大陣列**：不知道會收到多少，用 `realloc` 兩倍兩倍長大（`out`、`out_len`、`out_cap` 三個變數）。

**程式碼**（看 `step1_raw.c`，約 180 行；這些是重點）

```c
#define FRAME_SAMPLES 320                        /* 20 ms @ 16 kHz */
#define FRAME_BYTES   (FRAME_SAMPLES * 2)        /* 640 */

static void pcm_to_bytes(const int16_t *pcm, size_t n, uint8_t *out) {   /* int16 → 2 bytes（little-endian） */
    for (size_t i = 0; i < n; i++) {
        uint16_t v = (uint16_t)pcm[i];           /* 先當無號數看（bit 樣式不變） */
        out[2 * i]     = (uint8_t)(v & 0xff);    /* 低 byte 在前，和 WAV 檔裡一樣 */
        out[2 * i + 1] = (uint8_t)(v >> 8);
    }
}
/* send 的迴圈 */
double t0 = clock_ms();
for (size_t k = 0; k < n_frames; k++) {
    memset(pkt, 0, sizeof(pkt));                 /* 最後一包不足 320 個 sample：補 0 */
    pcm_to_bytes(w.pcm + k * FRAME_SAMPLES, n, pkt);
    udp_send(&u, pkt, sizeof(pkt));
    sleep_until_ms(t0 + 20.0 * (k + 1));         /* 絕對時刻 */
}
/* recv 的迴圈 */
int k = udp_recv(&u, buf, sizeof(buf), wait_ms, NULL);    /* >0 收到幾 bytes；0 逾時；-1 錯誤 */
if (k == 0) break;                                        /* 2 秒沒封包：送端送完了 */
bytes_to_pcm(buf, k / 2, out + out_len);  out_len += k / 2;
```

`load_wav_16k_mono(path, &w)` 的 `&w` 是**輸出參數**：呼叫端準備一個 `wav_t`，把它的位址交給函式，函式把結果填進去——
C 沒有「回傳兩個值」，都用這招（`parse_port` 裡 `strtol(s, &end, 10)` 的 `&end` 也是）。

**怎麼測**

| macOS / Linux | Windows PowerShell |
|---|---|
| 視窗 1：`./step1_raw recv 5004 out1.wav` | `.\step1_raw.exe recv 5004 out1.wav` |
| 視窗 2：`./step1_raw send 127.0.0.1 5004 $W` | `.\step1_raw.exe send 127.0.0.1 5004 $W` |
| 比對：`python3 ../../baseline/smoke_check.py $W out1.wav` | `python ..\..\baseline\smoke_check.py $W out1.wav` |

再試一次，但中間夾搗蛋中繼（三個視窗；中繼聽 5005 轉給 5004，送端改送 5005）：

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` | `python ..\..\baseline\udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` |
| `./step1_raw send 127.0.0.1 5005 $W` | `.\step1_raw.exe send 127.0.0.1 5005 $W` |

**看到什麼才算對**

```
send：...aeueo-16kHz.wav（112541 samples → 352 包）→ 127.0.0.1:5004，每 20 ms 一包
送完 352 包，花了 7040 ms                      ← 352 × 20 = 7040：真的照時間送
收到 352 包，112640 samples，寫到 out1.wav      ← 112640 = 352 × 320（最後一包補了 99 個 0）
PASS: 225082 bytes 的 PCM 逐 byte 相同（352 包，最後一包有 198 bytes 的 0）
```

夾 `--loss 0.05` 之後：`收到 335 包`、WAV 短了 17 包，用播放器聽會聽到「跳一下」。**這就是問題**：沒有 header，收端連「掉了哪一包」都不知道。

**常見錯誤**

- `#include "udp.h"` 不是第一個 include → Windows 上 winsock 的順序錯、POSIX 上 `select` 不見：wrapper 的註解有說，一定排第一。
- 用 `sleep_ms(20)` 代替 `sleep_until_ms(t0 + 20k)` → 7 秒的檔案送了 10 秒；收端 timeout 不會錯，但 live 模式會一直欠載。
- `bind` 失敗「port 被占用」→ 上一個 recv 還沒關（Ctrl-C 它），或換一個 port。

---
