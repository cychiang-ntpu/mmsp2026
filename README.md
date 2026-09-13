# MMSP 2026（115-1）多媒體訊號處理 — 課程資料庫

國立臺北大學通訊工程學系大三必修「多媒體訊號處理」。
授課教師：江振宇（[教師個人網頁](https://web.ntpu.edu.tw/~cychiang/)）
本 repo 提供去年作業樣本、專題規格與課程時程，供修課同學 pull／fork 使用。
個人作業（MP1–MP5）的題目規格與樣本都在 samples_2025-C/，同學依規格自己寫，用 samples_2025-python/ 對答案。

## 教學目標

「**學為所用**」、「**將系統做出來！**」——把課本中的演算法或系統，
以程式語言（C language）實現出來，讓自己檢驗是否已求甚解。
在這門課裡，你有機會把大一到大三修過、與多媒體訊號處理相關的課程
（程式設計、微積分、機率、訊號與系統等）做一次綜合整合；
修完後，你將具備把課本中的數學式、演算法甚至整個系統實作出來的能力，
達到學為所用的境界。

## 教學目標及重點

1. 了解數位訊號處理之實際應用
2. 研習資料之表示方法及通訊方法
3. 數位文字、圖像、影像及聲音之表示及處理
4. 以程式語言實作基礎多媒體訊號處理系統

本學期的具體實踐：從文字（UTF-8、熵編碼）、聲音（PCM、STFT）到影像
（JPEG／MJPEG），一路把資料的表示、壓縮、封裝與網路傳輸做出來，
最終在區域網路上完成自己的 **MiniLINE** 即時通訊系統。

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
├── lectures/               每週上課講義與範例（依日曆週編號，上完課陸續發布）
├── docs/                   課程時程與評分方式摘要
│   └── tutorials/          新手教學文件（VSCode、終端機、Git、除錯…）
└── tools/                  課程工具：ci/（MP 自動測試，已可用）、loss／reorder 模擬器等（陸續發布）
```

## 課程 Slack（線上即時討論）

本課程使用 **Slack** 進行線上即時討論、公告資訊、分享資料與程式碼，
請同學務必用以下連結加入：

👉 [加入 ntpu-ce-mmsp-2026 Slack 工作區](https://join.slack.com/t/ntpu-ce-mmsp-2026/shared_invite/zt-48ss07vog-APg0gDwKIJUQ88j8nfRJPQ)

## 助教（TA）

| 姓名 | 系級 | 電子郵件 |
|---|---|---|
| 梁博森 | 通訊碩二 | benson20030603 [at] gmail.com |
| 洪子軒 | 通訊碩二 | loveiswar456789 [at] gmail.com |
| 廖經凱 | 通訊碩二 | kevin05251017 [at] gmail.com |
| 郭宸瑋 | 通訊碩一 | s711581117 [at] ms.ntpu.edu.tw |

問問題的建議順序：先查 [docs/tutorials/](docs/tutorials/) 的教學與
錯誤急救手冊 → Slack 頻道發問（附完整錯誤訊息與程式碼）→ 私訊助教。

## 第一堂課 checklist

1. **加入課程 Slack**（連結見上方），之後所有公告與討論都在那裡。
2. 讀 [docs/course_plan.md](docs/course_plan.md)：整學期的時程、評分與分組方式。
3. 照 [docs/tutorials/vscode_c_starter.md](docs/tutorials/vscode_c_starter.md)（Windows）、
   [macos_c_starter.md](docs/tutorials/macos_c_starter.md)（macOS）或
   [wsl_c_starter.md](docs/tutorials/wsl_c_starter.md)（WSL）把開發環境架起來，
   跑出 Hello World（文件結尾有其他教學的建議閱讀順序）。
4. 學會存檔點：[docs/tutorials/git_intro.md](docs/tutorials/git_intro.md)，
   並依課堂指示建置個人 repo。
5. 編譯執行 [team_projects/team1_textlink/baseline/chat.c](team_projects/team1_textlink/baseline/chat.c)
   聊天範例，和同學互傳訊息（步驟在 vscode_c_starter.md 步驟 6）。
6. 瀏覽 [samples_2025-C/](samples_2025-C/) 了解去年作業長什麼樣子。

## 個人作業（MP1–MP5）

- 五份個人 C 語言作業。題目規格與去年樣本在 [samples_2025-C/](samples_2025-C/) 各 mini_project 的 README，
  **依規格自己從頭寫**；正確輸出用 [samples_2025-python/](samples_2025-python/) 的 Python 實作對答案。
- 用 **GitHub Actions 自動建置與比對**：個人 repo 放一個 workflow 檔，每次 push 就自動編譯並和 Python 版對答案。
  設定方式見 [docs/tutorials/github_actions_ci.md](docs/tutorials/github_actions_ci.md)，本機也能跑同一支測試腳本。
- 評分用今年的私有測資、同一支腳本（照抄樣本無法通過），配合每週課堂講解。
- 統一截止：**2026/10/23（五）18:00**。push 到個人 repo 並登錄 commit SHA。

## 開發環境還沒設定好？

新手教學都在 [docs/tutorials/](docs/tutorials/)：VSCode／GCC 安裝
（Windows：vscode_c_starter.md；macOS：macos_c_starter.md；WSL：wsl_c_starter.md）、
終端機（terminal_basics.md）、C 速查表
（c_cheatsheet.md）、編譯錯誤急救（c_error_guide.md）、逐行除錯
（vscode_debug_tutorial.md）、Git（git_intro.md）、Makefile
（makefile_intro.md）、GitHub Actions 自動測試（github_actions_ci.md；入門可先看助教梁博森的
[最小範例](https://github.com/Bensonlllll/build_on_github_test)）。完成 Hello World 後再開始作業。
