#!/usr/bin/env bash
# 老師／助教用：列出全班個人 repo 最新一次 mp-ci 的結果與 commit SHA。
#   用法：bash class_status.sh repos.txt        （每列一個 owner/repo，# 開頭為註解）
#   需要 gh CLI 並已 gh auth login；輸出 TSV，可直接貼進試算表。
set -u
list="${1:-repos.txt}"
printf 'repo\tstatus\tconclusion\tsha\tupdated_at\turl\n'
grep -vE '^\s*(#|$)' "$list" | while read -r repo; do
    row=$(gh api "repos/$repo/actions/workflows/mp-ci.yml/runs?per_page=1" \
          --jq '.workflow_runs[0] | [.status, .conclusion, .head_sha[0:7], .updated_at, .html_url] | @tsv' 2>/dev/null)
    if [ -z "$row" ]; then row=$(printf 'no-workflow\t-\t-\t-\thttps://github.com/%s/actions' "$repo"); fi
    printf '%s\t%s\n' "$repo" "$row"
done
