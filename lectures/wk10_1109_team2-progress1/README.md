# 第 10 週（2026/11/9）｜Team 2 自習週：11/11（三）18:00 前把 m1–m9 全部 push

對應：Team 2（m1–m9 11/11 18:00、step1–6 11/16 16:00、SHA 登錄 11/22 18:00；11/23 demo 與分組口試）
**11/9 不上課、助教不到場**。老師出國；這一週各組自己在家、或自己約時間聚在一起，把下面的事做完。問題貼 Slack `#team2-questions`，助教線上回。

老師貼給全班的公告稿：[announcement_1109-1123.md](announcement_1109-1123.md)。

## 這一週要做完的事（11/11（三）18:00 前）

1. **建 team repo**：在 GitHub 建一個 **private** repo `mmsp2026-team2-gXX`，Settings → Collaborators 加入所有組員、老師與助教（同 Team 1）；把整個 [`team_projects/team2_voicelink/`](../../team_projects/team2_voicelink/) 複製成 `mmsp2026-team2-gXX`（**資料夾名稱不要改**，含 `.github/workflows/team2-check.yml` 與 `.gitignore`——後者擋掉編出來的執行檔，`git add .` 才不會把它們 commit 進去），push 一次，到 repo 的 Actions 分頁確認 `team2-check` 有跑起來。
2. **繳交 GitHub 連結**：每組一位代表到數位學苑 3.0（<https://lms3.ntpu.edu.tw>）的 Team 2 繳交區，交 team repo 的 GitHub 連結（例：`https://github.com/<帳號>/mmsp2026-team2-gXX`），**11/11（三）18:00 前**。助教從這個連結看你們的 Actions 結果；沒交連結、或沒把助教加成 collaborator（private repo 打不開），助教就看不到你們的進度。
3. **Part A 九個模組 m1–m9**：照 [tutorial/TUTORIAL.md](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 的順序，每個資料夾 `cd` 進去 `make`、跑一次、照 README「改一個地方」改一處再跑一次。每個模組**各自一個 commit**（訊息寫哪一個模組、誰做的），README「做完要能回答」的答案寫進 commit 訊息或 `TEAM_LOG.md`。
   - m4／m5 要麥克風與喇叭：用自己的筆電做；沒有裝置的電腦會印「沒有裝置」正常結束，也算做完（截圖貼 `TEAM_LOG.md`）。
   - m8、m9 各約 20 分鐘，README 寫得比較長，邊做邊學，是接下來寫 codec 的基礎。
4. **push**。Actions 綠勾＝m1–m9 都在、都編得過、該跑的檢查都過。這就是「繳交 40 分」的前 9 項（缺一項扣 3）。
5. 有餘力就往 Part B 走（第 1–3 步），11/16 當天會比較輕鬆；第 1–6 步的截止是 11/16（一）16:00。

## 自己檢查

在 team repo 根目錄（Windows PowerShell；macOS／Linux 把 `python` 換成 `python3`）：

```powershell
python tests\check_submission.py --only m --wav ..\mmsp2026\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav
```

印出來的 `[PASS]`／`[FAIL]` 就是 Actions 會看到的東西；`--deadline-report` 會列每個資料夾最後一次 commit 的時間。

## 分工建議（四人組）

| 人 | 模組 |
|---|---|
| A | m1 WAV、m6 header |
| B | m2 UDP（兩支）、m3 時鐘 |
| C | m4 錄音、m5 播放、m7 環形 buffer |
| D | m8 μ-law、m9 ADPCM |

每個人至少要有自己的 commit——程式碼審查的「個人貢獻 10 分」看的就是 `git log`。做完的人去看隊友的模組、互相問 README 最後那幾題。

## 下一次見面：11/16（一）工作坊 ②（助教主持）

照 [wk11](../wk11_1116_team2-progress2/README.md)：第 4–6 步，**16:00 下課前 step1–6 全部 push**；第三節開始 `mulaw_system`。

**麥克風與通話裝置**：11/16 課堂上，助教會帶實驗室的 USB 麥克風，借給同學在課堂上使用。也歡迎自己帶 USB 麥克風，或帶自己的筆記型電腦、耳機、麥克風，方便現場做實作、驗證兩台通話（第 6 步 live）。
