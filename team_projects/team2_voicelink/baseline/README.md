# baseline/：Team 2 VoiceLink 的起點（文字聊天＋音訊／UDP wrapper＋最天真的語音串流）

這個資料夾裡的東西全部都能直接編譯、執行。規格（封包格式、STATS、驗收）在上一層的
[`../README.md`](../README.md)；這裡只說明**這些檔案是什麼、怎麼跑、跑起來會看到什麼**。

## 檔案

| 檔案 | 是什麼 | 要不要改 |
|---|---|---|
| `voicelink_raw.c` | **最天真的 UDP 語音串流**：`send`／`recv`／`call`／`devices` 四個指令。640 bytes 一包、沒有 header、收到什麼就寫什麼。檔案開頭的註解列出「要加的東西各放在哪個函式」 | 在它上面改，或只拿 wrapper 重寫 |
| `wrapper/wav.h` | 讀寫 WAV（照 chunk 走，`data` 不一定在第 44 byte；只收 16-bit PCM）：`wav_read`／`wav_write`／`wav_free` | 完成的 |
| `wrapper/udp.h` | UDP socket：`udp_open`／`udp_set_peer`／`udp_send`／`udp_recv`（有逾時）／`udp_close`，加上 big-endian 的 `put_u16`／`put_u32`／`get_u16`／`get_u32`（寫 header 會用到）。Windows／POSIX 的差異都抹平了 | 完成的 |
| `wrapper/clock.h` | `clock_ms`（單調時鐘）、`sleep_ms`、`sleep_until_ms`（準確的 20 ms 節奏靠它） | 完成的 |
| `wrapper/audio.h`／`audio.c` | 麥克風與喇叭：`audio_start_duplex(callback)`／`audio_stop`／`audio_list_devices`。底下是 miniaudio | 完成的 |
| `wrapper/miniaudio.h` | 第三方的單一檔頭音訊函式庫（見最下面的授權說明） | 不要改 |
| `udp_impair.py` | 搗蛋的中繼：夾在 send 與 recv 中間製造遺失、抖動、亂序、重複。評測用同一支 | 不要改 |
| `smoke_check.py` | `make smoke` 用：比對 send → recv 前後的 PCM 是否逐 byte 相同 | — |
| `chat.c` | 第 2 週的文字聊天（TCP／UDP），Team 3 整合時要用 | 這一輪不用改 |
| `Makefile` | `make`／`make smoke`／`make clean` | 自己加 .c 檔時改 |

## 建置與執行

| macOS／Linux（終端機） | Windows（PowerShell，MSYS2 的 gcc） |
|---|---|
| `cd team_projects/team2_voicelink/baseline` | `cd team_projects\team2_voicelink\baseline` |
| `make` | `mingw32-make` |
| `./voicelink_raw devices` | `.\voicelink_raw devices` |
| `./voicelink_raw recv 5004 out.wav`（視窗 1） | `.\voicelink_raw recv 5004 out.wav`（視窗 1） |
| `./voicelink_raw send 127.0.0.1 5004 ../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav`（視窗 2） | `.\voicelink_raw send 127.0.0.1 5004 ..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav`（視窗 2） |
| `./voicelink_raw call 5004 <對方IP> 5004`（兩台電腦各跑一次，戴耳機，Enter 結束） | `.\voicelink_raw call 5004 <對方IP> 5004` |
| `make smoke`（自動跑一次 devices ＋ 127.0.0.1 的 send→recv ＋ 比對） | 在 **Git Bash** 裡 `mingw32-make smoke`（PowerShell 不能把 recv 放到背景） |
| `make clean` | `mingw32-make clean` |

第一次 `make` 編 `wrapper/audio.c` 要 10–20 秒（miniaudio 有十萬行）；之後沒改它就不會重編。
三個平台都是 `-std=c99 -Wall -Wextra` 零警告；連結的函式庫 Makefile 會自己選
（Windows `-lws2_32 -lwinmm`、Linux `-ldl -lpthread -lm`、macOS `-lpthread -lm`；音訊後端由 miniaudio 在執行時自己載入）。

`send` 只接受 16 kHz／16-bit／單聲道的 WAV，不是的話印錯誤、結束碼 2。`recv` 在第一包之後 2000 ms（`--timeout`）
沒有封包就自己結束並寫出 WAV；第一包最多等 30 秒。沒給參數印用法、結束碼 2。

`make smoke` 正常的輸出結尾長這樣：

```
sent 352 packets in 7045 ms
received 352 packets, 225280 bytes
PASS: 225082 bytes 的 PCM 逐 byte 相同（352 包，最後一包補 198 bytes 的 0）
```

## raw 版做了什麼、沒做什麼

- `send`：讀 WAV → 每 320 個 sample（640 bytes）當一包 → `sendto` → `sleep_until_ms(t0 + 20·k)`。最後一包不足 640 補 0。
- `recv`：`udp_recv` 收到什麼就接在後面（**到達順序 = 播放順序**）→ 寫成 WAV。
- `call`：麥克風的 callback 湊滿 320 個 sample 就 `sendto`；另一條執行緒把收到的包放進一個 8 格的 ring；
  喇叭的 callback 從 ring 拿一包來播，ring 空的就播靜音。
- **沒有**：header、seq／timestamp、jitter buffer、PLC（packet loss concealment，封包遺失補償）、BYE、STATS。這些就是你們的作業；`voicelink_raw.c`
  開頭的註解說了每一樣該放在哪個函式裡。

## 把 udp_impair.py 夾在中間：會壞在哪裡

`udp_impair.py` 自己聽一個 port、把收到的每一包轉給另一個位址，途中照機率搗蛋：

```
python3 udp_impair.py <listen_port> <fwd_ip> <fwd_port> --loss p --jitter ms --reorder p --dup p [--seed n]
```

| 參數 | 意思 |
|---|---|
| `--loss p` | 機率 p 直接丟掉這一包 |
| `--jitter ms` | 每一包隨機延遲 0～ms 毫秒再轉送（後面的包抽到比較短就會超車 → 本身也會造成亂序） |
| `--reorder p` | 機率 p 把這一包扣住，等下一包送出後才送（保證至少和一包對調） |
| `--dup p` | 機率 p 多送一份複本 |
| `--seed n` | 固定亂數種子，每次跑結果一樣（寫自動測試用） |

同一台電腦、三個視窗（Windows 用 `python`、`.\`）：

```
視窗 1   ./voicelink_raw recv 5004 out.wav
視窗 2   python3 udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1 --dup 0.02
視窗 3   ./voicelink_raw send 127.0.0.1 5005 ../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav
```

然後用耳朵聽 `out.wav`，再用 `python3 smoke_check.py speech_osr_16k.wav out.wav` 看數字（會 FAIL，而且會告訴你第幾包開始不同）：

| 搗蛋 | raw 版的下場 | 為什麼 |
|---|---|---|
| `--loss 0.05` | `received` 比 `sent` 少 5%，WAV 變短，聽起來像「吃字」；`call` 時喇叭每隔一陣子空 20 ms（破音、卡卡的） | 沒有 seq，不知道掉了哪一包；也沒有 PLC 可以補 |
| `--reorder 0.1` | 每十包就有兩段 20 ms 的聲音對調：單字裡的音節會「口吃」；WAV 長度正確但內容不對 | 到達順序就是播放順序，沒有 timestamp 可以排回去 |
| `--jitter 30` | `recv`：WAV 長度正確但部分亂序（抖動造成超車）；`call`：ring 一下空（補靜音）一下滿（丟包），忽快忽慢 | 沒有 jitter buffer 先累積再播，播放時鐘被網路牽著走 |
| `--dup 0.1` | 多出 10% 的包，同一段聲音播兩次，WAV 變長 | 沒有 seq，分不出「重複」和「下一包」 |
| 送端中途 Ctrl-C | `recv` 2 秒後自己結束，WAV 到那裡為止 | 這一點 raw 版就已經做對了（`--timeout`）；你們的版本也要保持 |

正式版（`../README.md`）要的就是把這張表變成：`lost` 用 PLC 補、`reordered` 排回去、`dup` 丟掉、`late` 丟掉並計數，
而且播放時鐘每 20 ms 一拍**永遠不等網路**。

## miniaudio 的授權

`wrapper/miniaudio.h` 是 David Reid 的 [miniaudio](https://miniaud.io/) **v0.11.25**（2026-03-04，版本字串在檔案開頭與
`MA_VERSION_STRING`），授權是 **public domain 或 MIT-0 二選一**（原文在該檔案的最後面），可以直接放進你們的 team repo，
不需要另外附授權檔；報告或 `README.md` 裡提一句「音訊用 miniaudio（public domain／MIT-0）」就好。
`wrapper/audio.c` 是整支程式裡**唯一**定義 `MINIAUDIO_IMPLEMENTATION` 的檔案；其他檔案只要 `#include "wrapper/audio.h"`，
不要再 include `miniaudio.h`。
