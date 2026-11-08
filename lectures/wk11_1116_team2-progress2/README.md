# 第 11 週（2026/11/16）｜Team 2 工作坊 ②：跟著 tutorial 做第 4–6 步，開始 μ-law（助教主持）

對應：Team 2（11/22 截止、11/23 評測）
時間：13:10–16:00，三節。老師出國，助教主持。

## 做什麼

接著 [tutorial](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 做第 4–6 步；做完第 6 步就是**能兩台通話的 voicelink**（step4–6 push 完，繳交 40 分到手）。第 3 步還沒過的組先補。每步做完 push。
第三節開始寫這一輪真正的作業：**μ-law**（[mulaw_system/](../../team_projects/team2_voicelink/mulaw_system/README.md)：框架已給，程式裡四個 `★ 第 8 步` 就是要動手的地方；零件 m8）。回家做 [adpcm_huffman_system/](../../team_projects/team2_voicelink/adpcm_huffman_system/README.md)：ADPCM（零件 m9，demo 10 分）或／和 Huffman（demo 10 分）。

| 節次 | 時間 | 步 | 做完要看到 | push 什麼 |
|---|---|---|---|---|
| 1 | 13:10–14:00 | **第 4 步 counters**：late／dup／reordered、`--plc zero`、完整 STATS | `run_mock_tests.py … --only 1,2,3,5,7` PASS | `tutorial/step4_counters/` |
| 2 | 14:10–15:00 | **第 5 步 robust**：壞封包不當掉、ssrc 重置、seq 繞回；兩組互測 | `--only 1,2,3,4,5,6,7` PASS；互測兩個方向 PASS | `tutorial/step5_robust/` |
| 3 | 15:10–15:45 | **第 6 步 live**（15 分鐘）：`call` 兩組用熱點通話，換 `--plc zero`、`--buffer 200` 聽差別；**mulaw_system 開工**（20 分鐘）：照 `★ 第 8 步` (1)–(4) 把 m8 的表加進 codec 表，先讓 `--only 8` 的「bytes 0.509」那一項過 | 聽得到（單向也算）；`--only 8` 至少跑得完 | `tutorial/step6_live/`＋ 30 秒錄影或截圖；`mulaw_system/` 的半成品 commit |
| 3 | 15:45–16:00 | 收尾：**16:00 前 step1–6 全部 push**（team repo 的 Actions 綠勾）；進度報告（[範本](../../docs/templates/progress_report.md)）＋ SHA，11/22（日）18:00 前；11/23 帶兩台筆電、耳機、熱點，P 的 `slides.pdf` 要在 SHA 裡 | — |

## 助教要做的事

- 同 11/9：每步先帶看 5 分鐘差異，再各組做，巡場不代寫。
- 第二節：兩組一組互測（配對 1↔3、2↔4、…）：`run_mock_tests.py` 跑「你們 send 對他們 recv」與反向。
- 第三節 live：沒有麥克風的電腦用 `python_ref/voicelink.py send` 當聲源代替。
- 16:00 前把「問老師的問題」貼 Slack；11/19 前整理給老師。

## 給同學

- 第 3 步沒過的組，今天第一節先把它過了，再往下。
- 今天的繳交（規格①）：step4–6 各自一個資料夾 push，每步至少一個 commit、對應的 `--only` PASS 貼進 `TEAM_LOG.md`，step6 貼兩台 `call` 的 STATS 截圖；**今天 16:00 下課前**（GitHub Actions `team2-check` 綠勾）。互測不計分，但互測不過的程式 11/23 的 demo 也不會過。
- 回家：11/22 前 `mulaw_system` 過 `--only 8`；`adpcm_huffman_system` 做 ADPCM（`--only 9`，demo 10 分）或／和 Huffman（`--only 10 --with-huffman`，demo 10 分）。team repo 照 `team2_voicelink/` 的結構，助教照路徑批改。
- 進度報告（[docs/templates/progress_report.md](../../docs/templates/progress_report.md)）寫進度表、卡住的問題、想問老師的三件事；11/22 與 SHA 一起交。
