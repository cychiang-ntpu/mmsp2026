# 第 12 週（2026/11/23）｜Team 2 評測、Team 3 提早開題

對應：Team 2、Team 3
時間：13:10–16:00，三節。

> 每組帶**兩台筆電、耳機、手機熱點**；P 帶好 `slides.pdf`。評測對手端用 [python_ref/voicelink.py](../../team_projects/team2_voicelink/python_ref/voicelink.py)。

## 時程

| 節次 | 時間 | 內容 |
|---|---|---|
| 1–2 | 13:10–15:00 | **Team 2 評測**：11 組，每組約 9 分鐘（P 報告 4 分鐘 → D 現場展示 3 分鐘 → 問答 2 分鐘）；mock 驗收由助教在旁邊的電腦同步跑 |
| 3 | 15:10–15:40 | **Team 2 補展示與問答**：第一、二節沒排到或 live 失敗的組重試一次；老師隨機抽人解釋自己 commit 的一段程式（程式碼審查的參考） |
| 3 | 15:40–16:00 | **Team 3 開題（規格導讀）**：[規格](../../team_projects/team3_miniline/README.md)、[baseline](../../team_projects/team3_miniline/baseline/)、分組公告 |

## Team 2 評測流程（每組）

1. **登錄 SHA 的版本**：助教事先 clone、`make`、跑 `tests/run_mock_tests.py`（老師的 WAV、兩個方向：你們 send 對參考 recv、參考 send 對你們 recv）。測試 1–7 共 4 分、8 μ-law 3 分、9 ADPCM 3 分（demo 的「mock 測試」10 分）；加分看 10（Huffman，你們對你們）。結果表投影，不佔報告時間。
2. **P 報告（4 分鐘；與問答合計 10 分）**：**codec 比較表**（pcm／mulaw／adpcm／huff 的每包 bytes、kb/s、SQNR、掉包聽感）是主菜；ADPCM 每包歸零的理由；有做 Huffman 的講表怎麼建、省了多少、為什麼 μ-law 上省得多；最後一分鐘講 buffer 設多少、repeat 與 zero 的差別。
3. **D 現場展示（3 分鐘，10 分）**：兩台電腦用熱點 `call` 通話 30 秒；當場切 `--codec adpcm` 再切 `--plc zero` 聽差別；有 Huffman 的切 `huff-mulaw` 給大家看 STATS 的 bytes。
4. **問答（2 分鐘）**：老師問 1–2 題（μ-law 為什麼小聲音也清楚；ADPCM 為什麼用重建值預測、每包歸零的代價；Huffman 表為什麼可以不送；seq 與 ts 為什麼分開；遲到的包為什麼要丟）。
5. **程式碼實質審查（30 分，11/23 後一週內）**：老師與助教看登錄 SHA 的 repo——ADPCM 整合 10、μ-law 與 buffer／PLC 正確性 5、程式品質 5、個人貢獻 10（看 `git log`，沒有 C commit 的人 0 分）。審查期間可能找你當面解釋自己寫的一段。

評分（11/9 定版）：**tutorial 繳交 40**（m1–m9、step1–7 各自一個資料夾 push，每項 2.5）＋**今天 demo 30**（live 10＋mock 測試 10＋報告問答 10；Huffman +5）＋**程式碼實質審查 30**（組 20＋個人貢獻 10）。總分上限 100。

## Team 3 開題（20 分鐘）

今天只講三件事，11/30 再講影像與 BMP 的原理：

1. **和 Team 2 的差別**（規格「和前兩輪的差別」那張表）：一張 JPEG 要切很多片；缺片整張丟、不補；顯示來不及就丟舊的。
2. **固定的部分**：320×240、5 fps、片 ≤ 1 200 bytes、16 bytes header（magic、type、seq、frame_id、piece_idx、piece_cnt、ssrc）、HELLO／BYE、STATS 欄位。
3. **baseline 怎麼用**：`make_sequence` 做測試序列、`miniline_raw` 一張一個封包（看 IP 分段與遺失放大）、`camera-test` 確認攝影機、`python_ref/miniline.py` 當對手端、`tests/run_video_tests.py` 跑驗收 1–7。

第 1 週（11/23–11/29）目標：建 repo、跑通 baseline、分片／重組的 mock 模式過驗收 1 與 3。

## 回家作業／下週前要做的事

- Team 3：建 team repo（`mmsp2026-team3-gXX`），跑 `make smoke`、`make_sequence`、`miniline_raw`；讀 RFC 2435 的 3.1 節（真正的 MJPEG 是怎麼切片的）。
- 11/30：影像表示、BMP、Team 3 問答、補驗時段 2。
