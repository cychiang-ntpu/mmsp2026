#!/usr/bin/env bash
# 把課程 repo 裡「要給同學編譯、執行的程式」全部建置並跑一遍：macOS／Linux／Windows（Git Bash 或 MSYS2）通用。
#   bash tools/ci/build_course_code.sh          （從 repo 根目錄執行）
# GitHub Actions 每次 push 會在三個平台各跑一次（.github/workflows/build-course-code.yml）；
# 同學的電腦編不過時，也可以自己跑這支腳本，把最後幾行輸出貼到 Slack。
set -u
cd "$(dirname "$0")/../.."
ROOT=$(pwd)
export PYTHONIOENCODING=utf-8

MAKE=make; command -v make >/dev/null 2>&1 || MAKE=mingw32-make
# 要「真的跑得起來」才算：Windows 上的 python3 常常只是微軟商店的空殼，指令存在但不能執行
PY=""; for c in python3 python; do if "$c" -c "import sys" >/dev/null 2>&1; then PY=$c; break; fi; done
[ -n "$PY" ] || { echo "找不到可以執行的 Python 3"; exit 1; }
EXE=""; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=".exe" ;; esac
FAIL=0
ok()  { echo "  [PASS] $1"; }
bad() { echo "  [FAIL] $1"; FAIL=$((FAIL + 1)); }
run() { local name=$1; shift; if "$@" > "$ROOT/.ci_out/last.log" 2>&1; then ok "$name"; else bad "$name"; tail -n 25 "$ROOT/.ci_out/last.log" | sed 's/^/      /'; fi; }
nowarn() { if grep -Eiq "warning|error" "$ROOT/.ci_out/last.log"; then bad "$1：編譯有 warning"; grep -Ei "warning|error" "$ROOT/.ci_out/last.log" | head -n 10 | sed 's/^/      /'; else ok "$1：零警告"; fi; }

mkdir -p "$ROOT/.ci_out"
echo "平台：$(uname -s) $(uname -m)；$($MAKE --version | head -n 1)；$(gcc --version | head -n 1)；$($PY --version 2>&1)"

echo "== 第 2 週範例 =="
D=lectures/wk02_0914_text-utf8/examples
run "make" $MAKE -B -C $D all; nowarn "wk02 examples"
run "make check（C 與 Python 的 utf8_dump 輸出相同）" $MAKE -C $D check

echo "== 第 3 週範例 =="
D=lectures/wk03_0921_team1-kickoff/examples
run "make" $MAKE -B -C $D all; nowarn "wk03 examples"
run "make check（C 與 Python 的 entropy 輸出相同）" $MAKE -C $D check
run "huffman_trace：8 色影像" $MAKE -C $D trace
if grep -q "536 ÷ 256 = 2.0938" "$ROOT/.ci_out/last.log"; then ok "huffman_trace 的平均碼長 = 2.0938"; else bad "huffman_trace 的數字不對"; fi
run "huffman_trace：中文參數" $D/huffman_trace$EXE "多媒體多媒多"
if grep -q "9 ÷ 6 = 1.5000" "$ROOT/.ci_out/last.log"; then ok "huffman_trace 中文參數的結果正確"; else bad "huffman_trace 中文參數的結果不對"; fi
( cd $D && for s in huffman_demo.py alias_demo.py quantize_demo.py; do $PY $s > /dev/null 2>"$ROOT/.ci_out/py.log" || { echo "$s"; cat "$ROOT/.ci_out/py.log"; exit 1; }; done ) > "$ROOT/.ci_out/last.log" 2>&1 \
  && ok "Python 示範（huffman_demo、alias_demo、quantize_demo）" || { bad "Python 示範"; cat "$ROOT/.ci_out/last.log"; }
run "wav_info.py 讀真實的 WAV" $PY $D/wav_info.py lectures/wk03_0921_team1-kickoff/data/speech_osr_8k.wav
run "make_samples_js.py 可重現 samples.js" $PY $D/make_samples_js.py
if git diff --quiet -- lectures/wk03_0921_team1-kickoff/slides/samples.js 2>/dev/null; then ok "samples.js 與 data/ 裡的檔案一致"; else bad "samples.js 與 data/ 不一致：請重跑 make_samples_js.py 後 commit"; fi

echo "== Team 1 baseline（chat.c）=="
run "make" $MAKE -B -C team_projects/team1_textlink/baseline; nowarn "baseline chat.c"

echo "== Team 1 starter =="
D=team_projects/team1_textlink/starter
run "make" $MAKE -B -C $D; nowarn "starter"
$MAKE -C $D test > "$ROOT/.ci_out/last.log" 2>&1
if grep -q "FAIL 0、TODO 46" "$ROOT/.ci_out/last.log"; then ok "make test：46 個 TODO、0 個 FAIL（place holder 還沒寫，這是預期的）"; else bad "make test 的結果不是預期的 46 TODO"; tail -n 8 "$ROOT/.ci_out/last.log"; fi
$D/textlink$EXE > "$ROOT/.ci_out/last.log" 2>&1; [ $? -eq 2 ] && ok "textlink 不帶參數：印用法、結束碼 2" || bad "textlink 不帶參數的結束碼不是 2"
$D/textlink$EXE send 127.0.0.1 99999 x > "$ROOT/.ci_out/last.log" 2>&1; [ $? -eq 2 ] && ok "port 超出範圍：結束碼 2" || bad "port 檢查"
$D/textlink$EXE send 127.0.0.1 6553 "$D/Makefile" --raw > "$ROOT/.ci_out/last.log" 2>&1; [ $? -eq 1 ] && ok "連不上對方：結束碼 1、不卡住（非阻塞 connect＋逾時）" || { bad "連線失敗的處理"; cat "$ROOT/.ci_out/last.log"; }

echo "== Team 1 python_ref（純 Python 完整版）=="
R=team_projects/team1_textlink/python_ref/textlink.py
W=lectures/wk03_0921_team1-kickoff/data/speech_osr_8k.wav
run "inspect：真實語音，兩種符號都能逐 byte 還原" $PY $R inspect $W
if [ "$(grep -c "逐 byte 相同" "$ROOT/.ci_out/last.log")" -eq 2 ] && grep -q "12,343" "$ROOT/.ci_out/last.log"; then ok "inspect 的數字正確（K = 12,343）"; else bad "inspect 的輸出不對"; cat "$ROOT/.ci_out/last.log"; fi
rm -rf "$ROOT/.ci_out/rx"
$PY $R recv 6622 "$ROOT/.ci_out/rx" > "$ROOT/.ci_out/rx.log" 2>&1 &
RX=$!
sleep 2
run "send → recv（本機 TCP，Huffman，符號＝s16）" $PY $R send 127.0.0.1 6622 $W --huff
wait $RX && cmp -s $W "$ROOT/.ci_out/rx/speech_osr_8k.wav" && ok "收到的檔案逐 byte 相同" || { bad "python_ref 傳檔失敗"; cat "$ROOT/.ci_out/rx.log"; }

echo "== socket 冒煙測試：C 的 sticky_send → Python 的接收端（本機 127.0.0.1）=="
PORT=6611
$PY - "$PORT" > "$ROOT/.ci_out/server.log" 2>&1 <<'PYEOF' &
import socket, sys
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(("127.0.0.1", int(sys.argv[1]))); s.listen(1); s.settimeout(20)
c, _ = s.accept(); c.settimeout(10); got = b""
while True:
    d = c.recv(4096)
    if not d: break
    got += d
print(len(got), got.decode("utf-8", "replace"))
PYEOF
SERVER=$!
sleep 2
run "sticky_send 連線並送出 5 則" lectures/wk02_0914_text-utf8/examples/sticky_send$EXE 127.0.0.1 $PORT 5 0
wait $SERVER
if grep -q "第5則" "$ROOT/.ci_out/server.log"; then ok "接收端收到 5 則中文訊息（$(cut -d' ' -f1 "$ROOT/.ci_out/server.log") bytes）"; else bad "接收端沒有收到完整的訊息"; cat "$ROOT/.ci_out/server.log"; fi

echo
if [ $FAIL -eq 0 ]; then echo "全部通過"; else echo "有 $FAIL 項失敗"; fi
exit $FAIL
