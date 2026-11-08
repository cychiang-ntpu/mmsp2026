# baseline/：Team 2 的 wrapper 與測試工具（不是程式起點）

能通話的完整程式在 [`../tutorial/step6_live/`](../tutorial/step6_live/README.md)，要加壓縮的框架在 [`../mulaw_system/`](../mulaw_system/README.md) 與 [`../adpcm_huffman_system/`](../adpcm_huffman_system/README.md)；手把手教學在 [`../tutorial/TUTORIAL.md`](../tutorial/TUTORIAL.md)。
這個資料夾只放**所有程式都要用、但你們不用改**的東西：

| 檔案 | 是什麼 | 誰用 |
|---|---|---|
| `wrapper/wav.h` | 讀寫 WAV（照 chunk 走，`data` 不一定在第 44 byte；只收 16-bit PCM）：`wav_read`／`wav_write`／`wav_free` | m1、m4、m5、m8、m9、所有 step |
| `wrapper/udp.h` | UDP socket：`udp_open`／`udp_set_peer`／`udp_send`／`udp_recv`（有逾時）／`udp_close`，加上 big-endian 的 `put_u16`／`put_u32`／`get_u16`／`get_u32`。Windows（Winsock）／POSIX 的差異都抹平了；**一定要是第一個 `#include`** | 每一支 |
| `wrapper/clock.h` | `clock_ms`（單調時鐘）、`sleep_ms`、`sleep_until_ms`（準確的 20 ms 節奏靠它） | m3、所有 step |
| `wrapper/audio.h`／`audio.c` | 麥克風與喇叭：`audio_start_duplex(callback)`／`audio_stop`／`audio_list_devices`。底下是 miniaudio；`audio.c` 是整支程式裡**唯一**定義 `MINIAUDIO_IMPLEMENTATION` 的檔案 | m4、m5、step6、兩個系統 |
| `wrapper/miniaudio.h` | 第三方的單一檔頭音訊函式庫（授權見下） | 不要改 |
| `udp_impair.py` | 搗蛋的中繼：夾在 send 與 recv 中間製造遺失、抖動、亂序、重複；`../tests/run_mock_tests.py` 與評測都用它 | 自己測、評測 |
| `smoke_check.py` | 比對兩個 WAV 的 PCM 是否逐 byte 相同（會告訴你第幾包開始不同） | tutorial 第 1 步 |
| `Makefile` | `make` 只把 `wrapper/audio.c` 編成 `audio.o`，確認 wrapper 在這台機器零警告 | CI |

team repo 照 `team2_voicelink/` 的結構繳交，`baseline/wrapper/` 整個帶著走（`mulaw_system/Makefile` 與 `adpcm_huffman_system/Makefile` 用 `-I../baseline/wrapper`）。

## udp_impair.py：怎麼搗蛋

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

同一台電腦、三個視窗（Windows 用 `python`、`.\`；以 tutorial 第 1 步為例）：

```
視窗 1   ./step1_raw recv 5004 out.wav
視窗 2   python3 ../../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1 --dup 0.02
視窗 3   ./step1_raw send 127.0.0.1 5005 $W
```

| 搗蛋 | 沒有 header／buffer 的程式（第 1 步）會怎樣 | 為什麼 |
|---|---|---|
| `--loss 0.05` | WAV 變短，聽起來像「吃字」 | 沒有 seq，不知道掉了哪一包；也沒有 PLC 可以補 |
| `--reorder 0.1` | 每十包就有兩段 20 ms 的聲音對調：音節「口吃」 | 到達順序就是播放順序，沒有 timestamp 可以排回去 |
| `--jitter 30` | 部分亂序（抖動造成超車）；live 時忽快忽慢 | 沒有 jitter buffer 先累積再播 |
| `--dup 0.1` | 同一段聲音播兩次，WAV 變長 | 沒有 seq，分不出「重複」和「下一包」 |

第 2–7 步就是一項一項把這張表修掉：`lost` 用 PLC 補、`reordered` 排回去、`dup` 丟掉、`late` 丟掉並計數，播放時鐘每 20 ms 一拍**永遠不等網路**。

## miniaudio 的授權

`wrapper/miniaudio.h` 是 David Reid 的 [miniaudio](https://miniaud.io/) **v0.11.25**（2026-03-04，版本字串在檔案開頭與
`MA_VERSION_STRING`），授權是 **public domain 或 MIT-0 二選一**（原文在該檔案的最後面），可以直接放進你們的 team repo，
不需要另外附授權檔；報告或 `README.md` 裡提一句「音訊用 miniaudio（public domain／MIT-0）」就好。
其他檔案只要 `#include "audio.h"`，不要再 include `miniaudio.h`。

（以前這裡還有 `voicelink_raw.c` 與 `chat.c`：前者已被 tutorial 第 1 步取代、11/9 移除；後者在 [team1_textlink/baseline/](../../team1_textlink/baseline/)。）
