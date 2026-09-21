# 課前 Slack 通知（上課前半小時貼）

下面框內的文字可以整段複製貼到 Slack。對應的圖文說明是 [setup_first.md](setup_first.md)，上課要打的指令是 [commands.md](commands.md)。

```
📢【今天 13:10 多媒體訊號處理】一進教室、一開機就先做這件事（約 5 分鐘）

電腦教室的公用電腦沒有 Git、gcc、make，而且可能每次重開機就還原。
今天三節課都要跟著打指令，請一坐下就先裝好，不要等到老師開始講才裝。

① 按 Windows 鍵，打 powershell，按 Enter
② 貼上這一行，按 Enter：

irm https://raw.githubusercontent.com/cychiang-ntpu/mmsp2026/main/tools/setup/lab_setup.ps1 | iex

③ 等到出現綠色的 ALL SET 就完成了。它下面會印一行 cd "…\examples"，把那一行貼上去，就到今天上課的資料夾了。
（會下載約 200–250 MB；全班一起下載會慢一點，等的時候不要關視窗。）

它會自動裝好 Git、gcc、make、Python，並把課程的 mmsp2026 資料夾抓到桌面；電腦上已經有的會跳過，不需要系統管理員權限。
出現紅字或中途斷線：同一行再貼一次就好（裝好的部分會跳過）。再不行就舉手。

• 圖文說明：https://github.com/cychiang-ntpu/mmsp2026/blob/main/lectures/wk03_0921_team1-kickoff/setup_first.md
• 今天上課要打的指令（一頁版，請開著）：https://github.com/cychiang-ntpu/mmsp2026/blob/main/lectures/wk03_0921_team1-kickoff/commands.md
• 用自己筆電、第 1 週已經裝好的同學：在 mmsp2026 資料夾打 git pull 就可以了。macOS／Linux 的同學不用跑上面那一行。
```

這一行安裝指令與 commands.md 裡的每一個 Windows 指令，都在乾淨的 Windows 上照同學的做法彩排過
（GitHub Actions 的 `class-rehearsal`：電腦上什麼都沒有、以及已有 `C:\msys64` 兩種情況）。
