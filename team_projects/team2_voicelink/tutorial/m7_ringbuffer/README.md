## m7_ringbuffer.c：沒有網路的 jitter buffer（step3 的心臟）

[← 教學總覽](../TUTORIAL.md)｜上一個：[m6 header 打包](../m6_header/README.md)｜下一個：[m8 μ-law](../m8_mulaw/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m7_ringbuffer` | `cd team_projects\team2_voicelink\tutorial\m7_ringbuffer` |
| `make`（編出 `./m7_ringbuffer`（Windows `.\m7_ringbuffer.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m7_ringbuffer.c`](m7_ringbuffer.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


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
