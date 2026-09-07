# MMSP 2026（115-1）多媒體訊號處理 — 課程資料庫

國立臺北大學通訊工程學系大三必修「多媒體訊號處理」。
本 repo 提供去年作業樣本、專題規格與課程時程，供修課同學 pull／fork 使用。
個人作業（MP1–MP5）的 starter 與規格將於第 1 週另行發布。

## 目錄結構

```
mmsp2026/
├── samples_2025-C/         2025 年 C 作業樣本（MP1 完整可編譯；其餘僅供
│                           參考結構與評語，關鍵演算法行已移除）
├── samples_2025-python/    Python 正確實作——同學對答案用（高階寫法，
│                           不透露 C 的實作方式）
├── team_projects/          三次 Team Project 的規格與 baseline
│   ├── team1_textlink/     TextLink：文字與封包（評測 10/12）
│   ├── team2_voicelink/    VoiceLink：即時語音管線（評測 11/9）
│   └── team3_miniline/     MiniLINE：視訊與整合（評測 12/7）
├── docs/                   課程時程與評分方式摘要
│   └── tutorials/          新手教學文件（VSCode、終端機、Git、除錯…）
└── tools/                  課程工具（loss／reorder 模擬器等，陸續發布）
```

## 課程 Slack（線上即時討論）

本課程使用 **Slack** 進行線上即時討論、公告資訊、分享資料與程式碼，
請同學務必用以下連結加入：

👉 [加入 ntpu-ce-mmsp-2026 Slack 工作區](https://join.slack.com/t/ntpu-ce-mmsp-2026/shared_invite/zt-48ss07vog-APg0gDwKIJUQ88j8nfRJPQ)

## 助教（TA）

| 姓名 | 學號 | 電子郵件 |
|---|---|---|
| 梁博森 | 711481101 | benson20030603@gmail.com |
| 洪子軒 | 711481104 | loveiswar456789@gmail.com |
| 廖經凱 | 711481114 | kevin05251017@gmail.com |
| 郭宸瑋 | 711581117 | s711581117@ms.ntpu.edu.tw |

問問題的建議順序：先查 [docs/tutorials/](docs/tutorials/) 的教學與
錯誤急救手冊 → Slack 頻道發問（附完整錯誤訊息與程式碼）→ 私訊助教。

## 第一堂課 checklist

1. **加入課程 Slack**（連結見上方），之後所有公告與討論都在那裡。
2. 讀 [docs/course_plan.md](docs/course_plan.md)：整學期的時程、評分與分組方式。
3. 照 [docs/tutorials/vscode_c_starter.md](docs/tutorials/vscode_c_starter.md)
   把開發環境架起來，跑出 Hello World（文件結尾有其他教學的建議閱讀順序）。
4. 學會存檔點：[docs/tutorials/git_intro.md](docs/tutorials/git_intro.md)，
   並依課堂指示建置個人 repo。
5. 編譯執行 [team_projects/team1_textlink/baseline/chat.c](team_projects/team1_textlink/baseline/chat.c)
   聊天範例，和同學互傳訊息（步驟在 vscode_c_starter.md 步驟 6）。
6. 瀏覽 [samples_2025-C/](samples_2025-C/) 了解去年作業長什麼樣子。

## 個人作業（MP1–MP5）

- 五份「除錯／CI」作業：拿到含 bug 的 starter，把它修到通過全部評分測試。
- 統一截止：**2026/10/23（五）18:00**。push 到個人 repo 並登錄 commit SHA。
- starter、規格與測試將於第 1 週（9/7）發布，配合每週課堂講解。

## 開發環境還沒設定好？

新手教學都在 [docs/tutorials/](docs/tutorials/)：VSCode／GCC 安裝
（vscode_c_starter.md）、終端機（terminal_basics.md）、C 速查表
（c_cheatsheet.md）、編譯錯誤急救（c_error_guide.md）、逐行除錯
（vscode_debug_tutorial.md）、Git（git_intro.md）、Makefile
（makefile_intro.md）。完成 Hello World 後再開始作業。
