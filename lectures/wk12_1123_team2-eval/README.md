# 第 12 週（2026/11/23）｜Team 2 評測、Team 3 提早開題

對應：Team 2、Team 3
時間：13:10–16:00，三節。

> 每組帶**兩台筆電、耳機、手機熱點**；P 帶好 `slides.pdf`。評測對手端用 [python_ref/voicelink.py](../../team_projects/team2_voicelink/python_ref/voicelink.py)。

## 時程

| 節次 | 時間 | 內容 |
|---|---|---|
| 1–2 | 13:10–15:00 | **Team 2 評測**：11 組，每組約 9 分鐘（P 報告 4 分鐘 → D 現場展示 3 分鐘 → 問答 2 分鐘）；mock 驗收由助教在旁邊的電腦同步跑 |
| 3 | 15:10–15:40 | **Team 2 個人口試**（分散到各組桌邊，老師與助教分頭問） |
| 3 | 15:40–16:00 | **Team 3 開題（規格導讀）**：[規格](../../team_projects/team3_miniline/README.md)、[baseline](../../team_projects/team3_miniline/baseline/)、分組公告 |

## Team 2 評測流程（每組）

1. **登錄 SHA 的版本**：助教事先 clone、`make`、跑 `tests/run_mock_tests.py`（老師的 WAV、兩個方向：你們 send 對參考 recv、參考 send 對你們 recv）。起點層看測試 1–7（S1–S6），壓縮層看 8（μ-law）、9（ADPCM），加分看 10（Huffman，你們對你們）。結果表投影，不佔報告時間。
2. **P 報告（4 分鐘）**：**codec 比較表**（pcm／mulaw／adpcm／huff 的每包 bytes、kb/s、SQNR、掉包聽感）是主菜；ADPCM 每包歸零的理由；有做 Huffman 的講表怎麼建、省了多少、為什麼 μ-law 上省得多；最後一分鐘講 buffer 設多少、repeat 與 zero 的差別。
3. **D 現場展示（3 分鐘）**：兩台電腦用熱點 `call` 通話 30 秒（S0）；當場切 `--codec adpcm` 再切 `--plc zero` 聽差別；有 Huffman 的切 `huff-mulaw` 給大家看 STATS 的 bytes。
4. **問答（2 分鐘）**：老師問 1–2 題（μ-law 為什麼小聲音也清楚；ADPCM 為什麼用重建值預測、每包歸零的代價；Huffman 表為什麼可以不送；seq 與 ts 為什麼分開；遲到的包為什麼要丟）。
5. **個人口試（第三節，20 分）**：每人 2–3 分鐘，指著自己 commit 的 codec 程式講（10）＋一題概念（10）。未達 10/20 可於 11/30 補驗時段 2 補驗一次。

評分（11/8 改版，滿分 100 含加分）：過程 30（11/9 的 m 模組 8＋步驟 6；11/16 的步驟 8＋交叉測試 4＋改一處 4）＋起點層 40（S0 現場 10＋測試 1–7 共 30）＋壓縮層 30（μ-law 15＋ADPCM 15）＋加分 10（Huffman）＋口試 20（個人）。過程、起點、壓縮、加分四項是組分；口試是個人分。

## Team 3 開題（20 分鐘）

今天只講三件事，11/30 再講影像與 BMP 的原理：

1. **和 Team 2 的差別**（規格「和前兩輪的差別」那張表）：一張 JPEG 要切很多片；缺片整張丟、不補；顯示來不及就丟舊的。
2. **固定的部分**：320×240、5 fps、片 ≤ 1 200 bytes、16 bytes header（magic、type、seq、frame_id、piece_idx、piece_cnt、ssrc）、HELLO／BYE、STATS 欄位。
3. **baseline 怎麼用**：`make_sequence` 做測試序列、`miniline_raw` 一張一個封包（看 IP 分段與遺失放大）、`camera-test` 確認攝影機、`python_ref/miniline.py` 當對手端、`tests/run_video_tests.py` 跑驗收 1–7。

第 1 週（11/23–11/29）目標：建 repo、跑通 baseline、分片／重組的 mock 模式過驗收 1 與 3。

## 回家作業／下週前要做的事

- Team 3：建 team repo（`mmsp2026-team3-gXX`），跑 `make smoke`、`make_sequence`、`miniline_raw`；讀 RFC 2435 的 3.1 節（真正的 MJPEG 是怎麼切片的）。
- 11/30：影像表示、BMP、Team 3 問答、補驗時段 2。
