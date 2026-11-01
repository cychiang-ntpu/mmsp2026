# Team 3｜MiniLINE：視訊（MJPEG 分片）與整合

- 期間：11/23－12/14｜繳交截止：**12/13（日）18:00**｜評測：12/14（三節課內）
- 2026/11/1 公告：因 11/9、11/16 停課，評測改為 12/14（原 12/7）；為了保留三週，**11/23（Team 2 評測當天）提早開題**：規格與 baseline 當天發布、第三節規格導讀，11/30 講影像與 BMP 並答疑
- 團隊 repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、
  `docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`
- 繳交：登錄 repo URL＋完整 commit SHA
- 本輪四人組角色：**P＋D＋V＋V**（三人組 P＋D＋V）；分組名單於 11/23 公告

## 一句話

把攝影機的畫面變成 **320×240、5 fps 的 MJPEG**，每張 JPEG **切成 ≤ 1 200 bytes 的片**用 UDP 送；
接收端把片**重組**成完整的 frame，**缺片的 frame 逾時就丟**、顯示來不及就從**有上限的 queue** 丟最舊的，
而且**視訊再塞也不能把文字和語音卡住**；掛斷之後要能再連。文字沿用 Team 1、語音沿用 Team 2，三條管線**同時跑**就是 MiniLINE。

## 和前兩輪的差別

| | Team 1 文字 | Team 2 語音 | Team 3 視訊 |
|---|---|---|---|
| 傳輸 | TCP，frame 長度前綴 | UDP，一包 = 一個 20 ms frame | UDP，**一個 frame = 很多包** |
| 掉包 | TCP 重傳 | 補一個 frame（PLC） | 整個 frame 都不能用 → **丟掉整張**，等下一張 |
| 時間 | 不急 | 20 ms 一拍 | 200 ms 一張；顯示跟不上就**丟舊的** |
| 大小 | 任意 | 固定 640 bytes | JPEG 每張大小不同（10–25 KB），要切片 |

新東西只有三個：**分片／重組**、**frame 逾時與 bounded queue**、**掛斷／重連**。JPEG 的編解碼由 wrapper 提供（stb），不用自己寫。

## 三個功能

### 功能 1｜mock 模式：JPEG 序列 → 封包 → JPEG 序列（評測用，不需要攝影機）

- `video-send` 讀一個資料夾裡的 `0000.jpg`、`0001.jpg`…，每 200 ms 送一張（切片後逐片送）。
- `video-recv` 收片、重組、逾時丟棄、bounded queue，照 5 fps 的節奏「顯示」（mock 模式是把完整的 frame 寫成 `outdir/NNNN.jpg`，並在 `outdir/shown.txt` 記下每一張顯示的 frame_id 與時間），結束印 STATS。
- 網路正常時，收到的每張 JPEG 要和原檔**逐 byte 相同**（只是切片再接起來，沒有重新壓縮）。
- 中間可以插 Team 2 的 [`udp_impair.py`](../team2_voicelink/baseline/udp_impair.py) 製造遺失、亂序、抖動；評測就是這樣跑的。

### 功能 2｜live 模式：攝影機 → 對方螢幕，三條管線同時

- `call`：攝影機 → JPEG → 切片 → UDP → 對方視窗顯示；同時文字（TCP，Team 1 的 frame）與語音（UDP，Team 2 的管線）在各自的 port 上跑。
- 同一套重組、逾時、queue；視訊的 STATS 另外印一行。

### 功能 3｜量測：fps、遺失、丟棄與延遲

- 每次執行都要印 STATS（格式固定）。報告要分析：**片的遺失率 vs 整張 frame 的損失率**、**queue 上限 vs 丟棄數 vs 延遲**（見「量測與報告」）。

## 固定的部分（各組一律相同，評測會照這個跑）

### 影像格式

| 項目 | 值 |
|---|---|
| 解析度 | 320 × 240 |
| 編碼 | baseline JPEG（wrapper 用 stb_image_write 的 JPEG 編碼器，品質參數預設 75）；接收端不重新壓縮，原樣存 |
| 目標 fps | 5（每 200 ms 一張） |
| 一片（piece）的上限 | **1 200 bytes** 的 JPEG 資料（加 16 bytes header 後 < 1 280，不會被乙太網路切） |
| mock 的輸入 | 資料夾裡 `NNNN.jpg`（四位數從 0000 開始），由 baseline 的 `make_sequence` 產生或老師提供 |

### 命令列介面

```
miniline video-send <ip> <port> <jpegdir|camera> [--fps 5] [--quality 75] [--burst] [--loop]
    mock：讀資料夾照 fps 送；camera：用攝影機（live）。--burst 不等 200 ms 一口氣送（看 queue 的用處）
miniline video-recv <port> <outdir> [--bind <ip>] [--queue <n>] [--timeout <ms>] [--idle <ms>] [--display none|window]
    收、重組、逾時丟棄、bounded queue，完整的 frame 寫成 outdir/NNNN.jpg（--display window 時同時開視窗顯示），結束印 STATS
miniline call <my_port> <peer_ip> <peer_port> [--video camera|<jpegdir>] [--queue <n>] [--timeout <ms>] [--no-audio] [--no-video]
    live：文字（TCP my_port）＋語音（UDP my_port+1）＋視訊（UDP my_port+2）同時；/quit 掛斷、/call 重連；結束印三行 STATS
miniline devices
    列出攝影機、麥克風、喇叭
```

| 參數 | 預設 | 意義 |
|---|---|---|
| `--queue <n>` | 3 | 顯示 queue 最多放幾張完整的 frame；滿了丟**最舊**的（計 `frames_dropped_queue`） |
| `--timeout <ms>` | 400 | 一張 frame 從收到第一片起，超過這麼久還沒收齊就丟（計 `frames_dropped_timeout`）；**或**收到的 frame_id 已經比它大 2 以上也丟 |
| `--idle <ms>` | 3000 | video-recv：這麼久沒有任何封包就當作結束（**不可以永久等待**） |
| `--display` | none | `window`：開一個視窗畫出來（wrapper 的 `display.h`）；`none`：只寫檔 |

結束碼：0 正常；1 網路或裝置錯誤；2 參數或檔案錯誤。

### 封包格式（UDP payload）

每個封包 = **16 bytes header ＋ ≤ 1 200 bytes 的 JPEG 片段**。多 byte 欄位 **big-endian**。

```
offset  長度  欄位
 0      1     magic      = 0x4D（'M'）
 1      1     type       = 0x01 PIECE、0x02 BYE（掛斷，payload 空）、0x03 HELLO（開始／重連，payload 空）
 2      2     seq        每送一個封包 +1（所有 type 共用），2^16 繞回；用來算片的遺失率
 4      4     frame_id   第幾張 frame，從 0 開始，每張 +1；重連後重新從 0
 8      2     piece_idx  這是第幾片（從 0）
10      2     piece_cnt  這張共幾片
12      4     ssrc       本次串流的隨機識別碼；收到不同 ssrc 就重置所有狀態（等於對方重連）
16      ≤1200 JPEG 片段  第 piece_idx 片 = JPEG bytes[piece_idx·1200 … )
```

- 每張 JPEG 切成 ⌈大小／1200⌉ 片，最後一片不補 0（長度由 UDP 封包長度得知）。
- 開始送之前送 3 次 HELLO（`frame_id` = 0）；結束送 3 次 BYE（`frame_id` = 共送了幾張）；兩者 `piece_idx` = `piece_cnt` = 0、沒有 payload。
- 接收端收到 HELLO（或新的 ssrc）→ 清空重組表與 queue、frame_id 歸零（同一個 ssrc 的 HELLO 要是 seq 比目前看過的都新才算，亂序遲到的舊 HELLO 忽略）。
- 收到 BYE → 把 queue 裡已完整的顯示完；mock 模式再**等 1 秒**看有沒有新的 HELLO（驗收 7 的「掛斷再連」就是這樣測），沒有就結束；live 模式回到「等待連線」。
- header 不合法（magic 錯、長度 < 16、piece_idx ≥ piece_cnt、片段 > 1 200）直接丟掉並計數，**不可以當掉**。

### 重組、逾時與 bounded queue

```
收到片 ──▶ [重組表：frame_id → 已收到哪些片、第一片的時間] ──完整──▶ [display queue（上限 n）] ──每 200 ms 取最舊──▶ 顯示／寫檔
                 │ 超過 --timeout 還沒收齊、或 frame_id 落後太多 → 丟（timeout）     │ 滿了 → 丟最舊（queue）
                 └ 重複的片 → 忽略；同一張收齊後再來的片 → 忽略
```

- **顯示端有自己的時鐘**：每 200 ms 從 queue 取最舊的一張顯示；queue 空就什麼都不做（畫面停在上一張，計 `display_idle`）。不等網路。
- 重組表要有上限（建議同時最多 8 張），避免被亂丟的 frame_id 吃光記憶體。
- 正常網路下，`--burst` 送會讓 queue 一直滿，`frames_dropped_queue` 變大但程式**不能變慢、不能爆記憶體**——這是 bounded queue 存在的理由。

### 不能被視訊卡住（live）

文字、語音、視訊各用自己的 socket 與執行緒；視訊的重組與 JPEG 解碼**不可以**在收語音的執行緒裡做。驗收會用 `--burst` 把視訊塞爆，同時量語音的 `concealed`：視訊塞爆前後語音的 `concealed` 不能明顯變多（評測看差距 ≤ 2 倍）。

### 掛斷／重連（live）

狀態只要三個：`IDLE`（等對方）→ `CONNECTED`（收到 HELLO 或任何合法片）→ 收到 BYE 或 `--idle` 逾時 → 回 `IDLE`，清掉重組表與 queue。再收到 HELLO 就回 `CONNECTED`。
聊天視窗打 `/quit` 送 BYE 並回 `IDLE`；打 `/call` 重新送 HELLO 開始送。兩邊都要能掛斷、都要能再連。

### 量測輸出（STATS）

程式結束時印到 stderr，欄位順序與名稱不可改：

```
STATS role=video-recv queue=3 timeout_ms=400 frames_shown=58 frames_complete=58 frames_dropped_timeout=2 frames_dropped_queue=0 pieces_received=842 pieces_lost=23 pieces_bad=0 fps_actual=4.9 avg_assemble_ms=38.2 max_assemble_ms=131.0 display_idle=1
STATS role=video-send frames=60 pieces=865 bytes=1021344 fps_target=5 fps_actual=5.0 duration_ms=12010
```

| 欄位 | 意義 |
|---|---|
| `frames_shown` | 真的顯示（或寫檔）的張數（重連後累計，`shown.txt` 會看到 frame_id 重新從 0 開始） |
| `frames_complete` | 收齊的張數（含後來被 queue 丟掉的） |
| `frames_dropped_timeout`／`frames_dropped_queue` | 如上節 |
| `pieces_received`／`pieces_lost`／`pieces_bad` | 收到的合法片數／由 seq 判斷沒收到的封包數／header 不合法的數 |
| `fps_actual` | frames_shown ÷ 顯示的總秒數 |
| `avg_assemble_ms`／`max_assemble_ms` | 一張 frame 從第一片到收齊的時間（只算收齊的） |
| `display_idle` | 顯示時鐘到了但 queue 是空的次數 |

live 模式還要印 Team 1 的文字、Team 2 的語音各自的 STATS 行（原格式不變）。

## 量測與報告

用 `udp_impair.py` 固定 `--jitter 30`，把 `--loss` 從 0 掃到 10%（0、1、2、5、10），每點跑 3 次，記 `pieces_lost`、`frames_dropped_timeout`、`frames_shown`，畫成「片的遺失率 vs 整張 frame 的損失率」。再固定 loss 2%，把 `--queue` 從 1 掃到 10，記 `frames_dropped_queue` 與平均顯示延遲（第一片到達 → 顯示）。報告回答：

1. 片的遺失率 p 時，一張 k 片的 frame 完整的機率是 (1−p)^k；用你們量到的數字驗證，並解釋為什麼視訊對掉包比語音敏感得多。
2. JPEG 品質（`--quality`）調低，每張變小、片數變少，frame 損失率怎麼變？畫面差多少？你們建議的品質是多少？
3. queue 上限變大，丟棄變少但延遲變長；`--burst` 時發生什麼？為什麼不能用「無上限的 queue」？
4. live 時把視訊塞爆，語音的 `concealed` 有沒有變？你們是怎麼做到不互相卡住的（執行緒／socket 的安排）？

## 最低驗收

評測用的 JPEG 序列與 WAV 由老師當天給。項目 0–7 必過；評測會用 `python_ref/miniline.py` 當對手端各跑一次（你們的 send 對它的 recv、它的 send 對你們的 recv）。

0. 跨機器：兩台電腦（不是 127.0.0.1）能互通。
1. `video-send → video-recv`，沒有搗蛋：每張 JPEG **逐 byte 相同**、張數相同，`frames_dropped_timeout=0`。
2. 經過 `udp_impair.py --loss 0.03`：缺片的 frame 被丟、**沒有半張寫出來**（每個寫出的檔都是完整合法的 JPEG）、`frames_dropped_timeout` ≈ 缺片的張數；程式正常結束。
3. 經過 `--jitter 30 --reorder 0.1`：每張逐 byte 相同（片的順序被排回來）。
4. `--burst` 送 60 張：`frames_dropped_queue` > 0，程式不變慢、記憶體不漲、最後 `frames_shown + frames_dropped_queue = frames_complete`。
5. 送端中途被殺掉：recv 在 `--idle` 內自己結束、STATS 有印、已顯示的檔都完整。
6. 壞封包：對 recv 的 port 亂丟 100 個隨機 bytes、以及 `piece_idx ≥ piece_cnt` 的假封包，程式不當掉，之後仍能正常收。
7. 掛斷再連：送 BYE 之後再送 HELLO 與新的串流（新 ssrc），recv 把第二段也收齊，frame_id 從 0 重新算，兩段都正確。
8. live：兩台電腦戴耳機，文字、語音、視訊同時 60 秒；畫面動、聲音聽得懂、文字收得到；當場 `--burst` 視訊，語音的 `concealed` 不明顯變多。
9. 量測圖兩張與報告四題。

## 跨機器測試須知

- 和 Team 2 一樣：UDP 兩邊各自 bind；校園 Wi-Fi 擋 UDP → 手機熱點；Windows 防火牆要允許。
- 視訊的頻寬：5 fps × 20 KB ≈ 800 kb/s，熱點夠用；`--burst` 時會到好幾 Mb/s，這就是壅塞測試。
- 攝影機只有 live 要用；mock 模式與評測 1–7 完全不需要攝影機。電腦沒有攝影機的組，`call --video <jpegdir>` 用序列代替。

## 角色分工（本輪四人組 P+D+V+V、三人組 P+D+V）

- **P（口頭報告）**：架構圖（三條管線、執行緒）、分片格式、逾時與 queue 的設計決策、兩張量測圖、報告四題。
- **D（展示整合）**：`call` 的整合（三條管線、掛斷／重連）、live 展示、攝影機與視窗。
- **V（測試驗證）×2**：一位負責 mock 模式的自動驗收（`tests/`、`udp_impair.py`、壞封包、掛斷重連的腳本）；一位負責量測（兩張圖的腳本與原始數據、live 時語音 `concealed` 的對照實驗）。三人組由一位 V 兼兩者。
- 每個人都要有 C 程式的 commit；個人口試會問你自己寫的部分，也會問隊友寫的部分。

## 建議時程

| 週 | 日期 | 目標 |
|---|---|---|
| 1 | 11/23–11/29 | 建 team repo（`mmsp2026-team3-gXX`）；跑通 baseline 的 `miniline_raw`（整張 JPEG 直接送，> 1 500 bytes 會怎樣？）；定封包格式與 `docs/interface.md`；分片／重組 mock 模式過驗收 1、3 |
| 2 | 11/30–12/6 | 逾時、bounded queue、壞封包、掛斷重連：過驗收 2、4–7；自動化測試 |
| 3 | 12/7–12/13 | live 整合三條管線、兩張量測圖、投影片；**12/13 18:00 前登錄 SHA** |

## baseline/：文字／語音 baseline＋camera／JPEG／display wrapper

[`baseline/`](baseline/)：

| 檔案 | 是什麼 |
|---|---|
| `wrapper/jpeg.h` | 用 stb_image／stb_image_write：`jpeg_encode(rgb, w, h, quality, &out, &len)`、`jpeg_decode(bytes, len, &rgb, &w, &h)`（public domain） |
| `wrapper/camera.h`＋`camera_win.c`／`camera_mac.m`／`camera_linux.c` | 攝影機：`camera_open(w, h)`、`camera_read(rgb)`（320×240 RGB）、`camera_close`；Windows 用 Media Foundation、macOS 用 AVFoundation、Linux 用 V4L2 |
| `wrapper/display.h`＋`display_win.c`／`display_mac.m`／`display_linux.c` | 視窗顯示：`display_open(w, h, title)`、`display_show(rgb)`、`display_poll()`、`display_close`；Windows GDI、macOS Cocoa、Linux X11 |
| `wrapper/udp.h`、`clock.h`、`wav.h`、`audio.h` | 與 Team 2 相同 |
| `chat.c`、`voicelink` 的 python_ref | 文字與語音沿用 Team 1／Team 2 |
| `make_sequence.c` | 產生測試用的 JPEG 序列（會動的圖形＋frame 編號），評測的序列也是這樣做的 |
| `miniline_raw.c` | **最天真的視訊串流**：一張 JPEG 一個 UDP 封包（超過 MTU 會被切、常常整張不見）、沒有 header、收到什麼存什麼；加上 `devices`、`camera-test`（攝影機 → 視窗；電腦有多個攝影機時用 `--camera <編號>` 選）。你們要做的是把分片、重組、逾時、queue、掛斷重連加進去 |
| `udp_impair.py` | 同 Team 2（這裡是一份轉呼叫過去的小程式，參數相同） |

## python_ref/：純 Python 的完整版（對照用、評測的對手端）

[`python_ref/miniline.py`](python_ref/miniline.py)：mock 模式（`video-send`／`video-recv`）完整實作，封包格式相容，`--probe` 印每一張的重組過程與 queue 狀態。只用標準函式庫（JPEG 不需要解碼，只是 bytes）。

## 參考文獻

- H. Schulzrinne et al., "RTP: A Transport Protocol for Real-Time Applications," RFC 3550：<https://www.rfc-editor.org/rfc/rfc3550>（frame 切片、marker bit 的概念）。
- L. Berc, W. Fenner, R. Frederick, S. McCanne, P. Stewart, "RTP Payload Format for JPEG-compressed Video," RFC 2435, Oct. 1998：<https://www.rfc-editor.org/rfc/rfc2435>（真正的 MJPEG over RTP 怎麼切片：fragment offset、type、q）。
- ITU-T Rec. T.81（JPEG，1992）：<https://www.itu.int/rec/T-REC-T.81>；G. K. Wallace, "The JPEG Still Picture Compression Standard," *Comm. ACM*, 1991, [doi:10.1145/103085.103089](https://doi.org/10.1145/103085.103089)。
- stb_image／stb_image_write（Sean Barrett，public domain／MIT）：<https://github.com/nothings/stb>。
