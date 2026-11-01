# python_ref/：MiniLINE 的純 Python 完整版（mock 模式）

`miniline.py` 是 Team 3 規格（[`../README.md`](../README.md)）的 **mock 模式**完整實作：`video-send`／`video-recv`、16 bytes 的封包格式、
切片／重組、逾時、bounded display queue、HELLO／BYE／ssrc、STATS。只用標準函式庫（JPEG 不需要解碼，只是 bytes），Python 3.8 以上都能跑。它有兩個用途：

1. **對照用**：每一步都用最直白的高階寫法（`struct.pack`、`dict`、`list`、`select`），旁邊的註解標了「★ 對應 C」——你們的 C 程式就是把這些翻譯成固定大小的陣列、`recvfrom` 逾時、單調時鐘、執行緒。
2. **評測的對手端**：評測會用它的 `video-send` 對你們的 `video-recv`、它的 `video-recv` 對你們的 `video-send` 各跑一次。先在家裡互測過，評測當天就不會有驚喜。

`call`／`devices`（live 模式）、`video-send … camera`（攝影機）都不在 Python 版裡：只印一句話、結束碼 2。`--display window` 會印一句話然後照 `none` 做（只寫檔）。要做的是 C。

## 用法

```
python miniline.py video-send <ip> <port> <jpegdir> [--fps 5] [--quality 75] [--burst] [--loop]
python miniline.py video-recv <port> <outdir> [--bind <ip>] [--queue <n>] [--timeout <ms>] [--idle <ms>] [--display none|window]
```

預設與規格相同：`--fps 5`、`--queue 3`、`--timeout 400`、`--idle 3000`、`--bind 0.0.0.0`、`--display none`。`--quality` 收下但 mock 不用（不重新壓縮）。
任何指令加 `--probe` 會把中間過程印到 stderr。

同一台電腦自測（開兩個視窗；recv 先開）：

```
python miniline.py video-recv 5500 out --probe
python miniline.py video-send 127.0.0.1 5500 seq
```

夾著搗蛋的中繼（三個視窗；recv 聽 5500、中繼聽 5501 轉給 5500、send 送到 5501）：

```
python miniline.py video-recv 5500 out --probe
python ../baseline/udp_impair.py 5501 127.0.0.1 5500 --loss 0.03 --jitter 30 --seed 1
python miniline.py video-send 127.0.0.1 5501 seq
```

（`../baseline/udp_impair.py` 只是轉呼叫 Team 2 的同一支程式，參數一樣。）
測試用的序列：`../baseline/make_sequence`（C，真正的影像）或 `python ../tests/make_fake_sequence.py seq`（純 Python，合法但沒畫面的 JPEG，大小 3–25 KB 各不相同）。

## `--probe` 會印什麼

```
[probe] 第一包到了（type=3 seq=0 ssrc=0xFF72B759）
[probe] HELLO（seq=0）
[probe] frame=0 piece=0/6：已收 1/6  表=[0]
[probe] frame=0 收齊 6 片，0.0 ms
[probe] 顯示時鐘開始：第 i 拍在 +200·i ms
[probe] tick    0  queue=[0]  顯示 frame=0（6098 bytes，第一片到顯示 0 ms）
[probe] seq 從 52 跳到 54：中間 1 包沒收到（pieces_lost 累計 1）              ← --loss：這張之後會 timeout
[probe] frame=4 piece=11/15：已收 14/15  表=[4]
[probe] frame=5 piece=0/9：已收 1/9  表=[4, 5]
[probe] frame=4 丟掉（timeout）：401 ms 還沒收齊，只收到 14/15 片
[probe] tick    5  queue=[]  空的（display_idle=1）
[probe] frame=7 piece=3/12：重複，忽略                                        ← --dup
[probe] HELLO（seq=2）：比已經收到的封包舊（亂序遲到），忽略                  ← --reorder 把開頭的 HELLO 扣到第一片之後
[probe] queue 滿了（3）：丟掉最舊的 frame=12（frames_dropped_queue=1）        ← --burst
[probe] BYE（seq=754 frame_id=60）：queue（[59]）顯示完、表（[]）收齊或逾時後，再等 1 s 看有沒有新的 HELLO
[probe] tick   59  queue=[59]  顯示 frame=59（7534 bytes，第一片到顯示 0 ms）
STATS role=video-recv queue=3 timeout_ms=400 frames_shown=60 frames_complete=60 frames_dropped_timeout=0 frames_dropped_queue=0 pieces_received=751 pieces_lost=0 pieces_bad=0 fps_actual=5.0 avg_assemble_ms=0.0 max_assemble_ms=0.0 display_idle=0
```

每一片一行（這張收到幾片了、重組表裡有誰）、每張收齊或丟掉一行、每一拍一行（queue 裡有誰、顯示了誰或空的）。
看著這個輸出對照規格的「重組、逾時與 bounded queue」那一節，每一條規則都找得到對應的那一行。

## 程式結構（對照 C 的時候照這個順序讀）

| 區塊 | 函式／class | 對應規格 | C 版要換成 |
|---|---|---|---|
| 1 JPEG 序列 | `read_sequence` | mock 的輸入 `NNNN.jpg` | `fopen`／`fread` 整張進 `malloc` 的 buffer；live 改成 `camera_read` + `jpeg_encode` |
| 2 封包 | `pack_header`、`parse_packet`、`extend_seq`、`SeqTracker` | 封包格式；壞封包；16-bit seq 繞回與 `pieces_lost` | 逐 byte 位移填 header（不要 `memcpy` struct）；`uint16_t` 相減再轉 `int16_t`；一個小 bitmap 記最近沒收到的 seq |
| 3 video-send | `cmd_video_send` | 3 次 HELLO、每 200 ms 一張切片逐片送、3 次 BYE、`STATS role=video-send` | `clock.h` 的 `sleep_until`；切片就是 `for (off = 0; off < len; off += 1200)` |
| 4a 重組表 | `Reassembler.put`／`expire` | 重組表（上限 8 張）、重複的片忽略、`--timeout` 與「落後 2 張」的規則 | 固定 8 格的 struct 陣列：`frame_id`、`cnt`、`got[]`、`first_t`、`buf[cnt × 1200]` |
| 4b 顯示 | `Display.push`／`tick` | bounded queue（丟最舊）、顯示時鐘每 200 ms 一拍、`display_idle`、`shown.txt` | n 格的環狀陣列；live 時顯示是另一條執行緒（`display_show`），queue 加 mutex |
| 4c 主迴圈 | `cmd_video_recv` | HELLO／ssrc 重置、BYE、`--idle`、`STATS role=video-recv` | `recvfrom` 加逾時；live 的三條管線各自的 socket 與執行緒 |
| 5 main | `main` | 命令列、結束碼 0／1／2 | `argv` 解析 |

計數器的定義設計成彼此一致：沒有搗蛋時 `frames_shown = frames_complete = 張數`、`frames_dropped_* = pieces_lost = pieces_bad = 0`，輸出的每張與輸入逐 byte 相同；
`--burst` 時 `frames_shown + frames_dropped_queue = frames_complete`。

## 規格沒說、這支程式這樣做的約定

你們的 C 版**可以**採用同樣的約定（和這支程式互測時結果就會一樣），不採用也能過驗收（評測只看規格寫的東西）。

- **HELLO／BYE 的其他欄位**：HELLO 的 `frame_id = 0`；BYE 的 `frame_id` = 這次串流送了幾張；兩者 `piece_idx = piece_cnt = 0`、payload 空。
  接收端對 HELLO／BYE **不**檢查 `piece_idx < piece_cnt`（那條規則只對 PIECE 有意義），payload 多了也忽略。PIECE 的片段不可以是 0 bytes（算壞封包）。
- **`pieces_received` 只算合法的 PIECE**（HELLO／BYE 不算，但重複的片算——它是合法的片）；**`pieces_lost` 看所有 type 共用的 seq**：
  seq 往前跳 k 個就先記 k 個遺失，之後亂序補到了再減回來；結尾 3 次 BYE 全掉了也算在裡面。換 ssrc 時各段的 `pieces_lost` 累加。
- **同一個 ssrc 的 HELLO 也當作重新開始**（對方 `/call` 重連但沒換 ssrc）——但只有 seq 比目前看過的都新的 HELLO 才算；
  開頭 3 次 HELLO 被 `--reorder` 扣到第一片之後才到的，是遲到的舊包，忽略（不然會把剛收的 frame 0 清掉）。
- **重置時計數器不清**：HELLO／換 ssrc 清的是重組表、queue、顯示時鐘、frame_id；STATS 是整次執行的累計。`shown.txt` 也不清，所以可以看到 frame_id 從 0 重算（驗收第 7 項）；`NNNN.jpg` 會被第二段覆蓋。
- **顯示時鐘從第一張收齊的那一刻開始**（第 0 拍馬上顯示它），之後每 200 ms 一拍，不等網路；重置後時鐘重來。
  **`fps_actual = frames_shown ÷（拍數 × 0.2 s）`**，拍數含空的（`display_idle`）那些；收尾（BYE 或 idle）而且 queue 已經空了就停下時鐘，之後不再累計。
- **`shown.txt` 每行 `frame_id,t_ms`**，`t_ms` 是從 recv 啟動算起的毫秒（整數）。每次執行先清空再逐行 append。
- **BYE 之後的結束（mock）**：queue 顯示完、重組表裡還沒收齊的給它 `--timeout` 的機會，然後**再等 1 秒**（`BYE_GRACE_SEC`）看有沒有新的 HELLO／新 ssrc——
  有就重置繼續收（掛斷再連，驗收第 7 項就是這樣測：同一個 recv 連收兩段）；沒有就印 STATS 結束。
- **`--idle` 逾時的收尾**：表裡的都算 `frames_dropped_timeout`，queue 裡的照拍子顯示完才結束（不丟）。第一包來之前的等待不受 `--idle` 限制（最多等 `max(10 × idle, 30 s)`，不永久等）。
- **重組表滿了（8 張）丟最舊的**，算 `frames_dropped_timeout`。已經收齊或丟掉的 frame_id 再來的片忽略；`piece_cnt` 和這張第一片說的不一樣的片也忽略。
- **`--loop`**：送完從頭再送，`frame_id` 繼續往上數（不歸零；接收端的檔名就是 `frame_id`），Ctrl-C 才送 BYE、印 STATS。
- **send 的 `bytes`** = 所有 PIECE 封包的 UDP payload bytes（含 16 bytes header；HELLO／BYE 不算）；**`duration_ms`** 從第一張到最後一張的 200 ms 拍子結束（BYE 在那之後才送），所以 60 張 ≈ 12000 ms、`fps_actual=5.0`。

## 自動化驗收

[`../tests/run_video_tests.py`](../tests/run_video_tests.py) 自己啟動 `udp_impair.py`，跑規格「最低驗收」第 1–7 項；
`send_cmd`／`recv_cmd` 可以各自指定 Python 版或你們的 C 版（子命令可省略，會自動補 `video-send`／`video-recv`），所以四種組合都能自動測：

```
cd tests
python make_fake_sequence.py seq                       # 或用 ../baseline/make_sequence 產生真正的影像
python run_video_tests.py "python ../python_ref/miniline.py" "python ../python_ref/miniline.py" seq
python run_video_tests.py "../build/miniline"           "python ../python_ref/miniline.py" seq
python run_video_tests.py "python ../python_ref/miniline.py" "../build/miniline"           seq
```

## 量測時會看到的一件事：片的遺失率 vs 整張的損失率

`--jitter 30 --seed 1`，60 張（751 片，平均 12.5 片／張），這支程式對這支程式：

| `--loss` | pieces_lost | frames_dropped_timeout | frames_shown | (1−p)^12.5 × 60 |
|---|---|---|---|---|
| 0 | 0 | 0 | 60 | 60 |
| 0.01 | 9 | 8 | 52 | 53 |
| 0.02 | 17 | 14 | 46 | 47 |
| 0.05 | 45 | 30 | 30 | 32 |
| 0.10 | 82 | 41 | 19 | 16 |

掉 1% 的片就掉 13% 的張、掉 5% 的片掉一半——這就是 (1−p)^k，k 是一張的片數。語音一包就是一個 frame（k = 1），所以對掉包鈍得多。
報告的第 1 題就是用你們的數字驗證這個式子（每點跑 3 次、換 seed）；`--quality` 調低 → 每張變小 → k 變小 → 曲線往上。
