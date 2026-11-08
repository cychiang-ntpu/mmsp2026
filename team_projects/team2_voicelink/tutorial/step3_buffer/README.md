## 第 3 步｜step3_buffer.c：jitter buffer、播放時鐘、PLC——整支程式的核心

[← 教學總覽](../TUTORIAL.md)｜上一個：[第 2 步 header](../step2_header/README.md)｜下一個：[第 4 步 counters](../step4_counters/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/step3_buffer` | `cd team_projects\team2_voicelink\tutorial\step3_buffer` |
| `make`（編出 `./step3_buffer`（Windows `.\step3_buffer.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`step3_buffer.c`](step3_buffer.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


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
- **STATS**：規格的一行，這一步先填 `frames_out`／`received`／`lost`／`concealed`，其他印 0（第 4 步再補齊）。
  `lost` = 從第一個播的 seq 到結尾「應該有幾包」− 「真的到過幾包」（`unique`）。

**程式碼**（`diff ../step2_header/step2_header.c step3_buffer.c`；整個 buffer 包成 `jb_t` 和五個函式，第 7 步會原封不動拿去 live 用）

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
| `python3 ../../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` | `python ..\..\baseline\udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` |
| `python3 ../../python_ref/voicelink.py send 127.0.0.1 5005 $W` | `python ..\..\python_ref\voicelink.py send 127.0.0.1 5005 $W` |

自動驗收（一個視窗就好，腳本自己開 recv／中繼／send；`--only 1,2,5` 是這一步要過的三項）：

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../../tests/run_mock_tests.py "python3 ../../python_ref/voicelink.py send" "./step3_buffer recv" $W --only 1,2,5` | `python ..\..\tests\run_mock_tests.py "python ..\..\python_ref\voicelink.py send" ".\step3_buffer.exe recv" $W --only 1,2,5` |
| `python3 ../../tests/run_mock_tests.py "./step3_buffer send" "python3 ../../python_ref/voicelink.py recv" $W --only 1,2,5` | `python ..\..\tests\run_mock_tests.py ".\step3_buffer.exe send" "python ..\..\python_ref\voicelink.py recv" $W --only 1,2,5` |

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
