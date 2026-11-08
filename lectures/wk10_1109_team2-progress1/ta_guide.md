# 助教主持手冊：11/9 與 11/16 的 Team 2 工作坊 ①②（跟著 tutorial 做）

老師出國，這兩週由助教主持。本手冊是兩天的操作流程；同學看的講義在 [11/9 ①](README.md) 與 [11/16 ②](../wk11_1116_team2-progress2/README.md)。

## 事前（11/8 前）

- [ ] 老師在 Slack 貼公告（文字見下方），並指定兩天的主持助教各一位、計時助教一位；11/16 第三節最好三到四位助教。
- [ ] 在 Slack 開討論串 `#team2-questions`，兩天「問老師的問題」都貼在那裡。
- [ ] 準備：白板筆、計時器、熱點、一台能投影的筆電。配對表印一份：
  - 11/9：1↔2、3↔4、5↔6、7↔8、9↔10、11↔1（11 組是奇數，第 11 組和第 1 組）。
  - 11/16：1↔3、2↔4、5↔7、6↔8、9↔11、10↔1。
- [ ] 教室電腦先跑一次 `python_ref/voicelink.py` 與 `tests/run_mock_tests.py`（python_ref 對 python_ref，7/7 PASS），確認環境能用。

## 當天共同規則

- 13:10 準時開始；5 分鐘講流程：**今天全班照 tutorial 做，每一步做完就 push**。
- 每一步：助教先用 5 分鐘投影該步的「要加的東西」與兩個 step 檔的差異，再各組自己做（約 40 分鐘），助教巡場。
- 卡住先問「印出來／`--probe` 看到什麼」；不代寫，指到 TUTORIAL.md 的「常見錯誤」；兩組都說自己對時用 `python_ref/voicelink.py` 當裁判。
- 每節結束前把各組「做到第幾步、SHA、問老師的一題」記下來貼 Slack `#team2-questions`。

## 11/9 ①：第 1–3 步（基本層）

| 時間 | 步 | 助教 |
|---|---|---|
| 13:10–14:00 | 第 1 步 raw | 帶看 `step1_raw.c` 的 send／recv 兩個迴圈；巡場確認每組 recv 寫得出 WAV |
| 14:10–15:00 | 第 2 步 header | 帶看 header 的 12 bytes 與 `put_u16`／`put_u32`；讓每組經過 `udp_impair.py --loss 0.05` 看 seq 跳號 |
| 15:10–15:50 | 第 3 步 jitter buffer | 帶看環形陣列、播放時鐘、repeat；目標每組 `run_mock_tests.py --only 1,2,5` PASS |
| 15:50–16:00 | 收 SHA 與問題；沒做完 11/15 18:00 前 push | |

## 11/16 ②：第 4–7 步（進階層）

| 時間 | 步 | 助教 |
|---|---|---|
| 13:10–14:00 | 第 4 步 counters（基本層沒過的先補第 3 步） | 帶看 late／dup／reordered 的判斷式與 `--plc zero` |
| 14:10–15:00 | 第 5 步 robust＋第 6 步 μ-law；兩組互測（1↔3、2↔4…） | 帶看壞封包檢查與 μ-law 查表；互測跑 `run_mock_tests.py` 兩個方向 |
| 15:10–15:45 | 第 7 步 live：兩組用熱點通話 | 沒麥克風的用 `python_ref` 當聲源 |
| 15:45–16:00 | 收尾：進度報告＋SHA 11/22 18:00；11/23 評測要帶的東西 | |

## 會後

- 11/9 的問題清單 11/12 前、11/16 的 11/19 前整理給老師（照類別合併）。
- 列「各組做到第幾步」的表（11/15、11/22 各一次）給老師。

## Slack 公告（老師貼）

**11/9 用：**

```
📢 11/9（一）老師出國，改為「Team 2 工作坊 ①」，助教主持，照常上課、照常點名。
帶筆電、熱點、耳機；來之前至少讓 send／recv 在自己電腦跑得起來。
全班一起照 tutorial（team_projects/team2_voicelink/tutorial/TUTORIAL.md）做第 1–3 步，每一步做完就 push 到 team repo：
第一節 第 1 步 raw → 第二節 第 2 步 header → 第三節 第 3 步 jitter buffer（做完就過基本層）
下課前把 SHA 貼到 Slack；沒做完的 11/15（日）18:00 前 push。
問題我 11/23 第一節統一回答。
講義：https://github.com/cychiang-ntpu/mmsp2026/blob/main/lectures/wk10_1109_team2-progress1/README.md
```

**11/16 用：**

```
📢 11/16（一）「Team 2 工作坊 ②」，助教主持，照常上課。離 11/22 截止只剩 6 天，今天是評測預演。
來之前先把 tests/run_mock_tests.py 自己對自己跑一遍，帶結果來。
接著做 tutorial 第 4–7 步，每步 push：第一節 第 4 步（完整 STATS、zero PLC（packet loss concealment，封包遺失補償））→ 第二節 第 5、6 步（壞封包、μ-law）＋兩組互測 → 第三節 第 7 步 live 通話
交：進度報告（docs/templates/progress_report.md）＋SHA，11/22（日）18:00 前。
11/23 評測：每組兩台筆電、耳機、熱點，P 的 slides.pdf 要在 SHA 裡。
講義：https://github.com/cychiang-ntpu/mmsp2026/blob/main/lectures/wk11_1116_team2-progress2/README.md
```
