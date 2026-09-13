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

教學見 [docs/tutorials/github_actions_ci.md](../docs/tutorials/github_actions_ci.md)。

## 待放入（依課綱）

- [ ] 可重現的 loss／reorder 模擬器（Team 2、Team 3 評測用）
- [ ] 網路／裝置 wrapper（音訊、camera、JPEG、display）
- [ ] mock transport（期末上機考與 mock mode 評分用）
- [ ] 測試素材：固定 WAV、JPEG 序列、測試軌跡
