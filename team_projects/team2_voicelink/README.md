# Team 2｜VoiceLink：即時語音管線（UDP、20 ms framing、jitter buffer、遺失補償）

- 期間：10/19－11/23｜繳交截止：**11/22（日）18:00**｜評測：11/23（三節課內）
- 2026/11/1 公告：11/9、11/16 老師出國改為助教主持的 tutorial 工作坊，截止與評測各順延兩週（原 11/8／11/9）；**同時新增功能 4「語音壓縮（μ-law）」**，見下
- 團隊 repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、
  `docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`
- 繳交：登錄 repo URL＋完整 commit SHA
- 本輪四人組角色：**P＋D＋D＋V**（三人組 P＋D＋V）；分組名單於 10/19 公告
- **11/9 定版**：[tutorial/](tutorial/TUTORIAL.md) 九個模組＋六步做出**能通話的 raw PCM voicelink**（第 6 步 live）。這一輪真正的作業是**加語音壓縮**，在 tutorial 旁邊兩個資料夾裡做：[`mulaw_system/`](mulaw_system/README.md)（μ-law）與 [`adpcm_huffman_system/`](adpcm_huffman_system/README.md)（ADPCM 或／和 Huffman），各給一個框架（第 6 步的程式＋一張 codec 表，`★` 標出要動手的地方）。評分 **40（m 與 step 全部 push）＋30（11/23 demo：第 6 步 live 10、ADPCM 10、Huffman 10）＋30（助教程式碼實質審查）**；**team repo 照這個資料夾的結構繳交**。見「驗收與評分」。

## 從這裡開始（學生進入點，11/8 起）

這一頁就是 Team 2 的全部：規格、評分、時程、tutorial 與工具都從這裡連出去。不用再看別的頁面才知道要做什麼。

### 這一輪要做什麼（30 秒版）

1. 跟著 [tutorial/TUTORIAL.md](tutorial/TUTORIAL.md) 做完九個模組（m1–m9）與六步（step1–step6），**每一個都 add／commit／push 到 team repo**——這就是 40 分。第 6 步做完就是能兩台通話的 voicelink（raw PCM）。
2. 加壓縮：[`mulaw_system/`](mulaw_system/README.md) 加 μ-law（零件 m8）；[`adpcm_huffman_system/`](adpcm_huffman_system/README.md) 再加 ADPCM（零件 m9）或／和 Huffman（Team 1 的 Huffman）。兩個資料夾都給了框架，程式裡搜尋 `★` 就是要動手的地方。
3. 11/22（日）18:00 前登錄 SHA；**11/23 demo 30 分**（第 6 步 live 10、ADPCM 10、Huffman 10）；之後一週助教看 repo，**程式碼實質審查 30 分**。

### team repo 的結構（助教照這個批改）

把整個 `team_projects/team2_voicelink/` 複製成 team repo 的內容，**資料夾名稱不要改**：

```
mmsp2026-team2-gXX/
├── baseline/                 wrapper 與 udp_impair.py（不改）
├── tutorial/                 m1_wav/ … m9_adpcm/、step1_raw/ … step6_live/（每個資料夾自己的 commit）
├── mulaw_system/voicelink.c  μ-law 系統（框架 + 你們加的 μ-law）
├── adpcm_huffman_system/voicelink.c   ADPCM 或／和 Huffman 系統
├── tests/、python_ref/       原樣（驗收腳本與對手端）
├── README.md、docs/interface.md、TEAM_LOG.md、CONTRIBUTIONS.md、AI_USAGE.md、slides.pdf
```

### 四個日期各要做到什麼

| 日期 | 做什麼 | 繳交 | 分 | 講義 |
|---|---|---|---|---|
| **11/9（一）** 不上課：自習週 | 建 team repo（照這個資料夾的結構）；Part A 模組 m1–m9 各「跑一次、改一處」 | 每個模組、每一步各自一個資料夾 push；**m1–m9 全部在 11/11（三）18:00 前** | 繳交 40 的一部分 | [wk10](../../lectures/wk10_1109_team2-progress1/README.md) |
| **11/16（一）** 工作坊 ②（助教主持） | 第 4–6 步（做完＝能通話）；兩組互測；`mulaw_system` 開工 | **step1–6 全部在 11/16（一）16:00 下課前 push**（含 step6_live） | 繳交 40 的其餘部分 | [wk11](../../lectures/wk11_1116_team2-progress2/README.md) |
| **11/22（日）18:00** | `mulaw_system` 過測試 8；`adpcm_huffman_system` 做 ADPCM（測試 9）或／和 Huffman（測試 10）；投影片含報告三題 | 登錄 repo URL＋commit SHA | — | — |
| **11/23（一）** 評測 | D 兩台 live 3 分鐘、助教當場跑測試、P 報告 4 分鐘＋問答 2 分鐘 | 帶兩台筆電、耳機、熱點 | demo 30；之後一週審查 30 | [wk12](../../lectures/wk12_1123_team2-eval/README.md) |

滿分 100；各項怎麼算在「[驗收與評分](#驗收與評分119-定版403030)」。

### 五個最常用的指令（Windows PowerShell；macOS／Linux 把 `mingw32-make` 換成 `make`、`.\x.exe` 換成 `./x`）

```powershell
cd team_projects\team2_voicelink\tutorial\step6_live       # 每個模組／步驟各自一個資料夾，裡面有 README、.c、Makefile
mingw32-make                      # 只編這一步（在上一層 tutorial\ 下 mingw32-make 會編全部）
$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav"   # 34 秒的測試語音（16 kHz 單聲道）

# 自己對自己（兩個視窗）：先 recv 再 send；收端結束會印一行 STATS、寫出 out.wav
.\step6_live.exe recv 5004 out.wav --buffer 60 --plc repeat
.\step6_live.exe send 127.0.0.1 5004 $W

# 自動驗收：測試 1–7（第 6 步）；mulaw_system 做完加 8、adpcm_huffman_system 加 9（Huffman：--with-huffman 跑 10）
python ..\..\tests\run_mock_tests.py ".\step6_live.exe send" ".\step6_live.exe recv" $W --only 1,2,3,4,5,6,7

# 壓縮系統（框架現在就編得過；加了 μ-law 之後 --codec mulaw 才會被接受）
cd ..\..\mulaw_system; mingw32-make; .\voicelink.exe send 127.0.0.1 5004 ..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav --codec mulaw
```

更多指令與參數：「[命令列介面](#命令列介面)」；壞網路模擬：`baseline/udp_impair.py`；對手端：`python_ref/voicelink.py`。

### 這個資料夾裡有什麼

| 位置 | 是什麼 | 你會在什麼時候用 |
|---|---|---|
| [tutorial/](tutorial/TUTORIAL.md) | 手把手教學總覽；m1–m9、step1–step6 每個各自一個資料夾，各有 README | 11/9、11/16 全程 |
| [tutorial/step6_live/](tutorial/step6_live/README.md) | 能兩台通話的完整程式（raw PCM）；tutorial 的終點 | demo 的第一個 10 分 |
| [mulaw_system/](mulaw_system/README.md) | **你們寫**：第 6 步＋codec 表的框架，加 μ-law（`★ 第 8 步`） | 11/16 第三節開工；測試 8 |
| [adpcm_huffman_system/](adpcm_huffman_system/README.md) | **你們寫**：同一個框架，加 ADPCM 或／和 Huffman（`★ 第 9 步`） | 11/22 前；測試 9、10；demo 各 +10 |
| [baseline/wrapper/](baseline/wrapper/) | WAV／UDP／時鐘／音訊四個標頭 | 所有程式都用，一起複製 |
| [baseline/udp_impair.py](baseline/udp_impair.py) | 掉包／亂序／抖動的搗蛋中繼 | 自己測 buffer 與 PLC |
| [python_ref/voicelink.py](python_ref/voicelink.py) | Python 完整版（含 μ-law、ADPCM） | 評測對手端；寫 codec 時對照 |
| [tests/run_mock_tests.py](tests/run_mock_tests.py) | 自動驗收測試 1–10 | 每做完一步就跑 |
| 第 9 週 [audio_coding.md](../../lectures/wk09_1102_midterm-speech/audio_coding.md) | μ-law／A-law／ADPCM 的原理，圖文版 | 寫 codec 前讀 |

下面是完整規格。第一次看先讀「名詞解釋」，再看「固定的部分」與「驗收與評分」。

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
| **STATS**（statistics 的縮寫，統計行） | 程式結束時印的一行統計，欄位固定 | 評測用正規表示式抓，報告的圖從這裡來 |
| **BYE／HELLO**（不是縮寫，就是「再見」／「哈囉」：告訴對方「我結束了」／「我開始了」的控制封包） | 送端結束時送的封包（BYE）；Team 3 才有 HELLO | UDP 沒有連線，所以「結束」要自己告訴對方；掉了就靠逾時 |

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

### 功能 4｜語音壓縮：μ-law（必做）、ADPCM（必做）、Huffman（加分）（這一輪的主題）

- `--codec mulaw`：送端把每個 16-bit sample 用 **G.711 μ-law** 壓成 8 bits（一包 **320 bytes**，位元率 128 kb/s → 一半），收端解回 16-bit 再播。第 9 週 2.2 講過的公式；用查表或分段折線都可以，但**編碼端與解碼端要用同一條曲線**。
- 收到的 WAV 不會逐 byte 相同（有失真），驗收改看 **SQNR**：用老師的語音檔，mock 模式沒有搗蛋時，輸出對原檔的 SQNR ≥ 30 dB（第 9 週 `mulaw_demo.py` 實測約 36–38 dB）。
- PLC 在**解碼之後**的 16-bit 上做（repeat 重複的是解碼後的 frame），所以 PLC 的程式不用改。
- `--codec adpcm`（必做）：4-bit ADPCM（一包 **160 bytes**、64 kb/s，原來的四分之一），第 9 週 2.3 與 [audio_coding.md](../../lectures/wk09_1102_midterm-speech/audio_coding.md) 的做法：只送「和預測值的差」、量化階距 Δ 隨上一個碼字放大或縮小。驗收 SQNR ≥ 20 dB（`python_ref` 在 7 秒母音檔約 21 dB）。**每一包獨立編碼**（預測器與 Δ 在每包開頭歸零），所以掉一包不會拖累下一包——報告要說明為什麼要這樣設計、代價是什麼。
- `--codec huff-mulaw`／`--codec huff-adpcm`（加分）：把 μ-law 的 8-bit 碼字（或 ADPCM 的 4-bit 碼字）再做一次 **Huffman 編碼**（Team 1 做過）。表是**固定的**：用老師給的語音檔（例如第 7 週 `speech_osr_16k.wav`）統計一次、寫死在程式裡（存成 `docs/huffman_table.txt` 一起交），兩端用同一張表，所以不用把表送過去（這張表就是期中考講的「事先約好的 side information」）。每個符號都要有碼（出現次數 0 的當 1），每包獨立編碼。μ-law 的碼字在安靜段很集中，通常能再省 15–30%；ADPCM 的碼字分布比較平，可能只省 5–10%——**報告要量出來並解釋為什麼**。payload 格式見封包格式的 0x06。`python_ref` 不會解 0x06，加分驗收是「你們的 send 對你們的 recv」（測試 10）。

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

四個子命令。`<…>` 一定要給，`[…]` 可省略。

| 子命令 | 指令 | 做什麼 |
|---|---|---|
| **send**（mock） | `voicelink send <ip> <port> <in.wav> [--burst] [--codec <名稱>]` | 讀 WAV，照 20 ms 的節奏一包一包送到 `<ip>:<port>`。`--burst` 不等 20 ms 一口氣全送（用來看 buffer 的用處） |
| **recv**（mock） | `voicelink recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero\|repeat] [--timeout <ms>]` | 在 `<port>` 收封包，排序、補償，照 20 ms 的節奏寫成 `<out.wav>`；結束時印一行 STATS |
| **call**（live） | `voicelink call <my_port> <peer_ip> <peer_port> [--buffer <ms>] [--plc zero\|repeat] [--codec <名稱>]` | 麥克風 → 對方喇叭、對方 → 自己喇叭，雙向通話；按 Enter 結束並印 STATS |
| **devices** | `voicelink devices` | 列出電腦上的麥克風與喇叭 |

例：

```
# 同一台電腦自測：先開 recv，再開 send
voicelink recv 5004 out.wav
voicelink send 127.0.0.1 5004 in.wav

# 兩台電腦通話：A（IP 192.168.1.10）與 B（IP 192.168.1.20）各打一行
voicelink call 5005 192.168.1.20 5005        # 在 A
voicelink call 5005 192.168.1.10 5005        # 在 B
```

| 參數 | 預設 | 意義 | 例子 |
|---|---|---|---|
| `--buffer <ms>` | 60 | jitter buffer 的目標延遲：先存這麼多毫秒的聲音再開始播（必須是 20 的倍數；60 = 先存 3 包） | `recv 5004 out.wav --buffer 200`：網路很抖時先存 10 包再播，比較不會缺，但延遲多 140 ms |
| `--plc zero\|repeat` | repeat | 該播的那包沒到時怎麼補：`zero` 補 320 個 0（靜音），`repeat` 重複上一個播出的 frame | `recv 5004 out.wav --plc zero`：掉包處會聽到「喀」一聲的斷音；用 `repeat` 單掉一包幾乎聽不出 |
| `--timeout <ms>` | 2000 | recv 這麼久沒收到任何封包就當作結束（送端被殺掉時 recv 不能永遠等） | `recv 5004 out.wav --timeout 5000`：等 5 秒才放棄 |
| `--codec pcm\|mulaw\|adpcm\|huff-mulaw\|huff-adpcm` | pcm | send／call 每包的聲音怎麼編：`pcm` 原樣 16-bit（640 bytes）、`mulaw` 壓成 8-bit（320 bytes）、`adpcm` 4-bit（160 bytes）、`huff-*` 再加一層 Huffman（長度不固定）。recv 看封包的 type 自動判斷，不用給 | `send 127.0.0.1 5004 in.wav --codec mulaw`：送出的 bytes 減半，收到的 WAV 有一點失真（SQNR 約 37 dB） |
| `--bind <ip>` | 0.0.0.0 | recv 只在這個網路介面上收（0.0.0.0 = 所有介面） | `recv 5004 out.wav --bind 192.168.1.10`：只收送到這張網卡的 |
| `--burst` | 關 | send 不等 20 ms，一口氣把整個檔案送完 | `send 127.0.0.1 5004 in.wav --burst`：recv 的 buffer 會塞滿、`overflow` 變大——這就是為什麼送端也要照時間送 |

幾個常見組合：

```
voicelink recv 5004 out.wav                               # 全部預設：buffer 60 ms、repeat、等 2 秒
voicelink recv 5004 out.wav --buffer 100 --plc zero       # 抖動大的網路、用靜音補
voicelink send 127.0.0.1 5004 in.wav --codec mulaw        # 送壓縮過的
voicelink call 5005 192.168.1.20 5005 --buffer 40         # 通話時把延遲壓低（網路好才行）
```

結束碼：0 正常；1 網路或裝置錯誤；2 參數或檔案格式錯誤。

### 封包格式（UDP payload）

每個 UDP 封包 = **12 bytes 的 header ＋ 聲音資料**。多 byte 的欄位都是 **big-endian**（高位在前）；聲音資料本身維持 WAV 的 little-endian。

| offset | 長度（bytes） | 欄位 | 值 |
|---|---|---|---|
| 0 | 1 | `magic` | 固定 0x56（ASCII 的 `V`）；不是就當壞封包 |
| 1 | 1 | `type` | 這包是什麼，見下表 |
| 2 | 2 | `seq` | 封包序號：每送一包 +1，從 0 開始，到 65535 之後繞回 0 |
| 4 | 4 | `ts` | timestamp，單位是 sample：第 k 個 frame 的 ts = 320·k |
| 8 | 4 | `ssrc` | 本次串流的隨機識別碼，同一次執行固定；收到不同的 ssrc 就把 buffer 清掉重來 |
| 12 | 依 type | 聲音資料 | 見下表 |

`type` 的五種值：

| type | 名稱 | 聲音資料 | 封包總長 | 說明 |
|---|---|---|---|---|
| 0x01 | AUDIO | 320 個 16-bit sample（little-endian） | 12 + 640 = **652** | 預設；最後一包不足 320 個 sample 時後面補 0 |
| 0x04 | AUDIO_MULAW | 320 個 μ-law byte（每 sample 1 byte） | 12 + 320 = **332** | `--codec mulaw`（必做） |
| 0x05 | AUDIO_ADPCM | 160 bytes（每 sample 4 bits，高 4 bits 先） | 12 + 160 = **172** | `--codec adpcm`（必做） |
| 0x06 | AUDIO_HUFF | 1 byte 底層 codec 的 type（0x04 或 0x05）＋ 2 bytes **有效位元數** n（big-endian）＋ ⌈n/8⌉ bytes 的 Huffman 位元串（MSB 先、最後補 0） | 12 + 3 + ⌈n/8⌉（不固定） | `--codec huff-mulaw`／`huff-adpcm`（加分） |
| 0x02 | BYE | 無 | **12** | 送端結束時送 3 次；`ts` = 整個檔案真正的 sample 總數 |

一個 AUDIO 封包的前 12 bytes 長這樣（seq = 5、ts = 1600、ssrc = 0x1234ABCD）：

```
56 01 | 00 05 | 00 00 06 40 | 12 34 AB CD | <640 bytes PCM…>
 ↑  ↑    seq        ts            ssrc
magic type
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

程式結束時印**一行**到 stderr（評測用正規表示式抓，**欄位順序與名稱不可改**；tutorial 第 6 步已經全部印對，不用改）。

**recv 印的**：

```
STATS role=recv buffer_ms=60 plc=repeat codec=mulaw frames_out=1678 received=1601 lost=77 late=3 dup=0 reordered=12 overflow=0 concealed=80 avg_delay_ms=58.3 max_delay_ms=112.0
```

| 欄位 | 意義 | 上面例子的意思 |
|---|---|---|
| `role` | 這一行是誰印的：`recv` 或 `send` | 收端 |
| `buffer_ms` | 執行時用的 `--buffer` | 先存 60 ms（3 包）再播 |
| `plc` | 執行時用的 `--plc`：`repeat` 或 `zero` | 掉包用重複上一包補 |
| `codec` | 收到的封包是哪種編碼（看 type）：`pcm`／`mulaw`／`adpcm`／`huff-mulaw`／`huff-adpcm`；一個都沒收到時印 `pcm` | 對方用 μ-law 送 |
| **`frames_out`** | 交給喇叭／寫進 WAV 的 frame 總數，含補償的；= 播出去的真封包 + `concealed` | 共播了 1678 個 20 ms（33.6 秒） |
| **`received`** | 收到的合法 AUDIO 封包數（重複、遲到的也算在內；壞封包不算） | 收到 1601 包 |
| **`lost`** | 從 seq 判斷「應該有但一直沒到」的包數（亂序後來到的不算） | 77 包在網路上不見了 |
| `late` | 到了但它的拍子已經過了、被丟掉的包數 | 3 包來不及 |
| `dup` | 同一個 seq 收到第二次、被丟掉的包數 | 沒有重複 |
| `reordered` | seq 比較小卻比較晚到、但還來得及被排回去的包數（不算遺失） | 12 包亂序但救回來了 |
| `overflow` | buffer 滿了、丟掉最舊那包的次數 | 沒發生 |
| **`concealed`** | PLC 補的 frame 數 = 播放時鐘到了但 buffer 裡沒有那個 seq 的次數；正常情況 ≈ `lost` + `late` | 補了 80 個（77 掉 + 3 遲到） |
| `avg_delay_ms` | 封包「到達」到「被播放」的時間差的平均（只算真的播出去的包）；大約等於 `buffer_ms` | 平均在 buffer 裡等 58 ms |
| `max_delay_ms` | 同上的最大值 | 最久的一包等了 112 ms |

數字之間要對得起來（評測會檢查）：`frames_out = (received − late − dup − overflow) + concealed`。

**send 印的**：

```
STATS role=send codec=pcm frames=1678 bytes=1094056 duration_ms=33560 pace=20
```

| 欄位 | 意義 | 上面例子的意思 |
|---|---|---|
| `role` | `send` | 送端 |
| `codec` | 用的 `--codec` | 沒壓縮 |
| `frames` | 送了幾個 20 ms frame（= 幾個 AUDIO 封包） | 1678 包 |
| `bytes` | 送上線的 UDP payload 總和（header + 聲音資料，不含 BYE）；pcm 是 652 × frames，mulaw 約一半 | 1 094 056 bytes |
| `duration_ms` | 從第一包到最後一包花的時間；照 20 ms 送應該 ≈ frames × 20 | 33.56 秒 |
| `pace` | 每包間隔多少 ms；正常 20，`--burst` 時 0 | 照時間送 |

## 遺失補償（PLC）的要求

- 兩種都要做：`zero`（補靜音）與 `repeat`（重複上一個 frame）。
- 報告要**用耳朵和用數字**比較：同樣掉 5%，哪一種比較不刺耳？為什麼 repeat 遇到連續掉包會變成「嗡」聲？
- 進階（加分，不是必做）：重複的 frame 每次衰減（×0.5），或 zero／repeat 之間做 2 ms 的淡入淡出。

## 量測與報告（第 5 題必交，其餘加分）

用 `udp_impair.py` 固定一組參數（例如 loss 5%、jitter ±30 ms、reorder 5%），把 `--buffer` 從 20 掃到 200 ms，
每個點跑 3 次，記 `concealed`、`late`、`avg_delay_ms`，畫成圖。報告回答：

1. buffer 變大，`late` 與 `concealed` 怎麼變？`avg_delay_ms` 怎麼變？兩者的取捨在哪裡？
2. 這組抖動參數下你們建議的 buffer 是多少 ms？理由？
3. `--burst` 送（沒有 20 ms 的間隔）時 buffer 會發生什麼事？`overflow` 怎麼變？這說明了為什麼送端也要照時間送。
4. live 模式從講話到對方聽到大概多久（自己想一個量法，例如拍手、或兩台電腦對著同一支錄音）？延遲由哪些部分組成？
5. **codec 比較表（必交）**：pcm／mulaw／adpcm（有做的加 huff-mulaw、huff-adpcm）各一列，欄位是每包 bytes、位元率 kb/s、SQNR dB、掉包 5% 時聽起來如何（一句話）。用老師的語音檔量。

## 驗收與評分（11/9 定版：40＋30＋30）

評測用的 WAV 由老師當天給（16 kHz、單聲道、30 秒左右）。mock 測試用 `tests/run_mock_tests.py`，對手端是 `python_ref/voicelink.py`（你們的 send 對它的 recv、它的 send 對你們的 recv 各跑一次；測試 10 例外，是你們對你們）。滿分 100；Huffman 加分最多 +5，總分上限 100。

### ① tutorial 繳交（40 分，組）

m1–m9 與 step1–step6 共 **15 項**都在 team repo 裡 → 40 分；**缺一項扣 3 分**（最低 0）。每一項要：

- **各自一個資料夾**（照 `tutorial/` 的結構：`tutorial/m1_wav/`、…、`tutorial/step6_live/`），裡面有 `.c`、`Makefile`，`make` **零警告**。
- **至少一個 commit**，訊息寫清楚是哪一項、誰做的（例：`m7 ringbuffer：改到達順序看 REPEAT 變 4 次（王小明）`）。
- **該項的檢查有過**：m 模組把 README「做完要能回答」的答案寫進 commit 訊息或 `TEAM_LOG.md`；step3–step6 把對應的 `run_mock_tests.py --only …` PASS 輸出貼進 `TEAM_LOG.md`；step6 另貼兩台 `call` 的 STATS 截圖。

截止：**m1–m9 → 11/11（三）18:00**；**step1–step6 → 11/16（一）16:00**（工作坊 ② 下課時）。逾期補交的項目減半。

**自動檢查**：team repo 根目錄放著 [`.github/workflows/team2-check.yml`](.github/workflows/team2-check.yml)（跟著結構複製過去就有），每次 push 都會跑 [`tests/check_submission.py`](tests/check_submission.py)：15 項各自「資料夾在、.c 在、make 零警告、該過的測試過」，Actions 綠勾就是 40 分；summary 那一行寫著缺幾項、扣幾分。自己在本機也能跑：`python tests/check_submission.py --wav <16k wav> --deadline-report`。
11/9、11/16 工作坊當天助教只協助與記錄進度，**不另外計分、不抽問扣分**。

### ② 11/23 demo（30 分，組）

| 項目 | 怎麼看 | 分 |
|---|---|---|
| **做到第 6 步** | 兩台電腦（不是 127.0.0.1）戴耳機 `call` 30 秒聽得懂（單向也算）；助教當場跑測試 1–7（你們 ↔ python_ref 兩個方向）全過 | 10 |
| **ADPCM**（`adpcm_huffman_system`） | 測試 9 PASS（兩個方向）＋現場切 `--codec adpcm` 通話 | 10 |
| **Huffman**（`adpcm_huffman_system`） | 測試 10 PASS（你們對你們）＋現場切 `--codec huff-mulaw`，STATS 的 `bytes` 比 μ-law 少 | 10 |

μ-law（`mulaw_system`）**不單獨計分**：它是 Huffman 的前提（Huffman 疊在 μ-law 碼字上），測試 8 要過，程式碼審查會看。
P 報告 4 分鐘（codec 比較表、三題）＋問答 2 分鐘不另計分，但審查時會對照你們報告裡的數字。

### ③ 程式碼實質審查（30 分；助教在 11/23 後一週內看登錄 SHA 的 repo）

| 看什麼 | 分 |
|---|---|
| **正確性與互通**：`mulaw_system` 過測試 8、`adpcm_huffman_system` 過 9／10；codec 表用對（type、payload、encode／decode 一致）、ADPCM 每包獨立、第 2 個 sample 換 index、nibble 順序、clamp、最後一包補 0；和 python_ref 兩個方向都通 | 10 |
| **程式品質**：零警告、函式切分合理、沒有死碼與複製貼上的殘骸、註解說得出「為什麼」、`docs/interface.md` 與程式一致、README 能照著建置 | 10 |
| **個人貢獻（個人分）**：`git log` 看得出每個人自己寫的部分，和 `CONTRIBUTIONS.md` 一致；**沒有 C commit 的人這項 0 分**；審查期間可能請你當面解釋自己寫的一段 | 10 |

①、②與③的前兩項是組分；③的第三項是個人分。個人總分 ＝ ① ＋ ② ＋ ③。

### 報告（放在 slides.pdf 裡，三題）

1. 你們的 buffer 設多少 ms？用 `jitter_sim.py` 或實測說明為什麼。
2. `repeat` 和 `zero` 在掉包 5% 時聽起來差在哪？為什麼？
3. **codec 比較表**（「量測與報告」第 5 題）：每包 bytes、kb/s、SQNR、掉包時聽感；有做 Huffman 的加一列並解釋省了多少、為什麼 μ-law 上省得比 ADPCM 多。

## 跨機器測試須知

- UDP 沒有「連線」：兩邊各自 bind 自己的 port，互相指定對方。防火牆第一次會問，要允許（Windows 要允許「私人網路」）。
- 校園 Wi-Fi 常常擋住電腦之間的 UDP（client isolation）：請準備**手機熱點**；評測當天教室也是用熱點。
- 同一台電腦自測：`send 127.0.0.1 5004` 加 `recv 5004`。要測搗蛋就把 `udp_impair.py` 夾在中間（它自己聽一個 port、轉給另一個）。
- 量延遲要用**同一個時鐘**：`avg_delay_ms` 量的是「到達 → 播放」（同一台電腦的時鐘，可信）；不要去比兩台電腦的時間。

## 角色分工（本輪四人組 P+D+D+V、三人組 P+D+V）

- **P（口頭報告）**：評測當天報告架構、封包格式、jitter buffer 的設計決策、量測結果與報告四題。
- **D（展示整合）×2**：一位負責**網路與 buffer**（`send`／`recv`／封包、jitter buffer、PLC、`main`）；一位負責**音訊與 live**（裝置、`call`、兩台電腦的現場展示、延遲量測）。三人組由一位 D 兼兩者，live 以能通話為準。
- **V（測試驗證）**：`tests/`、`make test`、用 `udp_impair.py` 的自動化測試、壞封包測試、buffer 掃描的腳本與原始數據。
- 每個人都要有 C 程式的 commit；程式碼審查的個人分（10）就看 `git log` 裡你自己寫的部分，沒有 commit 就是 0。

## 建議時程

| 週 | 日期 | 目標 |
|---|---|---|
| 1–2 | 10/19–11/1 | 建 team repo（`mmsp2026-team2-gXX`）；跑通 baseline 的 raw 串流；**兩台電腦 UDP 互通** |
| 3 | 11/2–11/8 | 11/2 第三節老師帶著做 [tutorial](tutorial/) 第 1–3 步（header、jitter buffer、repeat PLC） |
| 4 | 11/9–11/15 | 11/9 不上課（自習週）：建 team repo、Part A 模組 m1–m9 各自 commit，**11/11（三）18:00 前全部 push**（Actions 自動檢查）；有餘力先做第 1–3 步 |
| 5 | 11/16–11/22 | 11/16 工作坊 ②：第 4–6 步，**16:00 下課前 step1–6 全部 push（繳交 40 分到手）**；`mulaw_system` 開工（m8 → 表加一列 → 測試 8）；回家做 `adpcm_huffman_system`（m9 → ADPCM 測試 9；Huffman 測試 10）、組間互測；投影片、報告三題、進度報告；**11/22 18:00 前登錄 SHA** |

## tutorial/：手把手六步（從這裡開始）＋兩個系統資料夾（你們寫）

[`tutorial/TUTORIAL.md`](tutorial/TUTORIAL.md) 分兩部分。**Part A 拆解**：九支各 50–130 行的小程式，一支只做一件事——`m1_wav`（讀寫 WAV）、`m2_udp_send`／`m2_udp_recv`（UDP 一包就是一包）、`m3_clock`（20 ms 的時鐘）、`m4_mic_to_wav`（錄音）、`m5_play_wav`（播放）、`m6_header`（12 bytes header 打包／解析）、`m7_ringbuffer`（不用網路的 jitter buffer）、`m8_mulaw`（μ-law 編解碼）、`m9_adpcm`（ADPCM 編解碼，`--trace` 逐 sample 看）——先各自跑過、改過。**Part B 組裝**：六步把模組接成整支程式，每一步都是一支能編譯、能跑、能用 `run_mock_tests.py --only …` 驗的完整程式（`step1_raw/` → `step6_live/`），每一行都有註解。**每個模組與每一步各自一個資料夾**，裡面有自己的 `README.md`、`.c`、`Makefile`，可以單獨 `cd` 進去 `make`：

| 步 | 加什麼 | 過哪些驗收 |
|---|---|---|
| 1 | 讀 WAV、每 20 ms 送 640 bytes、收到就寫檔 | — |
| 2 | 12 bytes header（seq、ts、ssrc）、BYE | — |
| 3 | jitter buffer（依 seq 的環形陣列）、播放時鐘、repeat PLC、基本 STATS | 測試 1、2、5 |
| 4 | late／dup／reordered／overflow／延遲、`--plc zero`、完整 STATS | 測試 3、7 |
| 5 | 壞封包不當掉、ssrc 重置、seq 繞回 | 測試 4、6 |
| 6 | live `call`（麥克風→對方喇叭）、`devices`；**tutorial 到此為止** | 現場 live（demo 的第一個 10 分） |

**接下來是作業**，在 tutorial 旁邊的兩個資料夾，各給一個框架（第 6 步的程式＋一張 codec 表，程式裡用 `★` 標出要動手的地方）：

| 資料夾 | 加什麼 | 零件 | 驗收 |
|---|---|---|---|
| [`mulaw_system/`](mulaw_system/README.md) | codec 表加一列 μ-law（type 0x04、320 bytes） | m8 | 測試 8（和 python_ref 互通） |
| [`adpcm_huffman_system/`](adpcm_huffman_system/README.md) | 再加 ADPCM（0x05、160 bytes）或／和 Huffman（0x06，疊在 μ-law 碼字上） | m9、Team 1 的 Huffman | 測試 9、10；demo 各 +10 |

## baseline/：wrapper 與測試工具（不是程式；程式在 tutorial/ 與兩個系統資料夾）

[`baseline/`](baseline/) 只放三樣**所有程式都要用、但你們不用改**的東西：

| 檔案 | 是什麼 |
|---|---|
| `wrapper/` | 四個標頭＋一個 .c：`wav.h`（讀寫 WAV）、`udp.h`（UDP socket＋big-endian 工具；Windows／POSIX 差異抹平）、`clock.h`（單調時鐘、精準的 20 ms sleep）、`audio.h`／`audio.c`（麥克風與喇叭，用 miniaudio）。tutorial 每支程式都 `-I../../baseline/wrapper`，兩個系統資料夾 `-I../baseline/wrapper`；team repo 照結構繳交就自然帶著走 |
| `udp_impair.py` | 搗蛋的中繼：`python3 udp_impair.py <聽的port> <轉給的ip> <port> --loss 0.05 --jitter 30 --reorder 0.1 --dup 0.02`，`tests/run_mock_tests.py` 與評測都用它 |
| `smoke_check.py` | 比對兩個 WAV 的 PCM 是否逐 byte 相同（tutorial 第 1 步用） |

以前這裡還有一支 `voicelink_raw.c`（天真串流），已被 tutorial 第 1 步取代、11/9 移除；第 2 週的 `chat.c` 在 [team1_textlink/baseline/](../team1_textlink/baseline/)。

## python_ref/：純 Python 的完整版（對照用、評測的對手端）

[`python_ref/voicelink.py`](python_ref/voicelink.py)：mock 模式（`send`／`recv`）完整實作，與上述封包格式相容，`--probe` 會把每一拍 buffer 裡有哪些 seq、播了誰、補了誰印出來。
只用標準函式庫；live 模式不在 Python 版裡（要做的是 C）。

## 參考文獻

- H. Schulzrinne, S. Casner, R. Frederick, V. Jacobson, "RTP: A Transport Protocol for Real-Time Applications," RFC 3550, July 2003：<https://www.rfc-editor.org/rfc/rfc3550>（seq／timestamp／SSRC 的概念就是從這裡來的；本專案的 header 是它的簡化版）
- J. Postel, "User Datagram Protocol," RFC 768, Aug. 1980：<https://www.rfc-editor.org/rfc/rfc768>
- ITU-T Rec. G.711 Appendix I, "A high quality low-complexity algorithm for packet loss concealment with G.711," Sept. 1999：<https://www.itu.int/rec/T-REC-G.711>（真正的 PLC 長什麼樣；本專案只要求 zero／repeat）
- ITU-T Rec. G.114, "One-way transmission time," May 2003：<https://www.itu.int/rec/T-REC-G.114>（單向延遲 150 ms 以內通話品質好；報告第 4 題可以引用）
- miniaudio（音訊 wrapper 用的單一檔頭函式庫，public domain／MIT-0）：<https://miniaud.io/>
