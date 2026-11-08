## 第 4 步｜step4_counters.c：把每一包都算清楚（late／dup／reordered／overflow／延遲）

[← 教學總覽](../TUTORIAL.md)｜上一個：[第 3 步 jitter buffer](../step3_buffer/README.md)｜下一個：[第 5 步 robust](../step5_robust/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/step4_counters` | `cd team_projects\team2_voicelink\tutorial\step4_counters` |
| `make`（編出 `./step4_counters`（Windows `.\step4_counters.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`step4_counters.c`](step4_counters.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


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

**程式碼**（`diff ../step3_buffer/step3_buffer.c step4_counters.c`）

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
| `python3 ../../tests/run_mock_tests.py "python3 ../../python_ref/voicelink.py send" "./step4_counters recv" $W --only 1,2,3,5,7` | `python ..\..\tests\run_mock_tests.py "python ..\..\python_ref\voicelink.py send" ".\step4_counters.exe recv" $W --only 1,2,3,5,7` |
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
