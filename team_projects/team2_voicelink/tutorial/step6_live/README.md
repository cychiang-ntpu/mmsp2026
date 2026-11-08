## 第 6 步｜step6_live.c：`call`——麥克風進去、對方喇叭出來（執行緒只在這一步出現）

[← 教學總覽](../TUTORIAL.md)｜上一個：[第 5 步 robust](../step5_robust/README.md)｜下一個：[mulaw_system（你們寫）](../../mulaw_system/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/step6_live` | `cd team_projects\team2_voicelink\tutorial\step6_live` |
| `make`（編出 `./step6_live`（Windows `.\step6_live.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`step6_live.c`](step6_live.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。
這支程式用到麥克風／喇叭，`make` 會先把 `../../baseline/wrapper/audio.c`（miniaudio）編成 `../audio.o`，第一次要 10–20 秒。

**目標**：`call <my_port> <peer_ip> <peer_port>` 兩台電腦戴耳機通話；同一套 jitter buffer 與 PLC；按 Enter 結束印 STATS；`devices` 列裝置。聲音還是 raw PCM（640 bytes 一包）。

**tutorial 到這裡結束**：做到第 6 步、兩台能通話，就是 11/23 demo 的 raw 通話 10 分（之後 μ-law、ADPCM、Huffman 各 +10）。接下來的 μ-law、ADPCM、Huffman 在 [`../../mulaw_system/`](../../mulaw_system/README.md) 與 [`../../adpcm_huffman_system/`](../../adpcm_huffman_system/README.md)——那兩個資料夾給的框架就是這支程式加一張 codec 表。

**用到的模組**：m4（mic 那一半 → 湊 320 → m6 的 header → m2 的 `udp_send`）、m5（spk 那一半 → 向 m7 的 buffer 要 frame）。

**要加的東西（為什麼）**

- **callback**（`audio.h` 的說明）：你不是去「要」聲音，而是把一個函式交給音訊系統，它每 20 ms 在**它自己的執行緒**上呼叫你：
  給你剛錄到的 `n` 個 sample（`mic`），並要你把接下來要播的 `n` 個填進 `spk`。`n` 通常是 320，但不保證，所以 mic 要「湊」、spk 要「分」。
- **audio callback 就是播放時鐘**：mock 模式要自己算 `play_t0 + 20i`，live 不用——喇叭每 320 個 sample 跟你要一次，節奏由硬體決定。
  每湊滿 320 就 `jb_tick` 一次，其他一模一樣（開播條件也一樣：第一包到達 + `--buffer` ms）。
- **為什麼要一條接收執行緒**：mock 的「播放」只是寫記憶體，可以和收封包排在同一個迴圈；live 的播放由音訊系統在它的執行緒上呼叫，
  我們管不了它什麼時候來，所以收封包要另外一條執行緒（`recv_thread`：`udp_recv` 200 ms 逾時 → 解析 → `jb_put`）。

  ```
  main ──── 等 Enter ─────────────────────────────────────────────▶ running=0、BYE×3、audio_stop、join
  recv_thread ── udp_recv → parse → [lock] jb_put [unlock] ─────────▶ 一直到 running=0
  音訊執行緒 ─── 每 20 ms audio_cb：mic → 湊 320 → header → sendto
                                   spk ← 每 320 個 [lock] jb_tick [unlock]
  ```
- **鎖（mutex）**：兩條執行緒都碰 `jb`，不加鎖會在 `jb_put` 寫到一半時被 `jb_tick` 讀到。規則：**任何讀寫 jb 的地方都在 `lock_take` / `lock_give` 之間，
  鎖裡不做慢的事**（不 printf、不等網路）。平台差異（Windows `CRITICAL_SECTION`／POSIX `pthread_mutex`）包成四個小函式，和第 2 週 `chat.c` 同一套。
- **結束**：Enter → `running = 0` → `audio_stop()`（之後 callback 不會再來）→ 送 3 次 BYE → `thread_join`。對方收到 BYE 或看到新的 ssrc 就 `jb_reset`。
- `send`／`recv` 一行都沒改——`diff ../step5_robust/step5_robust.c step6_live.c` 全是「加」。

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
    if (mic) mic_push_and_send(c, mic, n);                    /* 湊滿 320 → header → udp_send */
    if (spk) spk_fill(c, spk, n);                             /* mic／spk 可能是 NULL（退路模式分開叫） */
}
static THREAD_FN(recv_thread) { while (c->running) { udp_recv(..., 200, ...); parse; lock; jb_put; unlock; } }
```

**怎麼測**

| macOS / Linux | Windows PowerShell |
|---|---|
| `./step6_live devices` | `.\step6_live.exe devices` |
| 同一台兩個視窗：`./step6_live call 5001 127.0.0.1 5002` 與 `./step6_live call 5002 127.0.0.1 5001` | `.\step6_live.exe call 5001 127.0.0.1 5002` 與 `.\step6_live.exe call 5002 127.0.0.1 5001` |
| 兩台電腦（手機熱點）：A `./step6_live call 5001 <B的IP> 5001`、B 反過來 | 同左（Windows 防火牆第一次會問，允許「私人網路」） |
| 夾搗蛋：中繼 `udp_impair.py 5005 127.0.0.1 5002 --loss 0.05 --jitter 30`，A 改送 5005 | 同左 |
| mock 模式沒變：`python3 ../../tests/run_mock_tests.py "./step6_live send" "./step6_live recv" $W --only 1,2,3,4,5,6,7` 仍 7/7 | `python ..\..\tests\run_mock_tests.py ".\step6_live.exe send" ".\step6_live.exe recv" $W --only 1,2,3,4,5,6,7` |

**看到什麼才算對**

```
音訊：duplex 裝置已啟動（錄 ...／放 ...）
通話中：我聽 port 5001，送到 127.0.0.1:5002，buffer=60 ms，plc=repeat，codec=pcm。請戴耳機。按 Enter 結束。
（按 Enter）
結束：送出 150 包、收到 148 包、補了 0 個 frame、壞封包 0
STATS role=recv buffer_ms=60 plc=repeat codec=pcm frames_out=142 received=148 lost=0 late=0 dup=0 reordered=0 overflow=0 concealed=0 avg_delay_ms=99.8 max_delay_ms=103.0
STATS role=send codec=pcm frames=150 bytes=97800 duration_ms=3000 pace=20
```

兩台電腦戴耳機講話 30 秒聽得懂（單向也算；11/23 demo 的 raw 通話 10 分）。`avg_delay_ms` ≈ buffer + 喇叭的輸出 buffer（約 40 ms），這是報告第 4 題「延遲由哪些部分組成」的一項。
`received` 比 `frames_out` 多幾包是正常的：按 Enter 時 buffer 裡還有沒播完的。

**常見錯誤**

- 沒戴耳機 → 喇叭的聲音進麥克風再送回去，回音越來越大（規格不要求回音消除）。
- 在 callback 裡 `printf` 每一包 → 超過 20 ms，聲音斷斷續續。鎖裡、callback 裡只做快的事。
- 忘了 `audio_stop()` 就 `thread_join` 或 `udp_close` → callback 還在跑、用到已經關掉的 socket，結束時當掉。
- 校園 Wi-Fi 擋住電腦之間的 UDP（client isolation）→ 用手機熱點；`devices` 列不出東西 → 沒有輸入裝置，`call` 回結束碼 1。

---
