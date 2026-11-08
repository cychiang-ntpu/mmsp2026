# 第 11 週（2026/11/16）｜Team 2 工作坊 ②：跟著 tutorial 做第 4–7 步（助教主持）

對應：Team 2（11/22 截止、11/23 評測）
時間：13:10–16:00，三節。老師出國，助教主持。

## 做什麼

接著 [tutorial](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 做第 4–7 步；做完第 7 步就是**官方起點**（step4–7 push 完，繳交 40 分到手）。第 3 步還沒過的組先補。每步做完 push。
第三節開始寫這一輪真正的作業：**ADPCM**（`--codec adpcm`，必做 15 分）。演算法先在零件 [m9_adpcm](../../team_projects/team2_voicelink/tutorial/m9_adpcm/README.md) 做一遍（`--trace`、四個實驗，約 20 分鐘），再照它第 6 節把 frame 版接進 step7；對照 `python_ref/voicelink.py` 的 `adpcm_encode`／`adpcm_decode`。

| 節次 | 時間 | 步 | 做完要看到 | push 什麼 |
|---|---|---|---|---|
| 1 | 13:10–14:00 | **第 4 步 counters**：late／dup／reordered、`--plc zero`、完整 STATS | `run_mock_tests.py … --only 1,2,3,5,7` PASS | `src/…step4…` |
| 2 | 14:10–15:00 | **第 5 步 robust**：壞封包不當掉、ssrc 重置、seq 繞回；**第 6 步 μ-law**：`--codec mulaw` | `--only 1,2,3,4,5,6,7` PASS；`--only 8` PASS（SQNR ≥ 30 dB、bytes 減半） | `src/…step5…`、`src/…step6…` |
| 3 | 15:10–15:45 | **第 7 步 live**（15 分鐘）：`call` 兩組用熱點通話，換 `--plc zero`、`--buffer 200` 聽差別；**ADPCM 開工**（20 分鐘）：在 step7 的 `--codec` 分支加 `adpcm`，先讓 `--only 9` 的「bytes 0.264」那一項過（先做 m9、再把 `adpcm_reconstruct`／`adpcm_encode_one` 複製進 step7 寫 frame 版） | 聽得到（單向也算）；`--only 9` 至少跑得完 | `src/…step7…`＋ 30 秒錄影或截圖；ADPCM 的半成品 commit |
| 3 | 15:45–16:00 | 收尾：進度報告（[範本](../../docs/templates/progress_report.md)）＋ SHA，11/22（日）18:00 前；11/23 帶兩台筆電、耳機、熱點，P 的 `slides.pdf` 要在 SHA 裡 | — |

## 助教要做的事

- 同 11/9：每步先帶看 5 分鐘差異，再各組做，巡場不代寫。
- 第二節：兩組一組互測（配對 1↔3、2↔4、…）：`run_mock_tests.py` 跑「你們 send 對他們 recv」與反向。
- 第三節 live：沒有麥克風的電腦用 `python_ref/voicelink.py send` 當聲源代替。
- 16:00 前把「問老師的問題」貼 Slack；11/19 前整理給老師。

## 給同學

- 第 3 步沒過的組，今天第一節先把它過了，再往下。
- 今天的繳交（規格①）：step4–7 各自一個資料夾 push，每步至少一個 commit、對應的 `--only` PASS 貼進 `TEAM_LOG.md`，step7 貼兩台 `call` 的 STATS 截圖；**11/22（日）18:00 前**。互測不計分，但互測不過的程式 11/23 的 mock 測試（10 分）也不會過。
- 回家：11/22 前把 ADPCM 做到 `--only 9` PASS；有餘力再做 Huffman（測試 10）。
- 進度報告（[docs/templates/progress_report.md](../../docs/templates/progress_report.md)）寫進度表、卡住的問題、想問老師的三件事；11/22 與 SHA 一起交。
