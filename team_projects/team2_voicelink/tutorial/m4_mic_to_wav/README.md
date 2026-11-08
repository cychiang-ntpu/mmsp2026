## m4_mic_to_wav.c：錄 3 秒麥克風

[← 教學總覽](../TUTORIAL.md)｜上一個：[m3 20 ms 時鐘](../m3_clock/README.md)｜下一個：[m5 播放](../m5_play_wav/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m4_mic_to_wav` | `cd team_projects\team2_voicelink\tutorial\m4_mic_to_wav` |
| `make`（編出 `./m4_mic_to_wav`（Windows `.\m4_mic_to_wav.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m4_mic_to_wav.c`](m4_mic_to_wav.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。
這支程式用到麥克風／喇叭，`make` 會先把 `../../baseline/wrapper/audio.c`（miniaudio）編成 `../audio.o`，第一次要 10–20 秒。

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
