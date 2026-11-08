# Team 2｜VoiceLink：即時語音管線（UDP、20 ms framing、jitter buffer、遺失補償）

- 期間：10/19－11/23｜繳交截止：**11/22（日）18:00**｜評測：11/23（三節課內）
- 2026/11/1 公告：11/9、11/16 老師出國改為助教主持的 tutorial 工作坊，截止與評測各順延兩週（原 11/8／11/9）；**同時新增功能 4「語音壓縮（μ-law）」**，見下
- 團隊 repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、
  `docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`
- 繳交：登錄 repo URL＋完整 commit SHA
- 本輪四人組角色：**P＋D＋D＋V**（三人組 P＋D＋V）；分組名單於 10/19 公告
- **11/1 簡化**：驗收分成基本／進階／加分三層（見「最低驗收」），只有基本層必過；並提供[手把手教學 tutorial/](tutorial/)，照著做七步就能從零做到過基本層與 μ-law。

## 一句話

把 Team 1 的「可靠但會等」換成「不等但會掉」：用 **UDP** 傳 **20 ms 一包**的 16 kHz 語音，
接收端用 **sequence／timestamp** 把包排回原順序、用 **jitter buffer** 吸收到達時間的抖動、
用 **遺失補償（PLC）** 把掉的包補起來，而且**永遠不等**——寧可補一包，也不讓喇叭停下來。

## 名詞解釋（看規格前先讀；原理在第 7 週 2.1–2.4 與第 9 週第三節）

| 名詞 | 意思 | 原理一句話 |
|---|---|---|
| **UDP**（User Datagram Protocol，使用者資料包協定） | 不建立連線、不保證送達、不保證順序的傳輸協定；一次 `sendto` 就是一個封包（datagram），對方一次 `recvfrom` 收到一整包 | 和 TCP 的差別是沒有重傳與排隊，所以不會「等」；即時語音寧可掉一包也不要全部延後 |
| **datagram／封包** | UDP 一次送出的一塊資料，含我們自己定義的 header 與 payload | 本專案一包 = 12 bytes header + 一個 20 ms 的 frame |
| **frame（音框）** | 固定長度的一小段聲音：20 ms = 320 samples = 640 bytes（16 kHz、16-bit） | 語音在 20 ms 內近似穩定，掉一包損失小；也是所有語音 codec 的自然單位 |
| **framing** | 把連續的 sample 切成一個一個 frame 再包成封包 | 送端照 20 ms 的節奏一包一包送，不是一口氣全送 |
| **seq**（sequence number） | 每送一包 +1 的編號（16-bit，到 65535 繞回 0） | 收端靠它知道哪一包**掉了、亂序、重複** |
| **ts**（timestamp） | 這包的第一個 sample 是整段聲音的第幾個 sample（每包 +320） | 收端靠它知道這包**該在什麼時候播**；和 seq 分開存，因為真實系統裡一包未必剛好一個 frame |
| **ssrc**（synchronization source identifier，同步來源識別碼） | 本次串流的隨機識別碼，同一次執行固定 | 對方重啟後 ssrc 不同，收端就知道要把 buffer 清掉重來 |
| **RTP（Real-time Transport Protocol，即時傳輸協定）** | 真實世界即時媒體用的封包格式（RFC 3550）；本專案的 header 是它的精簡版 | seq／ts／ssrc 三個欄位的概念都來自 RTP |
| **big-endian** | 多 byte 的整數高位在前（網路慣例）；WAV 檔內的 sample 是 little-endian | header 用 big-endian，PCM 維持 WAV 的原樣 |
| **jitter（抖動）** | 封包到達時間的不規則：送端每 20 ms 一包，收端收到的間隔忽長忽短 | 網路排隊、Wi-Fi 重傳造成；用「到達時間 − 預期時間」的變化量來量 |
| **jitter buffer** | 收端先存幾個 frame 再開始播的緩衝區，依 seq 排序 | 用「多等一點延遲」換「不卡」：抖動小於 buffer 的包都來得及 |
| **播放時鐘** | 收端自己每 20 ms 一拍、向 buffer 要一個 frame 的計時器 | 它不等網路；有就播、沒有就補；拍子一定前進 |
| **late（遲到）** | 包到了，但它的拍子已經過了 | 丟掉；buffer 越大 late 越少 |
| **dup**（duplicate，重複） | 同一個 seq 收到兩次（網路或中繼複製） | 丟掉第二份 |
| **reordered（亂序）** | seq 比較小的包比較晚到，但還來得及 | 排回正確位置，不算遺失 |
| **lost（遺失）** | 從 seq 看應該有、但直到該播的時候都沒到的包 | 由 PLC 補；統計上 `concealed ≈ lost + late` |
| **overflow** | buffer 滿了 | 丟最舊的；bounded buffer 才不會被 `--burst` 吃光記憶體 |
| **underrun（欠載）** | 播放時鐘到了但沒東西可播 | 真實系統會卡一下（喀聲）；本專案用 PLC 補，所以計 concealed 而不是停 |
| **PLC**（packet loss concealment，封包遺失補償） | 該播的 frame 沒有時「補一個」的方法 | `zero` 補靜音（會有 click）；`repeat` 重複上一個播出的 frame（連續掉包會變嗡聲）；真 codec 重複上一個 pitch 週期並衰減 |
| **concealed** | PLC 補了幾個 frame | 評測看它是否 ≈ lost |
| **latency／延遲** | 從聲音進麥克風到對方喇叭播出的時間 | = 取樣一個 frame（20 ms）＋網路＋jitter buffer（≥ `--buffer`）＋喇叭輸出 buffer；G.114 說 150 ms 以內通話品質好 |
| **mock 模式／live 模式** | mock：WAV → 封包 → WAV，可重現、評測用；live：麥克風 → 對方喇叭 | 同一套 buffer 與 PLC，只是輸入輸出不同 |
| **udp_impair.py（搗蛋中繼）** | 夾在送收之間、故意製造遺失／抖動／亂序／重複的程式 | `--seed` 讓每次一樣，評測才能重現 |
| **bitrate（位元率）** | 每秒送幾個 bit：16 kHz × 16 bits = 256 kb/s | 壓縮就是為了降低它 |
| **codec**（coder–decoder，編解碼器） | 編碼器＋解碼器：把 PCM 變小（編）、再變回 PCM（解） | 功能 4 的 μ-law、ADPCM；pcm 代表不壓縮 |
| **μ-law** | G.711 的對數量化：16-bit → 8-bit，小聲時階距細、大聲時粗 | 讓 SQNR 幾乎不隨音量變；一個查表就能做 |
| **ADPCM**（adaptive differential pulse code modulation，自適應差分脈衝編碼調變） | 只送「和預測值的差」並把量化階距隨訊號自動調整（4 bits／sample） | 語音相鄰 sample 很像，殘差小很多（prediction gain） |
| **SQNR**（signal-to-quantization-noise ratio，訊號對量化雜訊比） | 訊號功率 ÷ 量化（或編解碼）誤差功率，dB | 評測 μ-law 用它而不是逐 byte 相同 |
| **STATS** | 程式結束時印的一行統計，欄位固定 | 評測用正規表示式抓，報告的圖從這裡來 |
| **BYE／HELLO** | 送端結束時送的封包（BYE）；Team 3 才有 HELLO | UDP 沒有連線，所以「結束」要自己告訴對方；掉了就靠逾時 |

## 為什麼不用 TCP

Team 1 用 TCP：掉一個封包，TCP 會重傳，後面的資料全部排隊等它。文字檔可以等，**即時語音不能等**：
遲到 200 ms 的聲音已經沒有意義了。所以語音通話（Zoom、LINE、Discord）都走 UDP／RTP：
應用程式自己決定「掉了就補、遲到就丟」。這一輪你們要自己做的，就是這個「自己決定」的部分。

## 三個功能

### 功能 1｜mock 模式：WAV → 封包 → WAV（評測用，不需要麥克風）

- `send` 讀一個 16 kHz、16-bit、單聲道的 WAV，每 **20 ms（320 samples＝640 bytes）**包成一個 UDP 封包，
  **照真實時間送**（每 20 ms 一包，不是一口氣全送）。
- `recv` 收封包、經過 jitter buffer 與 PLC，照 20 ms 的節奏「播放」（mock 模式是寫進 WAV），結束後印 STATS。
- 網路正常時，收到的 WAV 的 PCM 資料要和原檔**逐 byte 相同**（長度也相同）。
- 中間可以插一支「搗蛋的中繼」（[`baseline/udp_impair.py`](baseline/udp_impair.py)）製造遺失、亂序、抖動、重複；
  評測就是這樣跑的。

### 功能 2｜live 模式：兩台電腦雙向通話

- `call`：麥克風 → 封包 → 對方喇叭，同時對方 → 自己喇叭。用耳機（不要求回音消除）。
- 同一套 jitter buffer 與 PLC；結束時印 STATS。

### 功能 3｜量測：buffer 延遲與遺失補償的代價

- 每次執行都要印 STATS（格式固定，見下）。報告要分析：**buffer 大小 vs 補償次數 vs 延遲**（見「量測與報告」）。

### 功能 4｜語音壓縮：μ-law（必做）、ADPCM（加分）（11/1 新增）

- `--codec mulaw`：送端把每個 16-bit sample 用 **G.711 μ-law** 壓成 8 bits（一包 **320 bytes**，位元率 128 kb/s → 一半），收端解回 16-bit 再播。第 9 週 2.2 講過的公式；用查表或分段折線都可以，但**編碼端與解碼端要用同一條曲線**。
- 收到的 WAV 不會逐 byte 相同（有失真），驗收改看 **SQNR**：用老師的語音檔，mock 模式沒有搗蛋時，輸出對原檔的 SQNR ≥ 30 dB（第 9 週 `mulaw_demo.py` 實測約 36–38 dB）。
- PLC 在**解碼之後**的 16-bit 上做（repeat 重複的是解碼後的 frame），所以 PLC 的程式不用改。
- 加分：`--codec adpcm`：4-bit DPCM／ADPCM（一包 160 bytes、64 kb/s），第 9 週 2.3 的做法；SQNR ≥ 20 dB 即可。報告要比較三種 codec 的位元率、SQNR、以及掉包時 PLC 的效果（ADPCM 掉一包後要怎麼重新同步預測器？）。

## 固定的部分（各組一律相同，評測會照這個跑）

### 音訊格式

| 項目 | 值 |
|---|---|
| 取樣率 | 16 000 Hz |
| sample | 16-bit signed、little-endian（WAV 的原樣）|
| 聲道 | 1 |
| frame | **20 ms＝320 samples＝640 bytes** |
| 輸入 WAV | 只接受上述格式；不是的話印錯誤、結束碼 2。WAV 要照 chunk 走（`data` 不一定在第 44 byte，第 3 週 2.6） |

### 命令列介面

```
voicelink send <ip> <port> <in.wav> [--burst] [--codec pcm|mulaw|adpcm]
    mock：讀 WAV 照 20 ms 節奏送（--burst 一口氣全送，用來看 buffer 的用處）
voicelink recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]
    mock：收、排序、補償，寫成 WAV，結束印 STATS
voicelink call <my_port> <peer_ip> <peer_port> [--buffer <ms>] [--plc zero|repeat] [--codec pcm|mulaw|adpcm]
    live：雙向通話，按 Enter 結束，結束印 STATS
voicelink devices
    列出麥克風與喇叭
```

| 參數 | 預設 | 意義 |
|---|---|---|
| `--buffer <ms>` | 60 | jitter buffer 的目標延遲（必須是 20 的倍數；60＝先存 3 個 frame 再開始播）|
| `--plc zero\|repeat` | repeat | 遺失補償：補 320 個 0，或重複上一個收到的 frame |
| `--timeout <ms>` | 2000 | recv：這麼久沒有任何封包就當作結束（**不可以永久等待**）|
| `--codec pcm\|mulaw\|adpcm` | pcm | send／call：每包的 PCM 怎麼編（pcm = 16-bit 原樣 640 bytes；mulaw = 320 bytes；adpcm = 160 bytes）；recv 由封包的 `type` 自動判斷，不用參數 |
| `--bind <ip>` | 0.0.0.0 | recv 綁哪個介面 |

結束碼：0 正常；1 網路或裝置錯誤；2 參數或檔案格式錯誤。

### 封包格式（UDP payload）

每個 UDP 封包＝**12 bytes 的 header＋640 bytes 的 PCM**（最後一包不足 640 時後面補 0 到 640）。
所有多 byte 欄位 **big-endian**（PCM 本身維持 WAV 的 little-endian）。

```
offset  長度  欄位
 0      1     magic    = 0x56（'V'）
 1      1     type     = 0x01 AUDIO（16-bit PCM，640 bytes）、0x02 BYE（結束，沒有 PCM）、
                         0x04 AUDIO_MULAW（320 bytes，每 sample 1 byte）、0x05 AUDIO_ADPCM（160 bytes，每 sample 4 bits，加分）
 2      2     seq      每送一包 +1，從 0 開始；2^16 後繞回 0
 4      4     ts       timestamp，單位 sample：第 k 個 frame 是 320·k
 8      4     ssrc     本次串流的隨機識別碼；同一次執行固定。收到不同 ssrc 就重置 buffer
12      640   PCM      320 個 16-bit little-endian sample（type 0x01）；type 0x04 是 320 bytes 的 μ-law；0x05 是 160 bytes（高 4 bits 先）
```

- `seq` 用來偵測**遺失、亂序、重複**；`ts` 用來決定**什麼時候播**。兩者分開存，因為真實系統裡一包未必剛好一個 frame（例如靜音壓縮）。
- 最後一包之後送 3 次 BYE（UDP 可能掉），接收端收到 BYE 或逾時都要能正常結束。
- **BYE 的 `ts` = 整個檔案真正的 sample 總數**（最後一包補的 0 不算）：接收端用它把輸出截到正確長度，所以沒有搗蛋時輸出能和輸入逐 byte 相同；BYE 全掉了就會多幾個 0（可接受）。BYE 的 `seq` 接在最後一個 AUDIO 之後繼續 +1。
- **逾時結束時**（沒收到 BYE）：最後一個真正收到的 frame 之後，播放時鐘在等待期間補的那些 frame 不算、不寫進 WAV（否則 `--timeout 2000` 會憑空多出 100 個靜音 frame）。
- header 不合法（magic 不對、長度和 type 不符：0x01 要 652、0x04 要 332、0x05 要 172、BYE 要 12）的封包直接丟掉並計數，**不可以當掉**。
- 同一次串流的 codec 不會換；收端以收到的第一個合法 AUDIO 封包的 type 為準，之後 type 不同的封包當壞封包。

### jitter buffer 與播放時鐘

- 接收端有一個**固定節奏的播放時鐘**：每 20 ms 要交出一個 frame 給喇叭（mock 模式：寫進 WAV）。這個時鐘不等網路。
- 開始播放前先累積 `--buffer` ms 的資料（預設 3 個 frame）。
- 每一拍：buffer 裡有「下一個該播的 seq」→ 播它；沒有 → **PLC 補一個 frame**（計 `concealed`），seq 照樣前進。
- 遲到（它的拍子已經過了）的封包丟掉（計 `late`）；重複的丟掉（計 `dup`）；亂序但還來得及的要排回正確位置（計 `reordered`）。
- buffer 要有上限（建議 `--buffer` 的 4 倍），滿了就丟最舊的（計 `overflow`）。

### 量測輸出（STATS）

程式結束時印**一行**到 stderr（評測用正規表示式抓，欄位順序與名稱不可改）：

```
STATS role=recv buffer_ms=60 plc=repeat codec=mulaw frames_out=1678 received=1601 lost=77 late=3 dup=0 reordered=12 overflow=0 concealed=80 avg_delay_ms=58.3 max_delay_ms=112.0
```

| 欄位 | 意義 |
|---|---|
| `codec` | pcm／mulaw／adpcm（收端依封包 type；一個都沒收到時印 pcm）|
| `frames_out` | 交給喇叭／寫進 WAV 的 frame 數（含補償的）|
| `received` | 收到的合法 AUDIO 封包數（含重複、遲到）|
| `lost` | 從 seq 判斷沒收到的包數（亂序後仍沒到的才算）|
| `late`、`dup`、`reordered`、`overflow` | 如上節 |
| `concealed` | PLC 補的 frame 數（= 播放時 buffer 沒有那個 seq 的次數）|
| `avg_delay_ms`、`max_delay_ms` | 封包「到達」到「被播放」的時間差的平均與最大值（只算真的播出去的包）|

`send` 也印一行：`STATS role=send codec=pcm frames=1678 bytes=1094056 duration_ms=33560 pace=20`（`bytes` 是送上線的 UDP payload 總和，mulaw 時約一半）。

## 遺失補償（PLC）的要求

- 兩種都要做：`zero`（補靜音）與 `repeat`（重複上一個 frame）。
- 報告要**用耳朵和用數字**比較：同樣掉 5%，哪一種比較不刺耳？為什麼 repeat 遇到連續掉包會變成「嗡」聲？
- 進階（加分，不是必做）：重複的 frame 每次衰減（×0.5），或 zero／repeat 之間做 2 ms 的淡入淡出。

## 量測與報告：buffer 要多大？（進階／加分；基本層只要回答上面兩題）

用 `udp_impair.py` 固定一組參數（例如 loss 5%、jitter ±30 ms、reorder 5%），把 `--buffer` 從 20 掃到 200 ms，
每個點跑 3 次，記 `concealed`、`late`、`avg_delay_ms`，畫成圖。報告回答：

1. buffer 變大，`late` 與 `concealed` 怎麼變？`avg_delay_ms` 怎麼變？兩者的取捨在哪裡？
2. 這組抖動參數下你們建議的 buffer 是多少 ms？理由？
3. `--burst` 送（沒有 20 ms 的間隔）時 buffer 會發生什麼事？`overflow` 怎麼變？這說明了為什麼送端也要照時間送。
4. live 模式從講話到對方聽到大概多久（自己想一個量法，例如拍手、或兩台電腦對著同一支錄音）？延遲由哪些部分組成？
5. pcm 與 mulaw（有做 adpcm 的加上 adpcm）：位元率、SQNR、掉包 5% 時聽起來的差別；用耳朵和數字各說一次。

## 最低驗收（11/1 簡化：三層）

評測用的 WAV 由老師當天給（16 kHz、單聲道、30 秒左右）。評測會用 `python_ref/voicelink.py` 當對手端各跑一次（你們的 send 對它的 recv、它的 send 對你們的 recv）；`tests/run_mock_tests.py --only 1,2,5` 就是基本層。

### 基本層（必過；團隊分的 60%）

做到這一層的程式其實只有三件事：**一包 20 ms 加 header → 收端用一個依 seq 排序的 buffer 先存 3 包再播 → 沒有就重複上一包**。[tutorial/](tutorial/) 的第 1–5 步就是這一層。

| # | 項目 | 怎麼測 |
|---|---|---|
| B0 | 兩台電腦（不是 127.0.0.1）能互通 | 現場 |
| B1 | `send → recv`，沒有搗蛋：輸出 WAV 的 PCM 與輸入**逐 byte 相同**、長度相同 | 測試 1 |
| B2 | 經過 `udp_impair.py --loss 0.05`：輸出的 frame 數與輸入相同（掉的用 repeat 補），`concealed ≈ lost`（±2），程式正常結束 | 測試 2 |
| B3 | 送端中途被殺掉：recv 在 `--timeout` 內自己結束、WAV 完整、STATS 有印 | 測試 5 |
| B4 | STATS 一行裡 `frames_out`、`received`、`lost`、`concealed` 四個數正確且 `frames_out = 播出的真封包 + concealed`；其他欄位可以印 0 | 測試 7 只看這四個 |

### 進階層（每項獨立計分；30%）

| # | 項目 | 怎麼測 |
|---|---|---|
| A1 | 亂序的包排回正確位置：`--reorder 0.1 --jitter 20`、`--buffer 100` 時輸出逐 byte 相同 | 測試 3 |
| A2 | `--plc zero` 也能用，報告用耳朵和數字比較兩種 | 現場 |
| A3 | **μ-law**（功能 4）：`--codec mulaw` SQNR ≥ 30 dB、`bytes` 減半 | 測試 8 |
| A4 | live：兩台電腦戴耳機通話 30 秒聽得懂（**單向**也算） | 現場 |

### 加分層（10%）

重複封包丟掉（測試 4）、壞封包不當掉（測試 6）、`late`／`dup`／`reordered`／`overflow`／`avg_delay_ms` 正確、buffer 掃描圖、ADPCM、PLC 衰減或淡入淡出、`--burst` 不爆記憶體。

### 報告（兩題就好）

1. 你們的 buffer 設多少 ms？用 `jitter_sim.py` 或實測說明為什麼。
2. `repeat` 和 `zero` 在掉包 5% 時聽起來差在哪？為什麼？（想多寫的再加「量測與報告」那一節的題目。）

## 跨機器測試須知

- UDP 沒有「連線」：兩邊各自 bind 自己的 port，互相指定對方。防火牆第一次會問，要允許（Windows 要允許「私人網路」）。
- 校園 Wi-Fi 常常擋住電腦之間的 UDP（client isolation）：請準備**手機熱點**；評測當天教室也是用熱點。
- 同一台電腦自測：`send 127.0.0.1 5004` 加 `recv 5004`。要測搗蛋就把 `udp_impair.py` 夾在中間（它自己聽一個 port、轉給另一個）。
- 量延遲要用**同一個時鐘**：`avg_delay_ms` 量的是「到達 → 播放」（同一台電腦的時鐘，可信）；不要去比兩台電腦的時間。

## 角色分工（本輪四人組 P+D+D+V、三人組 P+D+V）

- **P（口頭報告）**：評測當天報告架構、封包格式、jitter buffer 的設計決策、量測結果與報告四題。
- **D（展示整合）×2**：一位負責**網路與 buffer**（`send`／`recv`／封包、jitter buffer、PLC、`main`）；一位負責**音訊與 live**（裝置、`call`、兩台電腦的現場展示、延遲量測）。三人組由一位 D 兼兩者，live 以能通話為準。
- **V（測試驗證）**：`tests/`、`make test`、用 `udp_impair.py` 的自動化測試、壞封包測試、buffer 掃描的腳本與原始數據。
- 每個人都要有 C 程式的 commit；個人口試會問你自己寫的部分，也會問隊友寫的部分。

## 建議時程

| 週 | 日期 | 目標 |
|---|---|---|
| 1–2 | 10/19–11/1 | 建 team repo（`mmsp2026-team2-gXX`）；跑通 baseline 的 raw 串流；**兩台電腦 UDP 互通** |
| 3 | 11/2–11/8 | 11/2 第三節老師帶著做 [tutorial](tutorial/) 第 1–3 步（header、jitter buffer、repeat PLC）；回家做完第 3 步 → **基本層 B1–B3 過了** |
| 4 | 11/9–11/15 | 11/9 工作坊 ①：Part A 模組＋第 1–3 步，每步 push → 基本層；回家做完第 3 步（11/15 前 push） |
| 5 | 11/16–11/22 | 11/16 工作坊 ②：第 4–7 步（完整 STATS、壞封包、μ-law、live）、組間互測、live 預演；投影片、報告兩題、進度報告；**11/22 18:00 前登錄 SHA** |

## tutorial/：手把手七步（11/1 新增；從這裡開始）

[`tutorial/TUTORIAL.md`](tutorial/TUTORIAL.md) 分兩部分。**Part A 拆解**：八支各 50–80 行的小程式，一支只做一件事——`m1_wav`（讀寫 WAV）、`m2_udp_send`／`m2_udp_recv`（UDP 一包就是一包）、`m3_clock`（20 ms 的時鐘）、`m4_mic_to_wav`（錄音）、`m5_play_wav`（播放）、`m6_header`（12 bytes header 打包／解析）、`m7_ringbuffer`（不用網路的 jitter buffer）、`m8_mulaw`（μ-law 編解碼）——先各自跑過、改過。**Part B 組裝**：七步把模組接成整支程式，每一步都是一支能編譯、能跑、能用 `run_mock_tests.py --only …` 驗的完整程式（`step1_raw.c` → `step7_live.c`），每一行都有註解：

| 步 | 加什麼 | 過哪些驗收 |
|---|---|---|
| 1 | 讀 WAV、每 20 ms 送 640 bytes、收到就寫檔 | — |
| 2 | 12 bytes header（seq、ts、ssrc）、BYE | — |
| 3 | jitter buffer（依 seq 的環形陣列）、播放時鐘、repeat PLC、基本 STATS | B1、B2、B3、B4 |
| 4 | late／dup／reordered／overflow／延遲、`--plc zero`、完整 STATS | A1、A2 |
| 5 | 壞封包不當掉、ssrc 重置、seq 繞回 | 加分 |
| 6 | `--codec mulaw` | A3 |
| 7 | live `call`（麥克風→對方喇叭） | A4 |

你們可以直接以第 7 步的程式為起點改成自己的（要能說明每一行在做什麼——口試會問），也可以自己從頭寫。

## baseline/：官方文字 baseline＋音訊／UDP wrapper

[`baseline/`](baseline/) 有三樣東西，都可以直接編譯執行：

| 檔案 | 是什麼 |
|---|---|
| `chat.c` | 第 2 週的文字聊天（TCP／UDP 都有），Team 3 整合時要用；這一輪不用改 |
| `voicelink_raw.c`＋`wrapper/` | **音訊／UDP wrapper**：`wav.h`（讀寫 WAV）、`udp.h`（開 socket、`sendto`／`recvfrom`＋逾時）、`audio.h`（麥克風與喇叭，用 miniaudio）、`clock.h`（單調時鐘與 20 ms 的 sleep）。`voicelink_raw` 用它們做**最天真的串流**：640 bytes 一包、沒有 header、收到什麼就寫什麼。經過 `udp_impair.py` 之後聽聽看壞在哪裡——這就是你們要解決的問題 |
| `udp_impair.py` | 搗蛋的中繼：`python3 udp_impair.py <聽的port> <轉給的ip> <port> --loss 0.05 --jitter 30 --reorder 0.1 --dup 0.02`，評測用同一支 |

`voicelink_raw.c` 的 `send`／`recv`／`call` 骨架、命令列解析、WAV 與裝置處理都是完成的；你們要做的是把 header、seq／ts、jitter buffer、PLC、STATS 加進去。可以在它上面改，也可以只拿 wrapper 重寫。

## python_ref/：純 Python 的完整版（對照用、評測的對手端）

[`python_ref/voicelink.py`](python_ref/voicelink.py)：mock 模式（`send`／`recv`）完整實作，與上述封包格式相容，`--probe` 會把每一拍 buffer 裡有哪些 seq、播了誰、補了誰印出來。
只用標準函式庫；live 模式不在 Python 版裡（要做的是 C）。

## 參考文獻

- H. Schulzrinne, S. Casner, R. Frederick, V. Jacobson, "RTP: A Transport Protocol for Real-Time Applications," RFC 3550, July 2003：<https://www.rfc-editor.org/rfc/rfc3550>（seq／timestamp／SSRC 的概念就是從這裡來的；本專案的 header 是它的簡化版）
- J. Postel, "User Datagram Protocol," RFC 768, Aug. 1980：<https://www.rfc-editor.org/rfc/rfc768>
- ITU-T Rec. G.711 Appendix I, "A high quality low-complexity algorithm for packet loss concealment with G.711," Sept. 1999：<https://www.itu.int/rec/T-REC-G.711>（真正的 PLC 長什麼樣；本專案只要求 zero／repeat）
- ITU-T Rec. G.114, "One-way transmission time," May 2003：<https://www.itu.int/rec/T-REC-G.114>（單向延遲 150 ms 以內通話品質好；報告第 4 題可以引用）
- miniaudio（音訊 wrapper 用的單一檔頭函式庫，public domain／MIT-0）：<https://miniaud.io/>
