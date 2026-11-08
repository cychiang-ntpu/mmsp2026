# 第 10 週（2026/11/9）｜Team 2 工作坊 ①：跟著 tutorial 做第 1–3 步（助教主持）

對應：Team 2（11/22 截止、11/23 評測）
時間：13:10–16:00，三節。老師出國，助教主持；問題彙整後老師 11/23 第一節統一回答。

## 做什麼

三節課就是**全班一起照 [tutorial/TUTORIAL.md](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 做第 1–3 步**。
每一步做完都要 **push 到 team repo**（以組為單位），下課前交半成品。

> **11/9 規格定版**：九個模組與七步**每一個 push 到 team repo 就是 40 分**（各自一個資料夾、至少一個 commit、`make` 零警告）；tutorial 第 7 步 `step7_live/step7_live.c` 是官方起點，之後在它上面加 μ-law、ADPCM（必做）、Huffman（加分）。11/23 demo 30 分、之後一週程式碼實質審查 30 分。詳見規格「[驗收與評分](../../team_projects/team2_voicelink/README.md#驗收與評分119-定版403030)」。

| 節次 | 時間 | 步 | 做完要看到 | push 什麼 |
|---|---|---|---|---|
| 1 | 13:10–14:00 | **Part A 模組**（20 分鐘）：`m1_wav`、`m2_udp_send`／`recv`、`m3_clock`、`m6_header`、`m7_ringbuffer` 各跑一次、改一個地方；**第 1 步 raw**（30 分鐘）：讀 WAV、每 20 ms 送 640 bytes、收到就寫檔 | 每個模組印出講義說的東西；`recv` 寫出的 WAV 聽得到；經過 `udp_impair.py --loss 0.05` 後變短、亂序 | `src/step1_raw.c`（可改名）＋ `TEAM_LOG.md` 寫「誰做了什麼」 |
| 2 | 14:10–15:00 | **第 2 步 header**：12 bytes header（seq、ts、ssrc）、BYE | 收端每包印 `seq=… ts=…`；搗蛋時**看得到 seq 跳號** | `src/…step2…`；`docs/interface.md` 寫下封包格式 |
| 3 | 15:10–15:50 | **第 3 步 jitter buffer**：環形 buffer、播放時鐘、repeat PLC（packet loss concealment，封包遺失補償）、基本 STATS | `python3 ../tests/run_mock_tests.py … --only 1,2,5` 三項 PASS | `src/…step3…`＋測試輸出截圖 |
| 3 | 15:50–16:00 | 收尾：登錄今天的 commit SHA 到 Slack 討論串；沒做完的回家做完，11/15（日）18:00 前 push | — |

## 助教要做的事

- 開場 5 分鐘：講今天三步與「每步 push」的規則；投影 tutorial。
- 每一步開始先用 5 分鐘帶大家看該步的「要加的東西」與程式碼差異（`diff step1_raw/step1_raw.c step2_header/step2_header.c`；每一步各自一個資料夾，README 在裡面），然後各組自己做，助教巡場。
- 卡住的組先問「`--probe`／印出的 seq 看到什麼」；不幫改程式，可以指到 TUTORIAL.md 的「常見錯誤」。
- 第三節結束前收集每組的 SHA 與「問老師的一題」貼 Slack `#team2-questions`；11/12 前整理成清單給老師。

## 給同學

- 帶筆電（每組至少兩台）、熱點、耳機；來之前 `git pull`、baseline `make` 編得過。
- 可以直接複製 tutorial 的 step 程式再改，但程式碼審查會看你們改的部分，demo 問答會問每一行在做什麼。
- 今天的繳交（規格①，每項 2.5 分）：m1–m9 與 step1–3 共 12 項，各自一個資料夾 push 到 team repo，每項至少一個 commit（訊息寫哪一項、誰做的），`make` 零警告；m 模組把 README「做完要能回答」的答案寫進 commit 訊息或 `TEAM_LOG.md`，step3 把 `--only 1,2,5` PASS 貼進 `TEAM_LOG.md`。**11/15（日）18:00 前**，逾期減半。今天助教只協助、不計分。
