## m1_wav.c：讀 WAV、印格式、寫出前 1 秒

[← 教學總覽](../TUTORIAL.md)｜下一個：[m2 UDP 明信片](../m2_udp/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m1_wav` | `cd team_projects\team2_voicelink\tutorial\m1_wav` |
| `make`（編出 `./m1_wav`（Windows `.\m1_wav.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m1_wav.c`](m1_wav.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


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
