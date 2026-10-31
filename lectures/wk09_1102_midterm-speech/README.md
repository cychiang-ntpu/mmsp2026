# 第 9 週（2026/11/2）｜期中考、語音實驗（Team 2 實作工作坊）

對應：期中考、Team 2（11/8 18:00 截止、11/9 評測）
時間：13:10–16:00，三節。

> **課前準備（同學）**：`git pull`；**每組至少兩台筆電、兩副耳機、一支手機熱點**；Team 2 的 repo 裡 baseline 要編得過（上週目標）。
> 用電腦教室公用電腦的同學：一開機先做 [../wk03_0921_team1-kickoff/setup_first.md](../wk03_0921_team1-kickoff/setup_first.md)。
> **跟著打指令請開 [commands.md](commands.md)**。

## 時程

| 節次 | 時間 | 內容 | 教材 |
|---|---|---|---|
| 1 | 13:10–14:00 | **期中考**：50 分鐘閉卷，範圍 MP1–MP5 與 Team 1 | 範例：[midterm_sample.md](midterm_sample.md)／[詳解](midterm_sample_solutions.md) |
| 2 | 14:10–15:00 | **語音實驗（上）**：用耳朵聽掉封包、用 Python 完整版當對手端驗封包格式、跑自動驗收 1–7 | [Team 2 規格](../../team_projects/team2_voicelink/README.md)、[baseline/](../../team_projects/team2_voicelink/baseline/)、[python_ref/](../../team_projects/team2_voicelink/python_ref/)、[tests/](../../team_projects/team2_voicelink/tests/) |
| 3 | 15:10–16:00 | **語音實驗（下）**：buffer 掃描與畫圖、兩台電腦 live 通話與延遲量測、檢核表簽核 | [../wk07_1019_audio-pcm/slides/jitter_buffer.html](../wk07_1019_audio-pcm/slides/jitter_buffer.html)、[../wk07_1019_audio-pcm/examples/jitter_sim.py](../wk07_1019_audio-pcm/examples/jitter_sim.py) |

第二、三節是**工作坊**：老師與助教在教室巡，每組照下面的檢核表做，做到哪一項就舉手讓助教簽；下課前每組回報進度。離截止只剩 6 天，今天的目標是把**驗收 0–7 在教室裡至少過一次**。

---

## 第一節｜期中考

- 13:10 準時發卷，14:00 收卷。閉卷、可帶不能上網的計算機。
- 題型與配分同[範例考題](midterm_sample.md)（選擇 20、計算 40、簡答 25、系統題 15），題數較少。
- 考完請把筆電開好、連上熱點，第二節直接開始。

---

## 第二節｜語音實驗（上）：先聽，再對格式，再跑驗收

### 2.1 聽掉封包（10 分鐘）

每組兩台電腦 A、B 連同一個熱點，先用**官方 baseline 的 raw 版**跑一次正常、一次搗蛋（指令見 commands.md 2.1）。
把 `out.wav` 與 `bad.wav` 用耳機聽：掉包的地方整段變短、亂序倒著播、抖動忽快忽慢。這是你們的程式要修好的三件事。

### 2.2 用 Python 完整版當對手端：封包格式對不對？（20 分鐘）

[python_ref/voicelink.py](../../team_projects/team2_voicelink/python_ref/voicelink.py) 的封包格式與規格完全一致。兩個方向各跑一次：

| 方向 | A（收） | B（送） | 看什麼 |
|---|---|---|---|
| 你們的 send → 參考的 recv | `python3 voicelink.py recv 5004 out.wav --probe` | 你們的 `voicelink send <A的IP> 5004 in.wav` | `--probe` 每一拍印出 buffer 裡的 seq：你們的 seq／ts 有沒有照規格跳（seq +1、ts +320）；結束時 STATS 的 `lost=0 concealed=0` |
| 參考的 send → 你們的 recv | 你們的 `voicelink recv 5004 out.wav` | `python3 voicelink.py send <A的IP> 5004 in.wav` | 你們的輸出 WAV 與 `in.wav` 的 PCM 逐 byte 相同；你們的 STATS 欄位順序與名稱和規格一樣 |

常見的格式錯：header 欄位用 little-endian（WAV 的習慣）而不是 big-endian；`ts` 用 frame 編號而不是 sample 數；最後一包沒補 0 到 640；BYE 沒送或送了但 recv 不認。
`--probe` 看到的現象 → 原因：seq 一直跳 2 → 送端 seq 從 1 開始或加了兩次；每一拍都 conceal → recv 的播放時鐘沒等到 `--buffer` 就開始，或 ts 單位錯。

### 2.3 跑自動驗收 1–7（20 分鐘）

[tests/run_mock_tests.py](../../team_projects/team2_voicelink/tests/run_mock_tests.py) 會自己開 `udp_impair.py`，把驗收 1–7 跑一遍（指令見 commands.md 2.3）。
先用「參考 vs 參考」確認環境沒問題，再換成「你們 vs 你們」，最後「你們 vs 參考」兩個方向。每一項 FAIL 都有原因訊息；評測當天用的就是同一支腳本（換老師的 WAV）。

---

## 第三節｜語音實驗（下）：量測、live、簽核

### 3.1 buffer 掃描（20 分鐘）

固定 `udp_impair.py --loss 0.05 --jitter 30 --reorder 0.05 --seed 1`，`--buffer` 取 20、40、60、100、140、200 各跑 3 次，記 `concealed`、`late`、`avg_delay_ms`（V 負責寫成腳本，原始數據存 CSV）。
先用 [jitter_sim.py](../wk07_1019_audio-pcm/examples/jitter_sim.py) 離線算一次當預測，再和實測比：曲線形狀應該一樣（late 隨 buffer 下降、avg_delay 上升），數字不同是正常的（真實網路的抖動不是均勻分布）。
畫成兩張圖（x = buffer_ms；y₁ = late + concealed、y₂ = avg_delay_ms），報告第 1、2 題就是解釋這兩張圖。

### 3.2 live：兩台電腦通話與延遲（20 分鐘）

戴耳機，兩邊各打 `voicelink call`（commands.md 3.2）。先用預設 `--buffer 60 --plc repeat`，再換 `--plc zero`、再把 buffer 拉到 200 ms，聽差別。
量「講話到對方聽到」的延遲（報告第 4 題）最簡單的做法：兩台電腦放在一起、一邊拍手，另一邊的耳機拿下來靠近，用手機錄音軟體看兩個尖峰差多少 ms；或者 A 的麥克風對著 B 的喇叭、B 的麥克風對著 A 的喇叭，聽回音的間隔。
把延遲拆成：麥克風取樣一個 frame（20 ms）＋ 網路 ＋ jitter buffer（≥ `--buffer`）＋ 喇叭的輸出 buffer；量到的數字應該 ≥ 20 + buffer。

### 3.3 檢核表（簽核用，20 分鐘；做不完的帶回去）

| # | 項目 | 證據 |
|---|---|---|
| 1 | 兩台電腦（不同 IP）baseline raw 互通 | 助教看 `received N packets` |
| 2 | 你們的 send → 參考的 recv：`lost=0 concealed=0` | STATS 截圖進 `TEAM_LOG.md` |
| 3 | 參考的 send → 你們的 recv：PCM 逐 byte 相同 | `run_mock_tests.py` 的測試 1 |
| 4 | `run_mock_tests.py` 你們 vs 你們：1–7 全 PASS | 終端機輸出 |
| 5 | buffer 掃描至少 3 個點、CSV 已 commit | repo |
| 6 | live 通話 30 秒、兩種 PLC 都試過 | 助教現場聽 |
| 7 | `docs/interface.md` 與規格一致（封包格式、STATS） | repo |

### 3.4 收尾（5 分鐘）

- 11/8（日）18:00 登錄 SHA；11/9 評測：mock 用老師的 WAV 跑 `run_mock_tests.py`（你們 vs 參考、兩個方向），live 用教室熱點。
- 投影片（`slides.pdf`）：架構圖、封包格式、buffer 設計決策、兩張掃描圖、live 延遲的量法與數字、報告四題。
- 口試會問：為什麼不用 TCP；seq 與 ts 為什麼分開；遲到的包為什麼要丟；repeat 連續掉包會怎樣；buffer 上限滿了丟哪一個、為什麼。

---

## 範例程式與素材

本週沒有新的範例程式；用的是第 7 週的 [examples/jitter_sim.py](../wk07_1019_audio-pcm/examples/jitter_sim.py)、[slides/jitter_buffer.html](../wk07_1019_audio-pcm/slides/jitter_buffer.html)、[data/](../wk07_1019_audio-pcm/data/) 與 Team 2 的 [baseline/](../../team_projects/team2_voicelink/baseline/)、[python_ref/](../../team_projects/team2_voicelink/python_ref/)、[tests/](../../team_projects/team2_voicelink/tests/)。

## 回家作業／下週前要做的事

1. **Team 2**：檢核表沒簽到的項目補完；buffer 掃描 6 個點 × 3 次；投影片；11/8 18:00 登錄 SHA。
2. 11/9 評測當天每組帶兩台筆電、耳機、熱點；P 準備 8 分鐘報告。

## 參考

- [Team 2 規格](../../team_projects/team2_voicelink/README.md)（含 RTP、G.711 App. I、G.114 的文獻）、[第 7 週講義](../wk07_1019_audio-pcm/README.md)第二節（jitter buffer、PLC 的原理）。
