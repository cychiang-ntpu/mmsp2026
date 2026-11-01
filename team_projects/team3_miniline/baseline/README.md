# baseline/：Team 3 MiniLINE 的起點（JPEG／攝影機／視窗 wrapper＋最天真的視訊串流）

這個資料夾裡的東西全部都能直接編譯、執行。規格（封包格式、重組、queue、STATS、驗收）在上一層的
[`../README.md`](../README.md)；這裡只說明**這些檔案是什麼、怎麼跑、跑起來會看到什麼**。

## 檔案

| 檔案 | 是什麼 | 要不要改 |
|---|---|---|
| `miniline_raw.c` | **最天真的 UDP 視訊串流**：`video-send`／`video-recv`／`camera-test`／`devices` 四個指令。一張 JPEG 一個 datagram、沒有 header、收到什麼就寫什麼。檔案開頭的註解列出「要加的東西各放在哪裡」 | 在它上面改，或只拿 wrapper 重寫 |
| `make_sequence.c` | 產生測試用的 JPEG 序列：`make_sequence <outdir> [--frames 60] [--quality 75] [--seed 1]`。會動的漸層＋彈跳的球＋右下角的 frame 編號；同一個 seed 每次產生的檔逐 byte 相同。評測的序列也是這樣做的 | 不用改 |
| `wrapper/jpeg.h`／`jpeg.c` | JPEG 編解碼：`jpeg_encode(rgb, w, h, quality, &out, &len)`、`jpeg_decode(bytes, len, &rgb, &w, &h)`、`jpeg_free`。底下是 stb；`jpeg.c` 是**唯一**定義 `STB_IMAGE_IMPLEMENTATION` 的檔案 | 完成的 |
| `wrapper/camera.h`＋`camera_win.c`／`camera_mac.m`／`camera_linux.c` | 攝影機：`camera_count`／`camera_name`／`camera_open(index, w, h)`／`camera_read(rgb, timeout_ms)`／`camera_close`，輸出固定 320×240 RGB（裝置給別的尺寸就裁切縮放）。Windows 用 Media Foundation、macOS 用 AVFoundation、Linux 用 V4L2。沒有攝影機時 `camera_count()` 是 0、`camera_open` 回傳 -1 | 完成的 |
| `wrapper/camera_util.h` | 三個 camera 後端共用：YUYV／NV12 → RGB、裁切＋縮放 | 完成的 |
| `wrapper/display.h`＋`display_win.c`／`display_mac.m`／`display_linux.c`／`display_none.c` | 視窗：`display_open(w, h, title)`／`display_show(rgb)`／`display_poll()`（回傳 0 = 視窗被關）／`display_close`。Windows GDI、macOS Cocoa、Linux X11；沒有桌面時 `display_open` 回傳 -1，程式退回只寫檔 | 完成的 |
| `wrapper/udp.h`、`clock.h`、`wav.h`、`audio.h`／`audio.c`、`miniaudio.h` | 和 Team 2 的 baseline 一模一樣（UDP、時鐘、WAV、麥克風喇叭） | 完成的 |
| `wrapper/stb_image.h`、`stb_image_write.h` | 第三方的單一檔頭 JPEG 函式庫（授權見最下面） | 不要改 |
| `chat.c` | 第 2 週的文字聊天（TCP），`call` 整合時沿用 | 這一輪整合時改 |
| `udp_impair.py` | 搗蛋的中繼（和 Team 2 的同一支，複製過來的）：遺失、抖動、亂序、重複。評測用同一支 | 不要改 |
| `smoke_check.py` | `make smoke` 用：數 send → recv 到了幾張、幾張逐 byte 相同 | — |
| `Makefile` | `make`／`make smoke`／`make clean`；依作業系統挑 camera／display 的後端與連結的函式庫 | 自己加 .c 檔時改 |

## 建置與執行

| macOS／Linux（終端機） | Windows（PowerShell，MSYS2 的 gcc） |
|---|---|
| `cd team_projects/team3_miniline/baseline` | `cd team_projects\team3_miniline\baseline` |
| `make` | `mingw32-make` |
| `./miniline_raw devices` | `.\miniline_raw devices` |
| `./make_sequence seq --frames 60` | `.\make_sequence seq --frames 60` |
| `./miniline_raw video-recv 5004 out --display window`（視窗 1） | `.\miniline_raw video-recv 5004 out --display window`（視窗 1） |
| `./miniline_raw video-send 127.0.0.1 5004 seq`（視窗 2） | `.\miniline_raw video-send 127.0.0.1 5004 seq`（視窗 2） |
| `./miniline_raw video-send <對方IP> 5004 camera`（攝影機 → 對方） | `.\miniline_raw video-send <對方IP> 5004 camera` |
| `./miniline_raw camera-test --display window --seconds 5 --out cam.jpg [--camera 1]` | `.\miniline_raw camera-test --display window --seconds 5 --out cam.jpg [--camera 1]` |
| `make smoke`（自動：產生 20 張 ＋ 127.0.0.1 的 send→recv ＋ 數張數） | 在 **Git Bash** 裡 `mingw32-make smoke`（PowerShell 不能把 recv 放到背景） |
| `make clean` | `mingw32-make clean` |

第一次 `make` 編 `wrapper/audio.c` 要 10–20 秒（miniaudio 有十萬行）；之後沒改它就不會重編。
三個平台都是 `-std=c99 -Wall -Wextra` 零警告（只有 `wrapper/jpeg.c` 多一個 `-Wno-unused-parameter`，
那是 stb 第三方程式碼裡的一個警告；見 Makefile 的說明）。連結的函式庫 Makefile 會自己選：

| 平台 | 連結 | 額外要裝的東西 |
|---|---|---|
| Windows | `-lws2_32 -lwinmm -lmfplat -lmf -lmfreadwrite -lmfuuid -lstrmiids -lole32 -lgdi32 -luser32` | 沒有（MSYS2 的 MinGW 都有） |
| macOS | `-framework AVFoundation CoreMedia CoreVideo Foundation Cocoa`，`.m` 檔用 clang、不用 ARC | 沒有。第一次用攝影機會問「終端機想要存取相機」，按允許 |
| Linux | `-ldl -lpthread -lm`，有 X11 時再加 `-lX11` | 視窗要 `sudo apt install libx11-dev`；沒裝的話 `make` 會印一行提示並改用 `display_none.c`（只寫檔不開視窗，mock 模式與評測 1–7 都不受影響）。攝影機要在 `video` 群組 |

沒給參數印用法、結束碼 2；網路或裝置錯誤結束碼 1；正常 0。`video-recv` 在第一包之後 `--idle`（預設 3000 ms）沒有封包就自己結束；第一包最多等 30 秒。

`make smoke` 正常的輸出結尾長這樣：

```
sent 20 frames (315473 bytes) in 4005 ms, 0 failed
received 20 datagrams, 315473 bytes, written to smoke_recv/0000.jpg..0019.jpg
exit codes: send=0 recv=0
原始 20 張；收到 20 張，其中合法 JPEG 20 張、和原始逐 byte 相同 20 張
PASS: 收到 20 張合法 JPEG（raw 版：20/20 張逐 byte 相同）
```

## raw 版做了什麼、沒做什麼

- `video-send`：讀 `NNNN.jpg`（或攝影機 → `jpeg_encode`）→ **整張當一個 datagram** `sendto` → `sleep_until_ms(t0 + 200·k)`。
- `video-recv`：`udp_recv` 收到一包就寫成 `outdir/NNNN.jpg`（**到達順序 = 檔名順序**）；`--display window` 時順便 `jpeg_decode` 畫出來。
- **沒有**：header、切片／重組、逾時、bounded queue、顯示時鐘、HELLO／BYE／ssrc、壞封包檢查、STATS、`call`。這些就是你們的作業；
  `miniline_raw.c` 開頭的註解說了每一樣該放在哪裡。

## 這支 raw 版會壞在哪裡：IP 分片

一張 320×240 的 JPEG 大約 10–25 KB。UDP 允許一個 datagram 最大 65 507 bytes，所以 `sendto` 一張 16 KB 的 JPEG 會成功——
但乙太網路／Wi-Fi 一次只能載 **1 500 bytes**（MTU），IP 層會把這個 datagram **切成十幾個分片**（fragment）送出去，
對方的 IP 層再拼回來。規則是：**任何一個分片不見，整個 datagram 就不見**，應用程式連「收到一半」都看不到。

| 情況 | raw 版的下場 | 為什麼 |
|---|---|---|
| `127.0.0.1` | 20 張全到、逐 byte 相同（`make smoke`） | 不經過網卡，沒有 MTU 的問題（loopback 的 MTU 很大、也不會掉） |
| 兩台電腦、Wi-Fi 掉 2% 的封包 | 一張 14 片的 JPEG 有 1 − 0.98¹⁴ ≈ **25%** 的機率整張消失；畫面一卡一卡 | 遺失被「放大」了：片的遺失率 p → 整張的損失率 1 − (1 − p)^k |
| 經過 `udp_impair.py --loss 0.02` | 看起來反而正常？ | `udp_impair.py` 是在 UDP 層轉送（它收到的是已經拼好的整個 datagram），搗蛋的是「整張」而不是「分片」。所以評測是用它對**你們切好的 1 200 bytes 的片**搗蛋，才會有「缺片的 frame 要丟掉」這件事 |
| 有些路由器／手機熱點 | 一張都到不了 | 它們根本不轉送 IP 分片（安全政策） |
| 接收端 | 不知道這是第幾張、掉了幾張、對方掛了沒 | 沒有 header、沒有 seq／frame_id、沒有 BYE |

正式版（`../README.md`）要的就是：**自己切成 ≤ 1 200 bytes 的片**（加 16 bytes header 後 < 1 280，不會被 IP 層再切）、
接收端**自己重組**、缺片的 frame **逾時丟掉**、顯示端有自己的時鐘與 **bounded queue**。
這樣掉 2% 的片時，掉的是「那幾張」，其他的照樣清楚；而且你能數出來（`pieces_lost`、`frames_dropped_timeout`）。

## camera-test 與 devices

- `miniline_raw devices`：列出攝影機（`camera_name`）、喇叭、麥克風（`audio_list_devices`）。
- `miniline_raw camera-test --display window --seconds 5 --out cam.jpg [--camera i]`：打開第 i 台攝影機（預設 0；編號看 `devices`）抓 5 秒，
  開得了視窗就即時顯示，
  最後一張寫成 `cam.jpg`（320×240、quality 75）。用來確認「攝影機能開、方向顏色對、視窗會動」，live 整合前先跑這個。
- 攝影機通常 30 fps，`camera_read` 給的是「最新的一張」，中間沒拿的會丟掉——所以 5 fps 的 `video-send camera` 不會越來越慢。
- 攝影機被別的程式占用、或隱私權設定沒開時，`camera_open` 印錯誤、結束碼 1。Windows 上設環境變數 `ML_CAMERA_DEBUG=1`
  會印出攝影機的每一種原生格式與選擇過程（有些筆電的驅動會列出 180 fps 這種假模式，baseline 會一個個試到能開為止）。
- 畫面一片黑？USB 攝影機會記住別的程式設的「手動曝光」；Windows 的 `camera_open` 會把曝光設回自動（`ML_CAMERA_DEBUG=1`
  看得到），macOS／Linux 由系統自動處理。
- 有兩台以上攝影機（筆電內建＋外接、或內建的 IR 鏡頭也被列出來）時，`devices` 的編號就是 `--camera` 的編號；
  `video-send ... camera --camera 1` 也可以選。
- 沒有攝影機的組：`video-send ... <jpegdir>` 用序列代替；評測 1–7 完全不需要攝影機。

## 授權

- `wrapper/stb_image.h`（**v2.30**）與 `wrapper/stb_image_write.h`（**v1.16**）是 Sean Barrett 的
  [stb](https://github.com/nothings/stb)，
  授權 **public domain 或 MIT 二選一**（原文在各檔案的最後面），可以直接放進 team repo，不需要另外附授權檔。
  `wrapper/jpeg.c` 是整支程式裡**唯一**定義 `STB_IMAGE_IMPLEMENTATION`／`STB_IMAGE_WRITE_IMPLEMENTATION` 的檔案；
  其他檔案只要 `#include "wrapper/jpeg.h"`，不要再 include `stb_*.h`。
- `wrapper/miniaudio.h` 是 David Reid 的 [miniaudio](https://miniaud.io/) **v0.11.25**，授權 **public domain 或 MIT-0 二選一**；
  `wrapper/audio.c` 是唯一定義 `MINIAUDIO_IMPLEMENTATION` 的檔案（和 Team 2 相同）。
- 報告或 `README.md` 裡提一句「JPEG 用 stb（public domain／MIT）、音訊用 miniaudio（public domain／MIT-0）」就好。
