## m5_play_wav.c：透過 callback 播一個 WAV

[← 教學總覽](../TUTORIAL.md)｜上一個：[m4 錄音](../m4_mic_to_wav/README.md)｜下一個：[m6 header 打包](../m6_header/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m5_play_wav` | `cd team_projects\team2_voicelink\tutorial\m5_play_wav` |
| `make`（編出 `./m5_play_wav`（Windows `.\m5_play_wav.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m5_play_wav.c`](m5_play_wav.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。
這支程式用到麥克風／喇叭，`make` 會先把 `../../baseline/wrapper/audio.c`（miniaudio）編成 `../audio.o`，第一次要 10–20 秒。

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
