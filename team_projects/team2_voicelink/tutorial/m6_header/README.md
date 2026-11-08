## m6_header.c：12 bytes 的 header，big-endian

[← 教學總覽](../TUTORIAL.md)｜上一個：[m5 播放](../m5_play_wav/README.md)｜下一個：[m7 環形 buffer](../m7_ringbuffer/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m6_header` | `cd team_projects\team2_voicelink\tutorial\m6_header` |
| `make`（編出 `./m6_header`（Windows `.\m6_header.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m6_header.c`](m6_header.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


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
