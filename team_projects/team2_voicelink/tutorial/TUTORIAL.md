# Team 2 VoiceLink 手把手教學：先拆成八個模組看懂，再七步組裝到過基本層＋μ-law

這份教學假設你修過一學期的 C，看過第 2 週的 `chat.c`（所以知道 socket 是什麼、`sendto`／`recvfrom` 長什麼樣），
其他都從零講。分成兩部分：

- **Part A 拆解**：八個 50–80 行的小程式 `m1_wav.c` … `m8_mulaw.c`，每個只做**一件事**、印出看得見的結果。
  先把每一塊零件玩過一遍（讀 WAV、寄一張 UDP 明信片、每 20 ms 醒來、錄音、放音、打包 header、環狀 buffer、μ-law）。
- **Part B 組裝**：七個程式 `step1_raw.c` … `step7_live.c`，每一個都是前一個的「超集」，把零件接起來變成規格的 `voicelink`。

Part B 的七步：

```
step1_raw      640 bytes 一包、沒有 header                       ← 和 baseline/voicelink_raw 一樣笨
step2_header   + 12 bytes header（magic/type/seq/ts/ssrc）+ BYE   ← 看得到 seq 跳號了
step3_buffer   + jitter buffer + 播放時鐘 + PLC（packet loss concealment，封包遺失補償） repeat + STATS    ← 過測試 1,2,5（基本層的核心）
step4_counters + late/dup/reordered/overflow/延遲 + --plc + --burst ← 過測試 1,2,3,5,7
step5_robust   + 壞封包、ssrc 換了、seq 繞回、--bind、結束碼        ← 過測試 1–7（基本層＋加分層的一部分）
step6_mulaw    + --codec mulaw（功能 4）                           ← 過測試 8（進階層 A3）
step7_live     + call／devices（麥克風 ↔ 喇叭，執行緒）             ← 進階層 A4
```

每一步都可以單獨編譯、單獨執行、單獨測；想知道「這一步到底改了什麼」就 `diff step3_buffer.c step4_counters.c`。
七個檔案只用 `baseline/wrapper/` 的四個標頭（`udp.h`、`clock.h`、`wav.h`、`audio.h`），什麼都沒複製，編譯時 `-I../baseline/wrapper`。

規格在 [`../README.md`](../README.md)：先把「名詞解釋」那張表看過一遍，下面每一步會指回去。
對照用的 Python 完整版在 [`../python_ref/voicelink.py`](../python_ref/voicelink.py)：它是評測的對手端，所以每一步都拿它互測。

## 0. 準備

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial` | `cd team_projects\team2_voicelink\tutorial` |
| `make`（或 `make step3` 只編第 3 步） | `mingw32-make`（或 `mingw32-make step3`） |
| `W=../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

下面所有指令都用 `$W` 代表那個 7 秒的測試 WAV（16 kHz、單聲道）。要聽真的語音就換成同一個資料夾的 `speech_osr_16k.wav`（34 秒）。
每一步的測試都要開**兩個**（或三個）終端機視窗：收端先開、再開送端；夾搗蛋中繼時中繼第二個開。

`make`（`mingw32-make`）會把 Part A 的模組和 Part B 的七步全部編出來（`make modules` 只編模組）。
`make check`（Git Bash／macOS／Linux）先跑不用互動的模組 m3／m6／m7／m8 並檢查結果，再自動跑 step3–step6 該過的測試、印總表；
`make check-ref` 改用 Python 版當送端（評測就是這樣跑）。
Windows 的 PowerShell 沒有 `sh`，`check` 請在 Git Bash 裡跑；單一步的指令在 PowerShell 都能跑。

---

# Part A 拆解：一次只看一個模組

每個模組都是獨立的一支程式，只用 wrapper 的標頭，50–80 行，註解比程式多。建議順序 m1 → m8，每個 10 分鐘：跑一次、看輸出、改一個地方再跑一次。

## m1_wav.c：讀 WAV、印格式、寫出前 1 秒

**看什麼**：`wav.h` 的 `wav_read(path, &w)` 把整個檔案讀進 `wav_t`（一個 struct：取樣率、聲道、bits、`pcm` 指標、sample 數）；
`&w` 是「輸出參數」——C 沒有回傳兩個值，所以把變數的位址交給函式填。`wav_write` 反過來。Team 2 只收 16 kHz／單聲道／16-bit。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m1_wav $W first_second.wav` | `.\m1_wav.exe $W first_second.wav` |

```
  取樣率   16000 Hz
  聲道     1
  sample   112541 個（每個 int16_t，225082 bytes）
  長度     7.034 秒 = 352 個 20 ms 的 frame        ← 352 就是之後 send 會送的包數
寫出 16000 個 sample（1.000 秒）到 first_second.wav
```

**改一個地方**：16000 改成 8000（寫半秒）；或印 `w.pcm[1000]`。拿一個 44.1 kHz 的 WAV 餵它，看它印錯誤、結束碼 2。

## m2_udp_send.c／m2_udp_recv.c：一次 sendto = 一個 datagram

**看什麼**：UDP 沒有 connect／accept。送端 `udp_open(&u, NULL, 0)`（隨便一個 port）＋ `udp_set_peer`（記下寄給誰）＋ `udp_send`；
收端 `udp_open(&u, NULL, port)` ＋ `udp_recv`（有逾時）。打一行寄一張明信片，對面一次收一整張——不會半行、不會黏行（TCP 才會）。

| macOS / Linux | Windows PowerShell |
|---|---|
| 視窗 1：`./m2_udp_recv 5004` | `.\m2_udp_recv.exe 5004` |
| 視窗 2：`./m2_udp_send 127.0.0.1 5004`，然後打字 | `.\m2_udp_send.exe 127.0.0.1 5004` |

```
12 bytes from 127.0.0.1:54372: hello 你好
11 bytes from 127.0.0.1:54372: second line
收到空字串（對方結束了）
```

**改一個地方**：收端的 `buf` 改成 8 bytes，寄一行長的——多出來的被作業系統直接丟掉（一個 datagram 一次收完）。
把送端的 ip 改成沒人在聽的位址：`udp_send` 照樣回成功（UDP 不知道對方在不在，這就是為什麼 Team 2 要自己送 BYE）。

## m3_clock.c：每 20 ms 醒來一次，誤差多少？

**看什麼**：`clock.h` 的 `sleep_until_ms(t0 + 20k)`——第 k 拍的截止時刻是**絕對時刻**，不是「上一拍 + 20」，所以誤差不累積。
這是送端「每 20 ms 一包」和收端「播放時鐘」都依賴的東西。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m3_clock` | `.\m3_clock.exe` |

```
第 100 拍：該在 + 2000.0 ms，實際 +2000.00 ms，差 +0.00 ms
總共 2000.0 ms（理想 2000），最大誤差 0.07 ms
OK：誤差在 5 ms 以內，可以當播放時鐘
```

**改一個地方**：把 `sleep_until_ms(due)` 換成 `sleep_ms(20)`，看 Windows 上 100 拍慢了 1 秒以上、誤差一拍一拍累積。

## m4_mic_to_wav.c：錄 3 秒麥克風

**看什麼**：`audio.h` 的 **callback**——你不是去「要」聲音，而是交一個函式給音訊系統，它每 20 ms 在它自己的執行緒上呼叫你，
給你 `n` 個剛錄到的 sample、要你填 `n` 個要播的 sample（沒東西就填 0）。`n` 通常是 320 但不保證。`user` 指標把我們的 struct 帶進 callback。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m4_mic_to_wav mic.wav` | `.\m4_mic_to_wav.exe mic.wav` |

```
錄音中… 3 秒（對著麥克風講話）
錄了 48000 個 sample（3.00 秒），callback 被叫了 152 次（平均每次 316 個 sample），存到 mic.wav
```

沒有裝置時印 `沒有裝置`、結束碼 1。**改一個地方**：在 callback 裡印每次的 `n`（只印前 10 次，callback 裡不能慢），看它不是每次都 320。

## m5_play_wav.c：透過 callback 播一個 WAV

**看什麼**：喇叭每次 callback 跟你要 `n` 個 sample，你從檔案的下一段複製過去；檔案播完就填 0。
live 模式的喇叭就是這樣餵的——只是「下一段」改成向 jitter buffer 要一個 frame、沒有就 PLC。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m5_play_wav $W` | `.\m5_play_wav.exe $W` |

```
播放 ...aeueo-16kHz.wav（7.03 秒）…
播了 112541 個 sample，callback 被叫了 ... 次，檔案結束後補了 ... 次 0
```

**改一個地方**：`p->pos += take` 改成 `p->pos += take + 320`（每次多跳一個 frame）——這就是「掉包」的聲音；
或 spk 後半填 0 聽「欠載」。

## m6_header.c：12 bytes 的 header，big-endian

**看什麼**：`put_u16`／`put_u32`（`udp.h`）一個 byte 一個 byte 用位移放：`0x1234` 變成 `12 34`（高位在前）。
不能把 struct 直接 `memcpy` 進封包（對齊空隙、CPU 是 little-endian）。`get_u16`／`get_u32` 解析回來要一模一樣。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m6_header`，再試 `./m6_header 65535` | `.\m6_header.exe`，再試 `.\m6_header.exe 65535` |

```
12 bytes：56 01 | 12 34 | 00 00 05 00 | de ad be ef
          magic type |  seq  |     ts      |    ssrc
解析回來：type=0x01 seq=4660 ts=1280 ssrc=0xDEADBEEF
OK：打包 → 解析 一模一樣
seq 的下一個：65535 → 0（uint16_t 到 65535 之後自動繞回 0）
```

**改一個地方**：`put_u16(p + 2, seq)` 換成 `memcpy(p + 2, &seq, 2)`，hex 變成 `34 12`——這是 little-endian，對方會解成 seq=13330。

## m7_ringbuffer.c：沒有網路的 jitter buffer（step3 的心臟）

**看什麼**：16 格環狀陣列，第 `seq` 包放在第 `seq % 16` 格；播放時鐘每拍要 `next_seq`，看一格就知道在不在：有 → PLAY、沒有 → REPEAT，
**拍子一定前進**。假的到達順序 `0,1,3,2,5,5,4,7`：3 比 2 先到（亂序）、5 到兩次（重複）、6 沒到（遺失）。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m7_ringbuffer` | `.\m7_ringbuffer.exe` |

```
收到 seq=3：放進第  3 格   buffer={ 0 1 3 }
收到 seq=2：放進第  2 格   buffer={ 0 1 2 3 }        ← 位置由 seq 決定：亂序到的自動排好
收到 seq=5：已經在第 5 格 → 重複，丟掉
...
tick 5  next_seq=5  buffer={ 5 7 }  PLAY seq=5
tick 6  next_seq=6  buffer={ 7 }  REPEAT（補 seq=5 的內容；seq 6 沒到）
tick 7  next_seq=7  buffer={ 7 }  PLAY seq=7
結果：PLAY 7 次、REPEAT 1 次；concealed = 1 = 遺失的包數（seq 6）
```

**改一個地方**：到達順序改成 `0,2,4,6`（看 REPEAT 變 4 次）；`RING_SLOTS` 改成 4，看 seq 7 和 seq 3 搶同一格（7 % 4 = 3）。

## m8_mulaw.c：μ-law 壓成 8 bits 再解回來，SQNR 多少？

**看什麼**：第 9 週 2.2 的公式照抄，建兩張表（`enc[65536]`、`dec[256]`），之後每個 sample 查一次表。bytes 剛好一半；SQNR ≈ 37 dB（規格 ≥ 30）。

| macOS / Linux | Windows PowerShell |
|---|---|
| `./m8_mulaw ../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav back.wav` | `.\m8_mulaw.exe ..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav back.wav` |

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

# Part B 組裝：七步做出 voicelink

## 第 1 步｜step1_raw.c：把 WAV 切成 20 ms 一包用 UDP 送出去

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
| 比對：`python3 ../baseline/smoke_check.py $W out1.wav` | `python ..\baseline\smoke_check.py $W out1.wav` |

再試一次，但中間夾搗蛋中繼（三個視窗；中繼聽 5005 轉給 5004，送端改送 5005）：

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` | `python ..\baseline\udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` |
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

## 第 2 步｜step2_header.c：每包加 12 bytes 的 header，親眼看見 seq 跳號

**目標**：送端每包前面加 `magic/type/seq/ts/ssrc`，最後送 3 次 BYE；收端解析、驗證、印出每包的 seq／ts，但還是照到達順序寫。

**用到的模組**：m6（`pack_header`／`parse_header` 就是它）＋第 1 步的 m1、m2、m3。

**要加的東西（為什麼）**

- **封包格式**（規格「封包格式」，RTP 的精簡版）：

  ```
  offset  0     1     2  3   4  5  6  7   8  9 10 11   12 ....................... 651
         +-----+-----+------+------------+------------+--------------------------------+
         |0x56 |type |  seq |     ts     |    ssrc    |   PCM 640 bytes（320 × int16）  |
         +-----+-----+------+------------+------------+--------------------------------+
          magic 0x01  16-bit   32-bit       32-bit      little-endian（WAV 的原樣）
                AUDIO  每包+1   每包+320     本次隨機
                0x02
                BYE  （BYE 只有 12 bytes，ts = 真正的 sample 總數）
  ```
  `seq` 回答「有沒有掉／亂序／重複」，`ts` 回答「什麼時候該播」，`ssrc` 回答「這是哪一次的串流」（對方重啟就換）。
- **big-endian**：網路慣例高位 byte 在前，CPU 卻多半是 little-endian，而且 struct 有對齊空隙——所以**不能把 struct 直接 memcpy 進封包**，
  要一個 byte 一個 byte 用位移放：`put_u16(p+2, seq)` 就是 `p[2] = seq >> 8; p[3] = seq & 0xff`（`udp.h` 已經寫好）。
- **先檢查長度再讀欄位**：長度不到 12 就去讀 `p[11]` 是讀到陣列外面（壞封包測試就是在測這個）。
- **BYE × 3**：UDP 沒有連線，「結束」要自己說；可能掉，所以說三次。BYE 的 `ts` 放真正的 sample 總數，收端用它去掉最後一包補的 0
  （這是 Python 版的約定，第 3 步會用到）。
- **STATS role=send**：規格要求的一行，印到 stderr（測試 8 要用 `bytes`）。

**程式碼**（`diff step1_raw.c step2_header.c`）

```c
typedef struct { uint8_t type; uint16_t seq; uint32_t ts; uint32_t ssrc; } hdr_t;   /* 解析好的 header */

static void pack_header(uint8_t *p, uint8_t type, uint16_t seq, uint32_t ts, uint32_t ssrc) {
    p[0] = MAGIC;  p[1] = type;
    put_u16(p + 2, seq);  put_u32(p + 4, ts);  put_u32(p + 8, ssrc);     /* big-endian，一個 byte 一個 byte 放 */
}
static int parse_header(const uint8_t *p, int len, hdr_t *h) {
    if (len < HDR_LEN || p[0] != MAGIC) return 0;                        /* 先看長度與 magic */
    h->type = p[1];  h->seq = get_u16(p + 2);  h->ts = get_u32(p + 4);  h->ssrc = get_u32(p + 8);
    if (h->type == T_AUDIO && len == PKT_AUDIO) return 1;                /* 長度要和 type 配：652 */
    if (h->type == T_BYE && len == HDR_LEN) return 1;                    /* 12 */
    return 0;
}
/* send：header 在前 12 bytes，PCM 從 offset 12 開始；seq 是 uint16_t，65535 之後自動繞回 0 */
pack_header(pkt, T_AUDIO, seq, (uint32_t)(k * FRAME_SAMPLES), ssrc);
pcm_to_bytes(w.pcm + start, n, pkt + HDR_LEN);
seq++;
/* 送完：3 次 BYE，ts = 真正的 sample 數 */
for (int i = 0; i < BYE_REPEAT; i++) { pack_header(bye, T_BYE, seq, (uint32_t)w.n_samples, ssrc); udp_send(...); seq++; }
/* recv */
if (!parse_header(buf, k, &h)) { n_bad++; continue; }                    /* 不是我們的封包：丟掉 */
if (h.type == T_BYE) break;
printf("seq=%u ts=%u ssrc=0x%08X\n", h.seq, h.ts, h.ssrc);
bytes_to_pcm(buf + HDR_LEN, FRAME_SAMPLES, out + out_len);               /* 還是到達順序！ */
```

**怎麼測**（三個視窗；這一步就開始和 Python 版互測）

| macOS / Linux | Windows PowerShell |
|---|---|
| `./step2_header recv 5004 out2.wav` | `.\step2_header.exe recv 5004 out2.wav` |
| `python3 ../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` | `python ..\baseline\udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` |
| `./step2_header send 127.0.0.1 5005 $W` | `.\step2_header.exe send 127.0.0.1 5005 $W` |
| 反過來：`python3 ../python_ref/voicelink.py recv 5004 out2b.wav --probe` 配 `./step2_header send 127.0.0.1 5004 $W` | `python ..\python_ref\voicelink.py recv 5004 out2b.wav --probe` 配 `.\step2_header.exe send 127.0.0.1 5004 $W` |

**看到什麼才算對**

```
seq=13 ts=4160 ssrc=0x152D469B
seq=15 ts=4800 ssrc=0x152D469B       ← 14 不見了（--loss 0.05）：現在「看得到」掉了哪一包
seq=16 ts=5120 ssrc=0x152D469B
...
BYE  seq=352 ts=112541（真正的 sample 數）
收到 335 包（丟掉 0 個壞封包），107200 samples，寫到 out2.wav
STATS role=send codec=pcm frames=352 bytes=229504 duration_ms=7040 pace=20      ← 送端（stderr）
```

Python 版收我們的封包時要印 `STATS role=recv ... frames_out=352 received=352 lost=0 ... concealed=0`，而且 `out2b.wav` 逐 byte 相同
（`--probe` 會把每一包的命運印出來，看得到它解析我們的 header）。這代表**封包格式對了**——11/9 討論會的「格式互測」就是測這個。

**常見錯誤**

- `put_u16`／`get_u16` 用錯邊（送端用 big-endian、收端卻 `buf[2] | buf[3] << 8`）→ Python 版印 `seq=256`、`seq=512`……跳 256 的倍數。
- 忘了 `pkt + HDR_LEN`：PCM 從 offset 0 開始寫，蓋掉 header → 對方說全是壞封包。
- `sizeof(pkt)` 還是 640 → 長度不是 652，對方丟掉。

---

## 第 3 步｜step3_buffer.c：jitter buffer、播放時鐘、PLC——基本層的核心

**目標**：收端先存 `--buffer` ms 再播，之後每 20 ms 一拍向 buffer 要「下一個 seq」，有就播、沒有就補，拍子一定前進；過測試 1、2、5。

**用到的模組**：m7（環狀 buffer 與 PLAY／REPEAT 的拍子，加上真的 320 個 sample 與到達時刻）、m3（播放時鐘的絕對時刻）、
m2（`udp_recv` 的逾時 = 離下一拍的時間）。

**要加的東西（為什麼）**

- **問題**：送端每 20 ms 一包，網路送到的時間卻有快有慢（jitter）、不見（loss）、超車（reorder）。喇叭卻**必須**每 20 ms 拿到一個 frame。
- **jitter buffer = 以 seq 為索引的環狀陣列**。16 格，第 `seq` 包放在第 `seq % 16` 格。「下一個該播的在不在」只要看一格，不用搜尋、不用排序；
  亂序到的包自動排到正確位置（位置由 seq 決定，不是到達順序）：

  ```
  格子:    0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
         +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
  seq:   |16 |17 | . |19 | . | . | . | . | . | . | . | . | . |13 |14 |15 |     . = 空
         +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
                                                              ^ expected=13 → 第 13%16=13 格有 13 → 播
  下一拍 expected=14 → 第 14 格 → 播；…… expected=18 → 第 2 格是空的 → PLC 補一個，expected 照樣變 19
  ```
  每格記：`valid`（有沒有東西）、`seq`（是哪一包）、`arrived`（到達時刻，算延遲用）、320 個 sample。
- **播放時鐘用絕對時刻**：第 i 拍在 `play_t0 + 20i`，`play_t0` = 第一包到達 + `--buffer` ms。理由和第 1 步的送端一樣：誤差不累積。
  程式停頓太久就用 `while` 一次補齊好幾拍，`frames_out` 才和時間一致：

  ```
  第一包到                 開播 play_t0          +20       +40       +60
  |<---- buffer 60 ms ---->|---------|---------|---------|---------|---->
  收 0  收 1  收 2  收 3     播 0      播 1      播 2      補 3(掉了)  播 4
  ```
- **一條執行緒就夠的小技巧**：`udp_recv` 的逾時設成「離下一拍還有多久」。有封包先到 → 收它、放進 buffer，再算一次；沒封包 →
  時間到 `udp_recv` 回傳 0，剛好就是該播下一拍的時候。所以一個 `for` 迴圈同時做了「收封包」和「每 20 ms 播一拍」。
  （Windows 上 `select` 的逾時只有 1 ms 的準度是因為 `clock.h` 第一次呼叫時做了 `timeBeginPeriod(1)`。）
- **16-bit seq 的比較**：`seq` 到 65535 會繞回 0，直接比大小會以為 0 < 65535。做法是「無號數相減再當成有號 16-bit 看」：

  ```c
  static int seq_diff(uint16_t a, uint16_t b) { return (int16_t)(uint16_t)(a - b); }
  /* a=0,     b=65535 → (uint16_t)(0 − 65535) = 1     → +1：a 比 b 新 1 包 */
  /* a=65534, b=2     → (uint16_t)(65534 − 2) = 65532 → −4：a 比 b 舊 4 包 */
  ```
  只要兩包距離小於 32768（約 11 分鐘）就對；RTP 也是這樣比的。整支程式所有「誰新誰舊」都用它，從此不用擔心繞回。
- **PLC repeat**：沒有該播的 seq 就重複上一個播出的 frame（`jb->last`），計 `concealed`。拍子照走——這就是「寧可補一包，也不讓喇叭停下來」。
- **三種結束**：BYE（知道結尾 seq → 把 buffer 播完再停）、`--timeout` 沒封包（送端死了 → 把等待期間補的 frame 從輸出去掉）、
  第一包等 30 秒都沒來。**永遠不會無限等**（規格）。
- **輸出截到 BYE 的 ts**：最後一包補的 0 去掉，沒搗蛋時輸出就和輸入逐 byte 相同、長度也相同。
- **STATS**：規格的一行，這一步先填 `frames_out`／`received`／`lost`／`concealed`，其他印 0（基本層 B4 只看這四個）。
  `lost` = 從第一個播的 seq 到結尾「應該有幾包」− 「真的到過幾包」（`unique`）。

**程式碼**（`diff step2_header.c step3_buffer.c`；整個 buffer 包成 `jb_t` 和五個函式，第 7 步會原封不動拿去 live 用）

```c
typedef struct { int valid; uint16_t seq; double arrived; int16_t pcm[FRAME_SAMPLES]; } slot_t;
typedef struct {
    slot_t   slot[RING_SLOTS];  int count;
    int      started;  uint16_t expected;                 /* 播放時鐘：下一個該播的 seq */
    int have_first;  uint16_t first_seq;                  /* 開播前看過最小的 seq：從它開始 */
    int have_highest; uint16_t highest;                   /* 看過最大的 seq */
    int have_end;    uint16_t end_seq;                    /* 結尾（BYE 或逾時才知道） */
    int16_t  last[FRAME_SAMPLES];                         /* 上一個播出的 frame（repeat 用） */
    unsigned received, unique, frames_out, concealed, trailing_concealed, played_real;
    double   delay_sum, delay_max;
} jb_t;

static void jb_put(jb_t *jb, uint16_t seq, const int16_t *pcm, double now) {   /* 收到一包 */
    jb->received++;
    if (jb->started && seq_diff(seq, jb->expected) < 0) { jb->unique++; return; }   /* 遲到：丟 */
    slot_t *s = &jb->slot[seq % RING_SLOTS];              /* 取餘數就是它的格子 */
    if (s->valid && s->seq == seq) return;                /* 重複：丟 */
    ... 填 s->seq / s->arrived / s->pcm，更新 highest、first_seq ...
}
static void jb_tick(jb_t *jb, int16_t *out, double now) {                      /* 一拍 */
    slot_t *s = &jb->slot[jb->expected % RING_SLOTS];
    if (s->valid && s->seq == jb->expected) { memcpy(out, s->pcm, ...); memcpy(jb->last, s->pcm, ...); s->valid = 0; ... }
    else { memcpy(out, jb->last, FRAME_BYTES); jb->concealed++; jb->trailing_concealed++; }   /* PLC repeat */
    jb->expected++;  jb->frames_out++;                    /* 不管有沒有，拍子都前進 */
}
```

主迴圈（`cmd_recv`）的五個步驟：

```c
for (;;) {
    if (jb_done(&jb)) break;
    /* 1. 下一件事的時刻：下一拍（play_t0 + 20 × frames_out）或逾時（last_rx + timeout），看哪個先 */
    /* 2. udp_recv(..., wait_ms, ...)：等到那一刻，或封包先來 */
    /* 3. 收到一包：AUDIO → 第一包就定 play_t0 = now + buffer；jb_put。BYE → jb_finish_at(結尾) */
    /* 4. 逾時：結尾 = 看過最大的 seq + 1；把等待期間補的 trailing_concealed 個 frame 從輸出去掉 */
    /* 5. 到了拍子：jb_tick → outbuf_append；while 補齊 */
}
```

BYE 的結尾怎麼算：三個 BYE 的 seq 各 +1，第一個掉了就會多算一包；所以用 `ts`（真正的 sample 數）交叉比對——
`ceil(ts/320)` 就是 frame 數 = 結尾的 seq；和 BYE 的 seq 差 0、1、2 都合理，採用 ts 算的（和 Python 版一樣）。

**怎麼測**

| macOS / Linux | Windows PowerShell |
|---|---|
| `./step3_buffer recv 5004 out3.wav` | `.\step3_buffer.exe recv 5004 out3.wav` |
| `python3 ../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` | `python ..\baseline\udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` |
| `python3 ../python_ref/voicelink.py send 127.0.0.1 5005 $W` | `python ..\python_ref\voicelink.py send 127.0.0.1 5005 $W` |

自動驗收（一個視窗就好，腳本自己開 recv／中繼／send；`--only 1,2,5` 是基本層）：

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../tests/run_mock_tests.py "python3 ../python_ref/voicelink.py send" "./step3_buffer recv" $W --only 1,2,5` | `python ..\tests\run_mock_tests.py "python ..\python_ref\voicelink.py send" ".\step3_buffer.exe recv" $W --only 1,2,5` |
| `python3 ../tests/run_mock_tests.py "./step3_buffer send" "python3 ../python_ref/voicelink.py recv" $W --only 1,2,5` | `python ..\tests\run_mock_tests.py ".\step3_buffer.exe send" "python ..\python_ref\voicelink.py recv" $W --only 1,2,5` |

**看到什麼才算對**

```
第一包到了（seq=0），60 ms 後開播
開播：expected=0，buffer 裡有 4 包
BYE（seq=352 ts=112541）：結尾 = seq 352，把 buffer 播完再結束
已存檔：out3.wav（112541 samples，352 frames）
STATS role=recv buffer_ms=60 plc=repeat codec=pcm frames_out=352 received=335 lost=17 late=0 dup=0 reordered=0 overflow=0 concealed=17 avg_delay_ms=60.3 max_delay_ms=68.9
```

`frames_out = 352`（和輸入一樣多）、`concealed = lost = 17`：掉的 17 包都補上了，聽起來只是偶爾「嗡」一下。測試腳本：

```
[PASS] 1 clean    225082 bytes 逐 byte 相同；lost=0 concealed=0
[PASS] 2 loss     frame 數 352（輸入 352）；concealed=17 lost=17；lost > 0（搗蛋有生效）
[PASS] 5 kill     recv 在 4.3 s 內結束（上限 7.3 s）；WAV 可讀，75 frames；frames_out=75 與 WAV 一致；收到的部分與輸入逐 byte 相同
summary: 3/3 passed
```

**常見錯誤**

- 逾時結束時沒有把「送端死掉之後」補的 frame 去掉 → 測試 5 的 WAV 多了 100 個靜音 frame、`frames_out` 和 WAV 對不上
  （規格明說這些不算）。
- `jb_tick` 在「沒有」的分支忘了 `expected++` → 時鐘卡在同一個 seq，之後全部變 late。
- 用 `seq < expected` 比大小 → 平常沒事，`--seq0 65500`（第 5 步）一跑就全錯。從一開始就用 `seq_diff`。

---

## 第 4 步｜step4_counters.c：把每一包都算清楚（late／dup／reordered／overflow／延遲）

**目標**：每個收到的 AUDIO 封包剛好落在一類，`frames_out = (received − late − dup − overflow) + concealed` 永遠成立；加 `--plc zero|repeat`、
`send --burst`；過測試 1、2、3、5、7。

**用到的模組**：m7（同一個環狀 buffer，`jb_put` 多了分類）、m6（seq 的 16-bit 繞回：`seq_diff`）。

**要加的東西（為什麼）**

- **四類**（名詞解釋 late／dup／reordered／overflow）：

  ```
  收到 seq ──┬─ 之前收過這個 seq？ ──是──▶ dup（丟）
             └─ 否 ─┬─ 它的拍子已經過了（seq 比 expected 舊）？ ──是──▶ late（丟）
                    └─ 否 ─┬─ 比看過最大的小？ ──是──▶ reordered（照樣放進去，位置由 seq 決定）
                           └─ 放進第 seq % 64 格 ──▶ 滿了（> cap）就丟最舊的 ──▶ overflow
  ```
- **「之前收過沒」用一張位元表**：`uint8_t seen[65536/8]`，第 seq 個 bit = 1 表示收過（8 KB；查一次 O(1)）。
  還在 buffer 裡、已經播過、遲到過的都算「收過」，所以 dup 一眼就看得出來。`lost` 也靠它：第一次看到的 seq 才算 `unique`。
- **上限 cap = 4 × buffer 的 frame 數**（規格建議）：`--burst` 一口氣送幾千包時 buffer 不會吃光記憶體。超過就丟最舊的（掃 64 格找 seq 最小的）。
  環狀陣列放大到 64 格，`--buffer` 最多 320 ms。
- **延遲** = 每個真的播出去的 frame「到達 → 播放」的時間差；`avg_delay_ms` 大約 = `--buffer` + 網路抖動。這是報告「buffer 大小 vs 延遲」的數字。
- **`--plc zero`**：補 320 個 0。波形突然跳到 0 會「喀」一聲；`repeat` 單掉一包幾乎聽不出，但連續掉會變「嗡」的機械音——報告第 2 題。

**程式碼**（`diff step3_buffer.c step4_counters.c`）

```c
static int  seen_get(const jb_t *jb, uint16_t seq) { return (jb->seen[seq >> 3] >> (seq & 7)) & 1; }   /* 第 seq/8 byte 的第 seq%8 位 */
static void seen_set(jb_t *jb, uint16_t seq)       { jb->seen[seq >> 3] |= (uint8_t)(1u << (seq & 7)); }

static void jb_put(jb_t *jb, uint16_t seq, const int16_t *pcm, double now) {
    jb->received++;
    if (seen_get(jb, seq)) { jb->dup++; return; }                              /* 1. 重複 */
    seen_set(jb, seq);  jb->unique++;
    if (jb->started && seq_diff(seq, jb->expected) < 0) { jb->late++; return; } /* 2. 遲到 */
    if (jb->have_highest && seq_diff(seq, jb->highest) < 0) jb->reordered++;   /* 3. 亂序但來得及 */
    else { jb->highest = seq; jb->have_highest = 1; }
    slot_t *s = &jb->slot[seq % RING_SLOTS];                                   /* 4. 放進去 */
    if (s->valid) { if (seq_diff(seq, s->seq) < 0) { jb->overflow++; return; } jb->overflow++; } else jb->count++;
    ...
    if (jb->count > jb->cap) jb_drop_oldest(jb);                               /* 5. 超過上限：丟最舊的 */
}
/* jb_tick 的 PLC 分支 */
if (jb->plc_zero) memset(out, 0, FRAME_BYTES); else memcpy(out, jb->last, FRAME_BYTES);
/* STATS：欄位順序與名稱不可改 */
fprintf(stderr, "STATS role=recv buffer_ms=%d plc=%s codec=pcm frames_out=%u received=%u lost=%u "
        "late=%u dup=%u reordered=%u overflow=%u concealed=%u avg_delay_ms=%.1f max_delay_ms=%.1f\n", ...);
```

**怎麼測**（測試 3 是 `--reorder 0.1 --jitter 20` 配 `--buffer 100`，腳本會自己加）

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../tests/run_mock_tests.py "python3 ../python_ref/voicelink.py send" "./step4_counters recv" $W --only 1,2,3,5,7` | `python ..\tests\run_mock_tests.py "python ..\python_ref\voicelink.py send" ".\step4_counters.exe recv" $W --only 1,2,3,5,7` |
| 手動看 overflow：`./step4_counters recv 5004 o.wav` ＋ `./step4_counters send 127.0.0.1 5004 $W --burst` | `.\step4_counters.exe recv 5004 o.wav` ＋ `.\step4_counters.exe send 127.0.0.1 5004 $W --burst` |
| 聽 zero 和 repeat 的差別：`./step4_counters recv 5004 z.wav --plc zero` 配第 3 步的 `--loss 0.05` 中繼 | `.\step4_counters.exe recv 5004 z.wav --plc zero` |

**看到什麼才算對**

```
[PASS] 3 reorder  225082 bytes 逐 byte 相同；reordered=27 late=0 concealed=0
[PASS] 7 stats    stderr 恰好一行 STATS；buffer_ms／plc 與參數一致；frames_out=352 = (343−0−8−0) + 17；...
summary: 5/5 passed
```

`--burst` 時：`overflow` 很大（幾百）、`frames_out` 還是 352、`concealed` ≈ overflow——buffer 有上限，所以記憶體沒爆，但聲音只剩最後 12 包是真的。
這就是「送端也要照時間送」的理由（報告「量測與報告」第 3 題）。

**常見錯誤**

- 四類不是互斥（例如 late 的包也計 reordered）→ 測試 7 的式子對不上。照上面的順序：dup → late → reordered → 放進去／overflow。
- `seen` 在遲到時沒設 → 同一包遲到兩次算兩次 late，`lost` 也會少算。
- `avg_delay` 把 PLC 補的也算進去 → 規格說只算真的播出去的包。

---

## 第 5 步｜step5_robust.c：壞封包不當掉、ssrc 換了、seq 繞回、--bind、結束碼

**目標**：對 port 丟 100 包亂碼不當掉；對方重啟（ssrc 換了）就重置；seq 從 65535 繞到 0 照樣對；過測試 1–7（含 4、6）。

**用到的模組**：m6（`parse_header` 先看長度再讀欄位；`m6_header 65535` 的繞回）、m2（`--bind`：`udp_open` 的第二個參數）。

**要加的東西（為什麼）**

- **壞封包**：`parse_header` 從第 2 步起就「先檢查長度再讀欄位」，本來就不會讀到陣列外面；這一步把它計數、印出來，並把「type 不認識」也算進去。
  規則只有一條：收到什麼都不能 crash（測試 6 丟的長度包括 1、11、12、13、651、652、653、1400）。
- **ssrc 換了**（名詞解釋「ssrc」）：對方重啟後 seq 從 0 開始、ts 也從 0 開始，不重置的話全部變 late。`jb_reset` 清 buffer、位元表、播放進度，
  **計數器保留**（STATS 是整次執行的）。別的 ssrc 的 BYE 不理。
- **seq 繞回與位元表**：`seq_diff` 已經處理「比大小」，但 `seen` 位元表 22 分鐘後繞回來會把新包當 dup。做法：每一拍把「正對面」
  （`expected + 32768`）那個 bit 清掉；拍子連續，所以一圈回來那些 bit 早就是 0。`send --seq0 65500` 可以從快繞回的地方開始送，親眼看它繞過去。
- **BYE 被亂序超車**：BYE 之後才到的更後面的包 → 把結尾往後推；BYE 的 seq 太離譜（比看過最大的多超過 cap）就不信它。
- **`--bind <ip>`**：只收某張網卡（規格的參數）；第一包的等待上限 = max(10 × timeout, 30 s)。
- **結束碼**：0 正常；1 網路或裝置錯誤（`udp_open` 失敗、`udp_recv` 回 −1）；2 參數或檔案格式錯誤。前幾步已經這樣做，這裡整理成一致。

**程式碼**（`diff step4_counters.c step5_robust.c`）

```c
static void seen_clear(jb_t *jb, uint16_t seq) { jb->seen[seq >> 3] &= (uint8_t)~(1u << (seq & 7)); }
static void jb_reset(jb_t *jb) {                       /* 新的串流：不能 memset 整個（cap、plc、計數器要留） */
    for (int i = 0; i < RING_SLOTS; i++) jb->slot[i].valid = 0;
    memset(jb->seen, 0, sizeof(jb->seen));
    jb->count = 0;  jb->started = 0;  jb->have_first = jb->have_highest = jb->have_end = 0;  jb->trailing_concealed = 0;
}
/* jb_tick 最後 */
seen_clear(jb, (uint16_t)(jb->expected + 32768));      /* 繞回的準備：每一拍清掉對面那一個 bit */
/* cmd_recv 收到一包 */
if (!parse_header(buf, k, &h)) { n_bad++; if (n_bad <= 3) printf("壞封包 %d bytes ...\n", k); }
else if (h.type == T_AUDIO) {
    if (have_ssrc && h.ssrc != ssrc) { printf("ssrc 變了：重置 buffer\n"); jb_reset(&jb); have_bye = 0; }
    ssrc = h.ssrc; have_ssrc = 1;
    ...
} else if (!jb.have_end && (!have_ssrc || h.ssrc == ssrc)) { /* BYE：同一個 ssrc 的才算 */ }
```

**怎麼測**

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../tests/run_mock_tests.py "python3 ../python_ref/voicelink.py send" "./step5_robust recv" $W --only 1,2,3,4,5,6,7` | `python ..\tests\run_mock_tests.py "python ..\python_ref\voicelink.py send" ".\step5_robust.exe recv" $W --only 1,2,3,4,5,6,7` |
| `python3 ../tests/run_mock_tests.py "./step5_robust send" "python3 ../python_ref/voicelink.py recv" $W --only 1,2,3,4,5,6,7` | `python ..\tests\run_mock_tests.py ".\step5_robust.exe send" "python ..\python_ref\voicelink.py recv" $W --only 1,2,3,4,5,6,7` |
| 繞回：`./step5_robust recv 5004 w.wav` ＋ `./step5_robust send 127.0.0.1 5004 $W --seq0 65500` | `.\step5_robust.exe recv 5004 w.wav` ＋ `.\step5_robust.exe send 127.0.0.1 5004 $W --seq0 65500` |
| 重啟：recv 開著，`send` 跑兩次（第二次 ssrc 不同） | 同左 |

**看到什麼才算對**

```
[PASS] 4 dup      225082 bytes 逐 byte 相同；dup=38
[PASS] 6 garbage  225082 bytes 逐 byte 相同；received=352（壞封包沒有被算進去）
summary: 7/7 passed
```

繞回那次：`第一包到了（seq=65500）`、`BYE（seq=316 ...）：結尾 = seq 316`、STATS `lost=0 concealed=0`，`w.wav` 逐 byte相同
（用測試腳本的 `same_pcm` 或 Python 版 `--probe` 看）。重啟那次：`ssrc 從 0x... 變成 0x...：重置 buffer`，第二段聲音接在第一段後面播。

**常見錯誤**

- `jb_reset` 用 `memset(jb, 0, sizeof *jb)` → cap 變 0，之後每一包都 overflow。
- 壞封包也更新 `last_rx` → 有人一直丟垃圾時 `--timeout` 永遠不會到。只有合法封包才算「有活動」。
- 位元表沒清對面那一格 → 跑超過 22 分鐘的 live 通話後突然全部變 dup。

---

## 第 6 步｜step6_mulaw.c：功能 4 語音壓縮——`--codec mulaw`，位元率減半

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

**程式碼**（`diff step5_robust.c step6_mulaw.c`）

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
| `S=../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `$S = "..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav"` |
| `python3 ../tests/run_mock_tests.py "./step6_mulaw send" "python3 ../python_ref/voicelink.py recv" $S --only 8` | `python ..\tests\run_mock_tests.py ".\step6_mulaw.exe send" "python ..\python_ref\voicelink.py recv" $S --only 8` |
| `python3 ../tests/run_mock_tests.py "python3 ../python_ref/voicelink.py send" "./step6_mulaw recv" $S --only 8` | `python ..\tests\run_mock_tests.py "python ..\python_ref\voicelink.py send" ".\step6_mulaw.exe recv" $S --only 8` |
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

## 第 7 步｜step7_live.c：`call`——麥克風進去、對方喇叭出來（執行緒只在這一步出現）

**目標**：`call <my_port> <peer_ip> <peer_port>` 兩台電腦戴耳機通話；同一套 jitter buffer 與 PLC；按 Enter 結束印 STATS；`devices` 列裝置。

**用到的模組**：m4（mic 那一半 → 湊 320 → m6 的 header → m2 的 `udp_send`）、m5（spk 那一半 → 向 m7 的 buffer 要 frame）、
m8（`--codec mulaw`）。

**要加的東西（為什麼）**

- **callback**（`audio.h` 的說明）：你不是去「要」聲音，而是把一個函式交給音訊系統，它每 20 ms 在**它自己的執行緒**上呼叫你：
  給你剛錄到的 `n` 個 sample（`mic`），並要你把接下來要播的 `n` 個填進 `spk`。`n` 通常是 320，但不保證，所以 mic 要「湊」、spk 要「分」。
- **audio callback 就是播放時鐘**：mock 模式要自己算 `play_t0 + 20i`，live 不用——喇叭每 320 個 sample 跟你要一次，節奏由硬體決定。
  每湊滿 320 就 `jb_tick` 一次，其他一模一樣（開播條件也一樣：第一包到達 + `--buffer` ms）。
- **為什麼要一條接收執行緒**：mock 的「播放」只是寫記憶體，可以和收封包排在同一個迴圈；live 的播放由音訊系統在它的執行緒上呼叫，
  我們管不了它什麼時候來，所以收封包要另外一條執行緒（`recv_thread`：`udp_recv` 200 ms 逾時 → 解析 → 解碼 → `jb_put`）。

  ```
  main ──── 等 Enter ─────────────────────────────────────────────▶ running=0、BYE×3、audio_stop、join
  recv_thread ── udp_recv → parse → decode → [lock] jb_put [unlock] ──▶ 一直到 running=0
  音訊執行緒 ─── 每 20 ms audio_cb：mic → 湊 320 → encode → sendto
                                   spk ← 每 320 個 [lock] jb_tick [unlock]
  ```
- **鎖（mutex）**：兩條執行緒都碰 `jb`，不加鎖會在 `jb_put` 寫到一半時被 `jb_tick` 讀到。規則：**任何讀寫 jb 的地方都在 `lock_take` / `lock_give` 之間，
  鎖裡不做慢的事**（解碼在鎖外做、不 printf、不等網路）。平台差異（Windows `CRITICAL_SECTION`／POSIX `pthread_mutex`）包成四個小函式，
  和 `baseline/voicelink_raw.c`、第 2 週 `chat.c` 同一套。
- **結束**：Enter → `running = 0` → `audio_stop()`（之後 callback 不會再來）→ 送 3 次 BYE → `thread_join`。對方收到 BYE 或看到新的 ssrc 就 `jb_reset`。
- `send`／`recv` 一行都沒改——`diff step6_mulaw.c step7_live.c` 全是「加」。

**程式碼**

```c
typedef struct {
    udp_t u;  jb_t jb;  lock_t lock;  volatile int running;
    uint32_t ssrc; uint16_t seq; uint32_t ts;                 /* 送端：我的串流 */
    int16_t mic_stage[FRAME_SAMPLES]; size_t mic_fill;        /* 湊 320 */
    int16_t spk_cur[FRAME_SAMPLES];   size_t spk_pos;         /* 分著播 */
    int have_first; double first_arrival;                     /* 第一包到達 + buffer ms 就開播 */
    ...
} call_t;

static void spk_fill(call_t *c, int16_t *spk, size_t n) {    /* 這裡就是 live 的播放時鐘 */
    while (done < n) {
        if (c->spk_pos >= FRAME_SAMPLES) {
            lock_take(&c->lock);
            if (!c->jb.started && c->have_first && now >= c->first_arrival + c->buffer_ms) jb_start(&c->jb);
            if (c->jb.started) jb_tick(&c->jb, c->spk_cur, now); else memset(c->spk_cur, 0, FRAME_BYTES);
            lock_give(&c->lock);
            c->spk_pos = 0;
        }
        ... 把 spk_cur[spk_pos..] 複製到 spk[done..]，最多 n − done 個 ...
    }
}
static void audio_cb(const int16_t *mic, int16_t *spk, size_t n, void *user) {
    call_t *c = (call_t *)user;                               /* user = 我們交給 audio_start_duplex 的 call_t 位址 */
    if (mic) mic_push_and_send(c, mic, n);                    /* 湊滿 320 → encode → header → udp_send */
    if (spk) spk_fill(c, spk, n);                             /* mic／spk 可能是 NULL（退路模式分開叫） */
}
static THREAD_FN(recv_thread) { while (c->running) { udp_recv(..., 200, ...); parse; decode; lock; jb_put; unlock; } }
```

**怎麼測**

| macOS / Linux | Windows PowerShell |
|---|---|
| `./step7_live devices` | `.\step7_live.exe devices` |
| 同一台兩個視窗：`./step7_live call 5001 127.0.0.1 5002` 與 `./step7_live call 5002 127.0.0.1 5001` | `.\step7_live.exe call 5001 127.0.0.1 5002` 與 `.\step7_live.exe call 5002 127.0.0.1 5001` |
| 兩台電腦（手機熱點）：A `./step7_live call 5001 <B的IP> 5001 --codec mulaw`、B 反過來 | 同左（Windows 防火牆第一次會問，允許「私人網路」） |
| 夾搗蛋：中繼 `udp_impair.py 5005 127.0.0.1 5002 --loss 0.05 --jitter 30`，A 改送 5005 | 同左 |

**看到什麼才算對**

```
音訊：duplex 裝置已啟動（錄 ...／放 ...）
通話中：我聽 port 5001，送到 127.0.0.1:5002，buffer=60 ms，plc=repeat，codec=pcm。請戴耳機。按 Enter 結束。
（按 Enter）
結束：送出 150 包、收到 148 包、補了 0 個 frame、壞封包 0
STATS role=recv buffer_ms=60 plc=repeat codec=pcm frames_out=142 received=148 lost=0 late=0 dup=0 reordered=0 overflow=0 concealed=0 avg_delay_ms=99.8 max_delay_ms=103.0
STATS role=send codec=pcm frames=150 bytes=97800 duration_ms=3000 pace=20
```

兩台電腦戴耳機講話 30 秒聽得懂（單向也算，進階層 A4）。`avg_delay_ms` ≈ buffer + 喇叭的輸出 buffer（約 40 ms），這是報告第 4 題「延遲由哪些部分組成」的一項。
`received` 比 `frames_out` 多幾包是正常的：按 Enter 時 buffer 裡還有沒播完的。

**常見錯誤**

- 沒戴耳機 → 喇叭的聲音進麥克風再送回去，回音越來越大（規格不要求回音消除）。
- 在 callback 裡 `printf` 每一包 → 超過 20 ms，聲音斷斷續續。鎖裡、callback 裡只做快的事。
- 忘了 `audio_stop()` 就 `thread_join` 或 `udp_close` → callback 還在跑、用到已經關掉的 socket，結束時當掉。
- 校園 Wi-Fi 擋住電腦之間的 UDP（client isolation）→ 用手機熱點；`devices` 列不出東西 → 沒有輸入裝置，`call` 回結束碼 1。

---

## 接下來：變成你們自己的 voicelink

1. 把 `step7_live.c` 改名放進 team repo 的 `src/`，`wrapper/` 的四個標頭放 `include/`（或照 baseline 的路徑 `-I`），Makefile 照 `tutorial/Makefile` 改。
   一支程式同時有 `send`／`recv`／`call`／`devices`，就是規格的 `voicelink`。
2. `tests/` 放 `run_mock_tests.py` 的呼叫腳本（`make test`），四種組合都跑：自己對自己、Python 送我收、我送 Python 收。
3. 報告兩題：buffer 設多少（用 `avg_delay_ms`／`concealed` 掃 20–200 ms）、`repeat` 與 `zero` 的差別（用耳朵和 `concealed` 的數字）。
4. 加分：ADPCM（`python_ref` 有做法）、PLC 衰減或淡入淡出（改 `jb_tick` 的 PLC 分支）。

| 步驟 | 要過的測試（`--only`） | 規格的驗收項目 |
|---|---|---|
| step3 | 1,2,5 | 基本層 B1 B2 B3（B4 的四個數） |
| step4 | 1,2,3,5,7 | 進階層 A1、加分層的計數器 |
| step5 | 1,2,3,4,5,6,7 | 加分層：dup、壞封包 |
| step6 | 8 | 進階層 A3 μ-law |
| step7 | 現場 | 基本層 B0、進階層 A4 live |
