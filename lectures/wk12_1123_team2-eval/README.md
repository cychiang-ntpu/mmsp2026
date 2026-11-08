# 第 12 週（2026/11/23）｜Team 2 demo 與口試日（流程同 Team 1）、Team 3 提早開題

對應：Team 2、Team 3
時間：13:10–16:00，三節。

> 每組帶**兩台筆電、耳機、手機熱點**；P 帶好 `slides.pdf`。評測對手端用 [python_ref/voicelink.py](../../team_projects/team2_voicelink/python_ref/voicelink.py)。

## 時程

| 節次 | 時間 | 內容 |
|---|---|---|
| 1–2 | 13:10–15:00 | **Team 2 demo**（同 Team 1 的流程）：11 組，每組約 9 分鐘（P 報告 4 分鐘 → D 現場展示 3 分鐘 → 問答 2 分鐘）；mock 驗收由助教在旁邊的電腦同步跑 |
| 3 | 15:10–15:40 | **Team 2 個人口試**（同 Team 1：分散到各組桌邊，老師與助教分頭問）：每人 2–3 分鐘，指著自己 commit 的一段程式講它在做什麼、一題概念；結果作為程式碼審查「個人貢獻 10 分」的依據。live 失敗的組趁這一節重試一次 |
| 3 | 15:40–16:00 | **Team 3 開題（規格導讀）**：[規格](../../team_projects/team3_miniline/README.md)、[baseline](../../team_projects/team3_miniline/baseline/)、分組公告 |

## Team 2 評測流程（每組）

1. **登錄 SHA 的版本**：助教事先 clone、`make`、跑 `tests/run_mock_tests.py`（老師的 WAV、兩個方向：你們 send 對參考 recv、參考 send 對你們 recv）。`tutorial/step6_live` 跑測試 1–7（raw 通話 10 分的條件之一）、`mulaw_system` 跑 8（μ-law 10 分）、`adpcm_huffman_system` 跑 9（ADPCM 10 分）與 10（Huffman 10 分，你們對你們）。結果表投影，不佔報告時間。
2. **P 報告（4 分鐘；不另計分，審查時對照）**：**codec 比較表**（pcm／mulaw／adpcm／huff 的每包 bytes、kb/s、SQNR、掉包聽感）是主菜；ADPCM 每包歸零的理由；有做 Huffman 的講表怎麼建、省了多少、為什麼 μ-law 上省得多；最後一分鐘講 buffer 設多少、repeat 與 zero 的差別。
3. **D 現場展示（3 分鐘）**：兩台電腦用熱點 `call` 通話 30 秒（raw，10 分）；當場切 `--codec mulaw`（μ-law 10 分的現場部分）、`--codec adpcm`（ADPCM 10 分的現場部分），再切 `--plc zero` 聽差別；有 Huffman 的切 `huff-mulaw` 給大家看 STATS 的 bytes 變少（Huffman 10 分的現場部分）。
4. **問答（2 分鐘）**：老師問 1–2 題（μ-law 為什麼小聲音也清楚；ADPCM 為什麼用重建值預測、每包歸零的代價；Huffman 表為什麼可以不送；seq 與 ts 為什麼分開；遲到的包為什麼要丟）。
5. **程式碼實質審查（30 分，11/23 後一週內，助教）**：看登錄 SHA 的 repo（照 `team2_voicelink/` 的結構）——正確性與互通 10（測試 8／9／10、codec 表、ADPCM 細節）、程式品質 10、個人貢獻 10（看 `git log`，沒有 C commit 的人 0 分）。審查期間可能找你當面解釋自己寫的一段。

評分（11/9 定版）：**tutorial 繳交 40**（m1–m9、step1–6 共 15 項各自一個資料夾 push，缺一項扣 3）＋**今天 demo 最多 40**（raw 通話 10、μ-law 10、ADPCM 10、Huffman 10，一項一項加）＋**助教程式碼實質審查最多 30**（正確性與互通 10、程式品質 10、個人貢獻 10）。**加總超過 100 以 100 計。**

## Team 3 開題（20 分鐘）

今天只講三件事，11/30 再講影像與 BMP 的原理：

1. **和 Team 2 的差別**（規格「和前兩輪的差別」那張表）：一張 JPEG 要切很多片；缺片整張丟、不補；顯示來不及就丟舊的。
2. **固定的部分**：320×240、5 fps、片 ≤ 1 200 bytes、16 bytes header（magic、type、seq、frame_id、piece_idx、piece_cnt、ssrc）、HELLO／BYE、STATS 欄位。
3. **baseline 怎麼用**：`make_sequence` 做測試序列、`miniline_raw` 一張一個封包（看 IP 分段與遺失放大）、`camera-test` 確認攝影機、`python_ref/miniline.py` 當對手端、`tests/run_video_tests.py` 跑驗收 1–7。

第 1 週（11/23–11/29）目標：建 repo、跑通 baseline、分片／重組的 mock 模式過驗收 1 與 3。

## 回家作業／下週前要做的事

- Team 3：建 team repo（`mmsp2026-team3-gXX`），跑 `make smoke`、`make_sequence`、`miniline_raw`；讀 RFC 2435 的 3.1 節（真正的 MJPEG 是怎麼切片的）。
- 11/30：影像表示、BMP、Team 3 問答、補驗時段 2。
