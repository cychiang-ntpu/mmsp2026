# tools/ — 課程工具

## ci/ — MP1–MP5 自動建置與測試（已可用）

| 檔案 | 給誰 | 用途 |
|---|---|---|
| [ci/mp-ci.yml](ci/mp-ci.yml) | 老師 | workflow 原始檔（hw-template 內有同一份；學生不需手動複製） |
| [ci/run_tests.sh](ci/run_tests.sh) | 同學、助教 | 建置 mp1–mp5 並與 Python 參考實作比對；本機與 GitHub Actions 跑同一支 |
| [ci/tests/](ci/tests/) | 同學 | 公開測資（評分另用私有測資，規則相同） |
| [ci/numcmp.py](ci/numcmp.py) | 腳本內部 | MP2 SQNR 與 MP5 spectrogram 的浮點容差比對 |
| [ci/class_status.sh](ci/class_status.sh) | 老師、助教 | 用 gh CLI 列出全班最新 CI 結果與 SHA |
| [ci/hw-template/](ci/hw-template/) | 老師 | 學生個人 repo 的 template（另建為 GitHub template repo，供 Use this template 或 Classroom 使用） |
| [ci/TEACHER_SETUP.md](ci/TEACHER_SETUP.md) | 老師、助教 | organization、template repo、Classroom 與評分的一次性設定 |
| [ci/build_course_code.sh](ci/build_course_code.sh) | 老師、同學 | 把課程裡所有範例、baseline、starter、python_ref 在 macOS／Linux／Windows 建置並跑一遍（GitHub Actions `build-course-code` 用同一支；同學的 Mac 編不過時跑它把輸出貼到 Slack） |
| [ci/class_rehearsal.ps1](ci/class_rehearsal.ps1) | 老師 | 上課彩排：在乾淨的 Windows 上照同學的做法跑一行安裝與當週 commands.md 的每個指令（GitHub Actions `class-rehearsal`） |
| [setup/lab_setup.ps1](setup/lab_setup.ps1) | 同學 | 電腦教室公用電腦一鍵安裝 Git、gcc、make、Python 與課程 repo（說明見 [lectures/wk03_0921_team1-kickoff/setup_first.md](../lectures/wk03_0921_team1-kickoff/setup_first.md)） |
| [md2pdf.py](md2pdf.py) | 老師 | 把課程 Markdown 印成 A4 PDF（考卷、紙本講義） |

教學見 [docs/tutorials/github_actions_ci.md](../docs/tutorials/github_actions_ci.md)。

## 依課綱的其他工具

- [x] 可重現的 loss／reorder／jitter／dup 模擬器：[team_projects/team2_voicelink/baseline/udp_impair.py](../team_projects/team2_voicelink/baseline/udp_impair.py)（`--seed` 可重現；Team 2 評測用）
- [x] 音訊／UDP wrapper：[team_projects/team2_voicelink/baseline/wrapper/](../team_projects/team2_voicelink/baseline/wrapper/)
- [x] Team 2 mock 模式自動驗收：[team_projects/team2_voicelink/tests/run_mock_tests.py](../team_projects/team2_voicelink/tests/run_mock_tests.py)
- [x] 測試素材：[lectures/wk07_1019_audio-pcm/data/](../lectures/wk07_1019_audio-pcm/data/)（16 kHz WAV）、[lectures/wk03_0921_team1-kickoff/data/](../lectures/wk03_0921_team1-kickoff/data/)（8 kHz WAV、BMP）
- [ ] camera／JPEG／display wrapper、JPEG 序列與測試軌跡（Team 3，11/30 開題時放入）
- [ ] mock transport（期末上機考）
