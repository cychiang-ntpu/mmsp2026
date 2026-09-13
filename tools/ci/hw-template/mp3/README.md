# MP3

把你的 C 程式放在這個資料夾。規則（詳見課程 repo 的 docs/tutorials/github_actions_ci.md）：

- 執行檔名稱必須是 `mp3`。沒有 Makefile 時會自動 `gcc -Wall -Wextra -std=c99 -o mp3 *.c -lm`；
  有 Makefile 時會 `make mp3`。
- 命令列參數與課程 repo `samples_2025-python/mini_project_3/` 的 Python 程式完全相同。
- 本機先跑 `bash ../mmsp2026/tools/ci/run_tests.sh mp3` 看到全綠再 push。

寫好後可以把這個 README 改成你的說明，或直接刪掉。
