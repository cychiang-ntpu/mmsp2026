# 老師／助教：MP 自動測試的 GitHub 端設定（一次性）

學生端教學在 [docs/tutorials/github_actions_ci.md](../../docs/tutorials/github_actions_ci.md)。
這份是老師端要做的事，依序約 30 分鐘。

## 1. 建立 organization（Classroom 需要；只用 template 可跳過）

1. <https://github.com/organizations/plan> → Create a free organization，名稱建議 `ntpu-ce-mmsp-2026`（與 Slack 同名）。
2. 用學校信箱申請教育方案：<https://education.github.com/teachers>（Classroom 的私有 repo 與 Actions 額度靠這個）。
3. Settings → Actions → General：Allow all actions（預設即可）。
4. People → 邀請四位助教，角色 Owner 或 Member 皆可（Classroom 另外設 TA）。

## 2. 建立作業 template repo

1. 在 organization（或你的帳號）新建 repo `mmsp2026-hw-template`（organization `ntpu-ce-mmsp-2026` 底下），**Public**（template 本身沒有作業內容，公開才能讓學生 Use this template；Classroom 用的話 private 也可以）。
2. 把本 repo 的 [hw-template/](hw-template/) 整個內容 push 上去（含 `.github/`、`.gitignore`、`.gitattributes`）：

   ```
   cp -R tools/ci/hw-template /tmp/hw && cd /tmp/hw
   git init -b main && git add -A && git commit -m "MP homework template"
   git remote add origin git@github.com:ntpu-ce-mmsp-2026/mmsp2026-hw-template.git
   git push -u origin main
   ```

3. 該 repo 的 Settings → 勾選 **Template repository**。
4. 自己按一次 Use this template 建一個測試 repo，放進一份 mp1.c push，確認 Actions 綠勾。
   **這一步請務必做**，本機只驗過腳本，GitHub 上的 checkout 與 numpy 安裝尚未實跑。

## 3. 建立 Classroom 作業

1. <https://classroom.github.com> → New classroom → 選 organization。
2. Students → 上傳名單（學號一列一個），學生接受作業時自己認領。
3. New assignment：
   - Title：`mmsp2026-hw`；repository prefix 會變成 repo 名前綴。
   - Individual assignment；Private；**Template repository** 選 `mmsp2026-hw-template`。
   - Deadline 填 2026-10-23 18:00（Classroom 只記錄，不鎖 push；評分以登錄的 SHA 為準）。
   - Grant students admin access：**不勾**（避免學生改 visibility 或刪 repo）。
   - Autograding：不用設，我們用 repo 內的 Actions。
4. 複製 assignment 的邀請連結，貼到 Slack。

## 4. 評分

- 公開測資：`tools/ci/tests/`。私有測資另放一個資料夾（**不要**放進本 repo），建議加入含 `\r`、BOM、空檔、大檔的案例，並先決定 BOM 是否算符號。
- 對每位學生的登錄 SHA：

  ```
  git clone <repo> s && cd s && git checkout <SHA>
  TESTS_DIR=/path/to/private_tests bash /path/to/mmsp2026/tools/ci/run_tests.sh
  ```

  結尾的「通過／失敗／略過」數字即測試群組得分依據；`.ci_out/` 留有每份輸出可覆核。
- 全班狀態總覽（Classroom 儀表板也有，這個可匯出 TSV）：

  ```
  cp tools/ci/repos.example.txt tools/ci/repos.txt   # 填入全班 owner/repo
  bash tools/ci/class_status.sh tools/ci/repos.txt > status.tsv
  ```

## 5. 課程 repo 必須保持公開

學生的 workflow 用 `actions/checkout` 抓 `cychiang-ntpu/mmsp2026`。若日後改私有，需在 template 的 mp-ci.yml 加 token，並讓每位學生設定 secret，成本高，不建議。
