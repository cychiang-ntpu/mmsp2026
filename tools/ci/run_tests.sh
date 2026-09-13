#!/usr/bin/env bash
# MP1–MP5 建置＋與 Python 參考實作比對。GitHub Actions 與本機用同一支腳本。
#
#   用法（在你的個人 repo 根目錄）：
#     bash <課程repo>/tools/ci/run_tests.sh            # 測所有存在的 mp1..mp5
#     bash <課程repo>/tools/ci/run_tests.sh mp1 mp3    # 只測指定的
#   環境變數：
#     COURSE_DIR   課程 repo 路徑（預設：本腳本所在 repo）
#     TESTS_DIR    測資資料夾（預設：$COURSE_DIR/tools/ci/tests；助教評分時換成私有測資）
#     PY           python 指令（預設 python3，不能用時退回 python）
#
# 個人 repo 約定：mp1/ … mp5/ 各一個資料夾，裡面有 Makefile（產生執行檔 mpN）
# 或單一 .c 檔（腳本會用 gcc -Wall -Wextra -std=c99 -lm 編譯）。
# 執行檔的命令列參數必須和 samples_2025-python 的對應程式完全相同。
set -u
CI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COURSE_DIR="${COURSE_DIR:-$(cd "$CI_DIR/../.." && pwd)}"
TESTS_DIR="${TESTS_DIR:-$CI_DIR/tests}"
# python 指令：預設 python3；Windows 上 python3 常是 Microsoft Store 的空殼（跑了沒輸出），退回 python
if [ -z "${PY:-}" ]; then
    if python3 --version > /dev/null 2>&1; then PY=python3; else PY=python; fi
fi
REF="$COURSE_DIR/samples_2025-python"
ROOT="$(pwd)"
WORK="$ROOT/.ci_out"; rm -rf "$WORK"; mkdir -p "$WORK"

PASS=0; FAIL=0; SKIP=0
ok()   { echo "  ✅ $*"; PASS=$((PASS+1)); }
bad()  { echo "  ❌ $*"; FAIL=$((FAIL+1)); }
skip() { echo "  ⏭  $*"; SKIP=$((SKIP+1)); }
section() { echo; echo "=== $* ==="; }

# 建置 mpN：有 Makefile 用 make，否則 gcc 所有 .c；成功時把執行檔路徑放進 BIN
build() {
    local mp="$1" dir="$ROOT/$1"
    [ -d "$dir" ] || { skip "${mp}/ 不存在，略過"; return 1; }
    if [ ! -f "$dir/Makefile" ] && ! ls "$dir"/*.c > /dev/null 2>&1; then skip "${mp}/ 還沒有 .c 檔或 Makefile，略過"; return 1; fi
    ( cd "$dir" && rm -f "$mp" &&
      if [ -f Makefile ]; then make -s "$mp" || make -s; else gcc -Wall -Wextra -std=c99 -o "$mp" *.c -lm; fi
    ) > "$WORK/$mp.build.log" 2>&1
    if [ ! -x "$dir/$mp" ]; then bad "$mp 建置失敗或沒有產生執行檔 $mp/${mp}（看 .ci_out/$mp.build.log）"; sed 's/^/     /' "$WORK/$mp.build.log" | head -30; return 1; fi
    if grep -q 'warning:' "$WORK/$mp.build.log"; then echo "  ⚠️  $mp 有編譯警告（不扣分，但請修）："; grep 'warning:' "$WORK/$mp.build.log" | head -5 | sed 's/^/     /'; fi
    ok "$mp 建置成功"; BIN="$dir/$mp"
}

# ---------------- MP1：符號統計 → CSV 逐 byte 相同 ----------------
test_mp1() {
    section "MP1 symbol statistics"; build mp1 || return
    for in_ in "$TESTS_DIR"/mp1_*.txt "$TESTS_DIR"/text_*.txt; do
        n=$(basename "$in_" .txt)
        "$PY" "$REF/mini_project_1/symbol_stats.py" "$in_" "$WORK/mp1_$n.ref.csv"
        if ! "$BIN" "$in_" "$WORK/mp1_$n.out.csv" > "$WORK/mp1_$n.log" 2>&1; then bad "${n}：程式執行失敗（exit≠0）"; continue; fi
        if diff -q "$WORK/mp1_$n.out.csv" "$WORK/mp1_$n.ref.csv" > /dev/null; then ok "$n"; else bad "${n}：CSV 與參考不同"; diff "$WORK/mp1_$n.out.csv" "$WORK/mp1_$n.ref.csv" | head -8 | sed 's/^/     /'; fi
    done
}

# ---------------- MP2：WAV 逐 byte 相同 ＋ SQNR 相同 ----------------
test_mp2() {
    section "MP2 function generator"; build mp2 || return
    while read -r fs bits ch kind f amp sec; do
        [ -z "$fs" ] && continue; n="${kind}_${fs}_${bits}b_${ch}ch"
        "$PY" "$REF/mini_project_2/wavegen.py" $fs $bits $ch $kind $f $amp $sec "$WORK/mp2_$n.ref.wav" > "$WORK/mp2_$n.ref.log"
        if ! "$BIN" $fs $bits $ch $kind $f $amp $sec "$WORK/mp2_$n.out.wav" > "$WORK/mp2_$n.out.log" 2>&1; then bad "${n}：程式執行失敗"; continue; fi
        if ! cmp -s "$WORK/mp2_$n.out.wav" "$WORK/mp2_$n.ref.wav"; then bad "${n}：WAV 內容不同（$(cmp "$WORK/mp2_$n.out.wav" "$WORK/mp2_$n.ref.wav" 2>&1 | head -1)）"; continue; fi
        if "$PY" "$CI_DIR/numcmp.py" --sqnr "$WORK/mp2_$n.out.log" "$WORK/mp2_$n.ref.log" > /dev/null 2>"$WORK/mp2_$n.err"; then ok "$n"; else bad "${n}：$(cat "$WORK/mp2_$n.err")"; fi
    done < <(grep -v '^#' "$TESTS_DIR/mp2_cases.txt")
}

# ---------------- MP3：FLC codebook 與 bin 逐 byte 相同；自己 decode 還原 ----------------
test_mp3() {
    section "MP3 fixed-length codec"; build mp3 || return
    for in_ in "$TESTS_DIR"/text_*.txt; do
        n=$(basename "$in_" .txt); o="$WORK/mp3_$n"
        "$PY" "$REF/mini_project_3/flc_codec.py" encode "$in_" "$o.ref.csv" "$o.ref.bin"
        if ! "$BIN" encode "$in_" "$o.out.csv" "$o.out.bin" > "$o.log" 2>&1; then bad "${n}：encode 執行失敗"; continue; fi
        cmp -s "$o.out.csv" "$o.ref.csv" || { bad "${n}：codebook.csv 與參考不同"; diff "$o.out.csv" "$o.ref.csv" | head -6 | sed 's/^/     /'; continue; }
        cmp -s "$o.out.bin" "$o.ref.bin" || { bad "${n}：encoded.bin 與參考不同"; continue; }
        if ! "$BIN" decode "$o.out.bin" "$o.out.csv" "$o.dec.txt" >> "$o.log" 2>&1; then bad "${n}：decode 執行失敗"; continue; fi
        cmp -s "$o.dec.txt" "$in_" && ok "$n" || bad "${n}：decode 後與原文不同"
    done
}

# ---------------- MP4：Huffman 樹平手順序可不同 → 驗 bin 大小相同 ＋ 交叉 decode ----------------
test_mp4() {
    section "MP4 Huffman codec"; build mp4 || return
    for in_ in "$TESTS_DIR"/text_*.txt; do
        n=$(basename "$in_" .txt); o="$WORK/mp4_$n"
        "$PY" "$REF/mini_project_4/huffman_codec.py" encode "$in_" "$o.ref.csv" "$o.ref.bin" > /dev/null
        if ! "$BIN" encode "$in_" "$o.out.csv" "$o.out.bin" > "$o.log" 2>&1; then bad "${n}：encode 執行失敗"; continue; fi
        a=$(wc -c < "$o.out.bin"); b=$(wc -c < "$o.ref.bin")
        [ "$a" -eq "$b" ] || { bad "${n}：encoded.bin 大小 $a bytes，參考 $b bytes"; continue; }
        "$PY" "$REF/mini_project_4/huffman_codec.py" decode "$o.out.bin" "$o.out.csv" "$o.pydec.txt" 2>/dev/null
        cmp -s "$o.pydec.txt" "$in_" || { bad "${n}：用 Python decoder 解你的 bin＋codebook，還原結果與原文不同"; continue; }
        if ! "$BIN" decode "$o.ref.bin" "$o.ref.csv" "$o.cdec.txt" >> "$o.log" 2>&1; then bad "${n}：decode 執行失敗"; continue; fi
        cmp -s "$o.cdec.txt" "$in_" && ok "$n" || bad "${n}：用你的 decoder 解 Python 的 bin＋codebook，還原結果與原文不同"
    done
}

# ---------------- MP5：gen 的 WAV 逐 byte 相同；spectrogram 數值在容差內 ----------------
test_mp5() {
    section "MP5 spectrogram"; build mp5 || return
    while read -r fs w win dft hop; do
        [ -z "$fs" ] && continue; n="${fs}_${win}_w${w}_d${dft}_h${hop}"; o="$WORK/mp5_$n"
        "$PY" "$REF/mini_project_5/spectrogram.py" gen $fs "$o.ref.wav"
        if ! "$BIN" gen $fs "$o.out.wav" > "$o.log" 2>&1; then bad "${n}：gen 執行失敗"; continue; fi
        cmp -s "$o.out.wav" "$o.ref.wav" || { bad "${n}：gen 的 WAV 與參考不同"; continue; }
        "$PY" "$REF/mini_project_5/spectrogram.py" spec $w $win $dft $hop "$o.ref.wav" "$o.ref.txt"
        if ! "$BIN" spec $w $win $dft $hop "$o.ref.wav" "$o.out.txt" >> "$o.log" 2>&1; then bad "${n}：spec 執行失敗"; continue; fi
        if "$PY" "$CI_DIR/numcmp.py" "$o.out.txt" "$o.ref.txt" > /dev/null 2>"$o.err"; then ok "$n"; else bad "${n}：$(cat "$o.err")"; fi
    done < <(grep -v '^#' "$TESTS_DIR/mp5_cases.txt")
}

# ---------------- 主流程 ----------------
echo "course: $COURSE_DIR"; echo "tests : $TESTS_DIR"; echo "repo  : $ROOT"
targets=("$@"); [ ${#targets[@]} -eq 0 ] && targets=(mp1 mp2 mp3 mp4 mp5)
for t in "${targets[@]}"; do
    case "$t" in mp1|mp2|mp3|mp4|mp5) "test_$t" ;; *) echo "未知目標 ${t}（只接受 mp1..mp5）"; FAIL=$((FAIL+1)) ;; esac
done
echo; echo "================ 結果：通過 ${PASS}、失敗 ${FAIL}、略過 $SKIP ================"
echo "所有輸出與參考檔在 .ci_out/，比對不過時打開來看差在哪。"
[ "$FAIL" -eq 0 ]
