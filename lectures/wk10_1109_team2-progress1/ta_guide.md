# 助教手冊：11/9 自習週（線上）與 11/16 Team 2 工作坊 ②（現場主持）

老師出國。**11/9 不上課、助教不到場**，同學自己把 m1–m9 做完 push（11/11 18:00 前），助教只在 Slack 線上回問題、11/12 看各組 Actions 結果。**11/16 由助教現場主持**第 4–6 步，16:00 前 step1–6 全部 push。同學看的講義在 [11/9](README.md) 與 [11/16 ②](../wk11_1116_team2-progress2/README.md)。

## 事前（11/8 前）

- [ ] 老師在 Slack 貼公告（文字見下方），並指定兩天的主持助教各一位、計時助教一位；11/16 第三節最好三到四位助教。
- [ ] 在 Slack 開討論串 `#team2-questions`，兩天「問老師的問題」都貼在那裡。
- [ ] 準備：白板筆、計時器、熱點、一台能投影的筆電。配對表印一份：
  - 11/9：1↔2、3↔4、5↔6、7↔8、9↔10、11↔1（11 組是奇數，第 11 組和第 1 組）。
  - 11/16：1↔3、2↔4、5↔7、6↔8、9↔11、10↔1。
- [ ] 教室電腦先跑一次 `python_ref/voicelink.py` 與 `tests/run_mock_tests.py`（python_ref 對 python_ref，7/7 PASS），確認環境能用。

## 當天共同規則

- （11/16）13:10 準時開始；5 分鐘講流程：**今天全班照 tutorial 做，每一步做完就 push**。
- 每一步：助教先用 5 分鐘投影該步的「要加的東西」與兩個 step 檔的差異，再各組自己做（約 40 分鐘），助教巡場。
- 卡住先問「印出來／`--probe` 看到什麼」；不代寫，指到 TUTORIAL.md 的「常見錯誤」；兩組都說自己對時用 `python_ref/voicelink.py` 當裁判。
- 每節結束前把各組「做到第幾步、SHA、問老師的一題」記下來貼 Slack `#team2-questions`。
- **評量**（11/9 定版）：工作坊兩天**不計分**。分數是規格①「tutorial 繳交 40」：每個模組、每一步各自一個資料夾 push 到 team repo、至少一個 commit、`make` 零警告（m1–m9 在 11/11（三）18:00 前、step1–6 在 11/16（一）16:00 前；各組 repo 的 Actions `team2-check` 綠勾＝15 項都在）。助教的工作是巡場協助，並在每節結束記錄各組做到第幾步。下表的「抽問」改成**巡場時順口問、答不出就指回 README**，不扣分：

  | 步 | 建議看的證據 | 順口問一題 |
  |---|---|---|
  | 1 | out.wav 與輸入的 bytes 數 | 掉包以後聲音為什麼變短而不是變難聽？ |
  | 2 | 收端前 5 行 seq／ts | ts 為什麼每包加 320 不是加 1？ |
  | 3 | `--only 1,2,5` PASS | buffer 大了，哪個數字變小、哪個變大？ |
  | 4 | 測試 7 輸出 | 用自己那一行代入 `frames_out = (received − late − dup − overflow) + concealed` |
  | 5 | 測試 5 輸出 | ssrc 突然變了，你們的程式做什麼？ |
  | 6 | 兩台 `call` 30 秒雙方 STATS | 同一台自己打自己為什麼有回音？ |
  | mulaw_system | 測試 8 的 SQNR 與 bytes | 為什麼 μ-law 的 bytes 剛好一半？header 有沒有變小？ |

## 11/9 自習週（助教線上）

| 時間 | 助教做什麼 |
|---|---|
| 11/9（一） | 在 Slack `#team2-questions` 貼公告（下方）；回答建 repo、Actions 跑不起來、make 編不過的問題 |
| 11/11（三）18:00 後 | 看 11 組 repo 的 Actions `team2-check`：綠勾＝m1–m9 都在；紅叉點進去看哪一項 FAIL，記錄 |
| 11/12（四）前 | 把「各組 m1–m9 完成狀況」與問題清單給老師 |

## 11/16 ②：第 4–6 步（做完＝能通話）＋mulaw_system 開工

| 時間 | 步 | 助教 |
|---|---|---|
| 13:10 | **點名**；看各組 Actions 狀態 | 缺席與 m1–m9 未綠勾的組記下來 |
| 13:10–14:00 | 第 4 步 counters（第 3 步沒過的先補） | 帶看 late／dup／reordered 的判斷式與 `--plc zero` |
| 14:10–15:00 | 第 5 步 robust＋第 6 步 μ-law；兩組互測（1↔3、2↔4…） | 帶看壞封包檢查與 μ-law 查表；互測跑 `run_mock_tests.py` 兩個方向 |
| 15:10–15:45 | 第 6 步 live（15 分鐘）＋ `mulaw_system` 開工（20 分鐘） | 沒麥克風的用 `python_ref` 當聲源；μ-law 先帶看 `tutorial/m8_mulaw`，再投影 `mulaw_system/voicelink.c` 裡的四個 `★ 第 8 步`，目標 `--only 8` 跑得完；ADPCM／Huffman 回家做（`adpcm_huffman_system/`，零件 m9） |
| 15:45–16:00 | 收尾：**16:00 前 step1–6 全部 push**（看各組 Actions）；進度報告＋SHA 11/22 18:00；11/23 評測要帶的東西 | |

## 會後

- 11/9 的問題清單 11/12 前、11/16 的 11/19 前整理給老師（照類別合併）。
- 列「各組做到第幾步」的表（11/11 看 m1–m9、11/16 16:00 看 step1–6；直接看各組 repo 的 Actions 結果）給老師。

## Slack 公告（老師貼）

**11/9 用：**

```
📢 11/9（一）不上課、助教不到場（老師出國）。這一週請各組自己（在家或自己約）完成：
1. 把 team_projects/team2_voicelink/ 整個複製成 team repo（mmsp2026-team2-gXX，資料夾名稱不要改），push 一次，看 Actions 的 team2-check 有沒有跑起來。
2. tutorial 的九個模組 m1–m9：每個資料夾 make、跑一次、照 README 改一處再跑，各自一個 commit（寫哪個模組、誰做的）。
3. 11/11（三）18:00 前全部 push；Actions 綠勾就是繳交 40 分的前 9 項（缺一項扣 3）。
問題貼 #team2-questions，助教線上回。11/16（一）照常上課（助教主持）：第 4–6 步，16:00 下課前 step1–6 全部 push。
講義：https://github.com/cychiang-ntpu/mmsp2026/blob/main/lectures/wk10_1109_team2-progress1/README.md
```

**11/16 用：**

```
📢 11/16（一）「Team 2 工作坊 ②」，助教主持，照常上課。今天 16:00 下課前 step1–6 要全部 push（Actions 綠勾）；離 11/22 登錄 SHA 只剩 6 天。
來之前先把 tests/run_mock_tests.py 自己對自己跑一遍，帶結果來。
接著做 tutorial 第 4–6 步，每步 push：第一節 第 4 步（完整 STATS、zero PLC（packet loss concealment，封包遺失補償））→ 第二節 第 5 步（壞封包）＋兩組互測 → 第三節 第 6 步 live 通話＋mulaw_system 開工
step1–6 在 16:00 前都 push 完 = 繳交 40 分到手；11/22 前做 mulaw_system（--only 8 PASS）與 adpcm_huffman_system（ADPCM --only 9、Huffman --only 10，demo 各 10 分）。
11/23 demo 30 分（live 通話 10、助教跑測試 1–9 共 10、報告問答 10）；之後一週程式碼實質審查 30 分（ADPCM 整合、正確性、品質、個人貢獻）。
交：進度報告（docs/templates/progress_report.md）＋SHA，11/22（日）18:00 前。
11/23 評測：每組兩台筆電、耳機、熱點，P 的 slides.pdf 要在 SHA 裡。
講義：https://github.com/cychiang-ntpu/mmsp2026/blob/main/lectures/wk11_1116_team2-progress2/README.md
```
