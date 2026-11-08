## m3_clock.c：每 20 ms 醒來一次，誤差多少？

[← 教學總覽](../TUTORIAL.md)｜上一個：[m2 UDP 明信片](../m2_udp/README.md)｜下一個：[m4 錄音](../m4_mic_to_wav/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m3_clock` | `cd team_projects\team2_voicelink\tutorial\m3_clock` |
| `make`（編出 `./m3_clock`（Windows `.\m3_clock.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m3_clock.c`](m3_clock.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


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
