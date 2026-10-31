#!/usr/bin/env python3
"""run_mock_tests.py：mock 模式的自動化驗收（README「最低驗收」第 1–7 項）。只用標準函式庫。

  python run_mock_tests.py "<send_cmd>" "<recv_cmd>" <in.wav> [--port N] [--keep]

  <send_cmd>／<recv_cmd> 是「程式名稱＋子命令」，後面的參數由本腳本接上：
    Python 對 Python：  python run_mock_tests.py "python ../python_ref/voicelink.py send" "python ../python_ref/voicelink.py recv" in.wav
    C 版對 C 版：        python run_mock_tests.py "../build/voicelink send" "../build/voicelink recv" in.wav
    混搭（評測就是這樣）："python ../python_ref/voicelink.py send" 配你們的 recv，反過來再跑一次
  --port N   recv 聽 N、udp_impair.py 聽 N+1（預設 5004）；--keep 留下輸出的 WAV 不刪

  七個測試（每個都印 [PASS]／[FAIL]，最後印 summary；有任何 FAIL 結束碼 1）：
    1 clean      沒有搗蛋：PCM 逐 byte 相同，lost=0 concealed=0
    2 loss       --loss 0.05：輸出 frame 數與輸入相同、concealed ≈ lost（±2）、正常結束
    3 reorder    --reorder 0.1 --jitter 20、recv --buffer 100：PCM 逐 byte 相同
    4 dup        --dup 0.1：PCM 逐 byte 相同、dup > 0
    5 kill       送端中途被殺：recv 在 --timeout 內自己結束、WAV 可讀、STATS 有印
    6 garbage    先對 recv 的 port 丟 100 個隨機 bytes，再正常送：recv 不當掉、PCM 逐 byte相同
    7 stats      四種搗蛋全開：STATS 欄位順序正確、frames_out = received − late − dup − overflow + concealed
  udp_impair.py 由本腳本自己啟動（在 ../baseline/），--seed 固定，所以每次結果一樣。
  「逐 byte 相同」容許輸出比輸入多最後一包補的 0（長度為 640 的倍數、多出來的全是 0）。
"""
import argparse
import os
import random
import re
import shlex
import socket
import struct
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
IMPAIR = os.path.join(HERE, "..", "baseline", "udp_impair.py")
FRAME_BYTES = 640
STATS_RE = re.compile(r"STATS role=recv buffer_ms=(\d+) plc=(zero|repeat) frames_out=(\d+) received=(\d+) lost=(\d+) late=(\d+) "
                      r"dup=(\d+) reordered=(\d+) overflow=(\d+) concealed=(\d+) avg_delay_ms=([\d.]+) max_delay_ms=([\d.]+)")
STATS_KEYS = ["buffer_ms", "plc", "frames_out", "received", "lost", "late", "dup", "reordered", "overflow", "concealed", "avg_delay_ms", "max_delay_ms"]


# ------------------------------------------------------------ 小工具
def split_cmd(cmd):
    """把 "python ../x/voicelink.py send" 拆成 list。Windows 的反斜線路徑先換成正斜線，shlex 才不會把它吃掉。"""
    if os.name == "nt":
        cmd = cmd.replace("\\", "/")
    return shlex.split(cmd)


def read_pcm(path):
    """照 chunk 走的 WAV 讀取，回傳 data chunk 的 bytes（找不到或不是 WAV 回傳 None）。"""
    try:
        d = open(path, "rb").read()
    except OSError:
        return None
    if len(d) < 12 or d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        return None
    p = 12
    while p + 8 <= len(d):
        cid, size = d[p:p + 4], struct.unpack("<I", d[p + 4:p + 8])[0]
        if cid == b"data":
            return d[p + 8:p + 8 + size]
        p += 8 + size + (size & 1)
    return None


def same_pcm(src, out):
    """out 的前面 = src（逐 byte）；多出來的尾巴只能是最後一包補的 0。回傳 (bool, 說明)。"""
    if out is None:
        return False, "輸出不是 WAV 或讀不到"
    if len(out) < len(src):
        return False, f"輸出比輸入短：{len(out)} < {len(src)} bytes"
    if out[:len(src)] != src:
        i = next(i for i in range(len(src)) if src[i] != out[i])
        return False, f"第 {i} byte（sample {i // 2}，frame {i // FRAME_BYTES}）開始不同"
    tail = out[len(src):]
    if len(tail) >= FRAME_BYTES or any(tail):
        return False, f"尾巴多了 {len(tail)} bytes 而且不全是 0"
    return True, f"{len(src)} bytes 逐 byte 相同" + (f"（尾巴多 {len(tail)} 個補 0 的 byte）" if tail else "")


def n_frames(nbytes):
    return (nbytes + FRAME_BYTES - 1) // FRAME_BYTES


def parse_stats(stderr_text):
    m = STATS_RE.search(stderr_text)
    if not m:
        return None
    v = dict(zip(STATS_KEYS, m.groups()))
    for k in STATS_KEYS:
        if k not in ("plc", "avg_delay_ms", "max_delay_ms"):
            v[k] = int(v[k])
    v["avg_delay_ms"], v["max_delay_ms"] = float(v["avg_delay_ms"]), float(v["max_delay_ms"])
    return v


def popen(argv, cwd=None):
    """啟動子程序，stdout／stderr 都收起來（UTF-8，壞字元不會讓測試炸掉）。"""
    return subprocess.Popen(argv, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8", errors="replace")


def wait(proc, timeout):
    """等子程序結束，回傳 (returncode 或 None=逾時, stdout, stderr)。逾時就殺掉。"""
    try:
        out, err = proc.communicate(timeout=timeout)
        return proc.returncode, out, err
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
        return None, out, err


# ------------------------------------------------------------ 一次完整的 send → (impair) → recv
class Run:
    """一次實驗的結果：recv 的結束碼、STATS、輸出 PCM、send 的結束碼。"""
    def __init__(self):
        self.recv_rc = self.send_rc = None
        self.recv_out = self.recv_err = self.send_err = ""
        self.stats = None
        self.pcm = None
        self.impair_err = ""


def run_once(cfg, out_wav, impair_opts=None, recv_opts=(), send_opts=(), before_send=None, kill_send_after=None, recv_timeout_ms=2000):
    """啟動 recv（與 udp_impair）、送、等 recv 結束。
    impair_opts     None = 直連；否則是 udp_impair.py 的參數 list（會自動加 --seed 1 --quiet）
    before_send     recv 開好之後、send 之前要做的事（callable，拿到 recv 的 port）
    kill_send_after 送了這麼多秒之後把 send 殺掉（測試 5）
    """
    r = Run()
    recv_port, impair_port = cfg.port, cfg.port + 1
    send_to = impair_port if impair_opts is not None else recv_port
    recv_argv = cfg.recv + [str(recv_port), out_wav, "--timeout", str(recv_timeout_ms)] + list(recv_opts)
    impair = None
    recv = popen(recv_argv)
    if impair_opts is not None:
        impair = popen([sys.executable, IMPAIR, str(impair_port), "127.0.0.1", str(recv_port), "--seed", "1", "--quiet"] + impair_opts)
    time.sleep(0.8)                                                 # 讓 recv 與 impair 把 socket 開好
    if before_send:
        before_send(recv_port)
    send = popen(cfg.send + ["127.0.0.1", str(send_to), cfg.wav] + list(send_opts))
    if kill_send_after is not None:
        time.sleep(kill_send_after)
        send.kill()
        r.send_rc, _, r.send_err = wait(send, 5)
    else:
        r.send_rc, _, r.send_err = wait(send, cfg.wav_sec + 30)
    # recv 的結束：BYE 到了會馬上結束；BYE 全掉或 send 被殺就靠 --timeout。多給 10 秒。
    r.recv_rc, r.recv_out, r.recv_err = wait(recv, recv_timeout_ms / 1000.0 + 10 + (cfg.wav_sec if kill_send_after is None else 0))
    if impair is not None:
        impair.terminate()
        _, _, r.impair_err = wait(impair, 5)
    r.stats = parse_stats(r.recv_err)
    r.pcm = read_pcm(out_wav)
    return r


# ------------------------------------------------------------ 七個測試。每個回傳 (ok, 一行說明)
def need(r, checks):
    """共同的前置檢查：recv 要正常結束（結束碼 0）而且有 STATS；之後再跑各測試自己的 checks（list of (bool, msg)）。"""
    if r.recv_rc is None:
        return False, "recv 沒有在時間內結束（逾時被殺）"
    if r.recv_rc != 0:
        return False, f"recv 結束碼 {r.recv_rc}；stderr 最後一行：{r.recv_err.strip().splitlines()[-1] if r.recv_err.strip() else '(空)'}"
    if r.stats is None:
        return False, f"stderr 裡沒有合法的 STATS 行；stderr：{r.recv_err.strip()[-200:]}"
    for ok, msg in checks:
        if not ok:
            return False, msg
    return True, "；".join(msg for _, msg in checks)


def stats_line(s):
    return f"frames_out={s['frames_out']} received={s['received']} lost={s['lost']} late={s['late']} dup={s['dup']} reordered={s['reordered']} overflow={s['overflow']} concealed={s['concealed']} avg_delay={s['avg_delay_ms']} ms"


def test_clean(cfg, out):
    r = run_once(cfg, out)
    if r.recv_rc is None or r.recv_rc != 0 or r.stats is None:
        return need(r, [])
    ok, why = same_pcm(cfg.src, r.pcm)
    s = r.stats
    return need(r, [(ok, why), (s["lost"] == 0 and s["concealed"] == 0, f"lost={s['lost']} concealed={s['concealed']}")])


def test_loss(cfg, out):
    r = run_once(cfg, out, impair_opts=["--loss", "0.05"])
    if r.recv_rc is None or r.recv_rc != 0 or r.stats is None:
        return need(r, [])
    s = r.stats
    want, got = n_frames(len(cfg.src)), n_frames(len(r.pcm)) if r.pcm is not None else -1
    return need(r, [(got == want, f"frame 數 {got}（輸入 {want}）"),
                    (abs(s["concealed"] - s["lost"]) <= 2, f"concealed={s['concealed']} lost={s['lost']}"),
                    (s["lost"] > 0, "lost > 0（搗蛋有生效）")])


def test_reorder(cfg, out):
    r = run_once(cfg, out, impair_opts=["--reorder", "0.1", "--jitter", "20"], recv_opts=["--buffer", "100"])
    if r.recv_rc is None or r.recv_rc != 0 or r.stats is None:
        return need(r, [])
    ok, why = same_pcm(cfg.src, r.pcm)
    s = r.stats
    return need(r, [(ok, why), (True, f"reordered={s['reordered']} late={s['late']} concealed={s['concealed']}")])


def test_dup(cfg, out):
    r = run_once(cfg, out, impair_opts=["--dup", "0.1"])
    if r.recv_rc is None or r.recv_rc != 0 or r.stats is None:
        return need(r, [])
    ok, why = same_pcm(cfg.src, r.pcm)
    s = r.stats
    return need(r, [(ok, why), (s["dup"] > 0, f"dup={s['dup']}")])


def test_kill(cfg, out):
    timeout_ms = 2000
    t0 = time.perf_counter()
    r = run_once(cfg, out, kill_send_after=1.5, recv_timeout_ms=timeout_ms)
    elapsed = time.perf_counter() - t0
    if r.recv_rc is None or r.recv_rc != 0 or r.stats is None:
        return need(r, [])
    s = r.stats
    # 送了 1.5 秒 ≈ 75 包；recv 應該在 1.5 s + timeout + 一點點之內結束（0.8 s 的開機等待也算進去）
    limit = 0.8 + 1.5 + timeout_ms / 1000.0 + 3.0
    frames = n_frames(len(r.pcm)) if r.pcm is not None else -1
    return need(r, [(elapsed < limit, f"recv 在 {elapsed:.1f} s 內結束（上限 {limit:.1f} s）"),
                    (r.pcm is not None and len(r.pcm) > 0, f"WAV 可讀，{frames} frames"),
                    (frames == s["frames_out"], f"frames_out={s['frames_out']} 與 WAV 一致"),
                    (r.pcm is not None and r.pcm == cfg.src[:len(r.pcm)], "收到的部分與輸入逐 byte 相同")])


def test_garbage(cfg, out):
    def throw_garbage(port):
        rng = random.Random(7)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        for _ in range(100):
            n = rng.choice([1, 5, 11, 12, 13, 100, 651, 652, 653, 1400])     # 各種長度，包括剛好 12／652 但 magic 不對的
            try:
                sock.sendto(bytes(rng.getrandbits(8) for _ in range(n)), ("127.0.0.1", port))
            except OSError:
                pass
        sock.close()
        time.sleep(0.3)
    r = run_once(cfg, out, before_send=throw_garbage)
    if r.recv_rc is None or r.recv_rc != 0 or r.stats is None:
        return need(r, [])
    ok, why = same_pcm(cfg.src, r.pcm)
    s = r.stats
    return need(r, [(ok, why), (s["received"] == n_frames(len(cfg.src)), f"received={s['received']}（壞封包沒有被算進去）")])


def test_stats(cfg, out):
    r = run_once(cfg, out, impair_opts=["--loss", "0.05", "--jitter", "30", "--reorder", "0.1", "--dup", "0.02"], recv_opts=["--buffer", "60"])
    if r.recv_rc is None or r.recv_rc != 0 or r.stats is None:
        return need(r, [])
    s = r.stats
    played = s["received"] - s["late"] - s["dup"] - s["overflow"]
    lines = [ln for ln in r.recv_err.splitlines() if ln.startswith("STATS ")]
    return need(r, [(len(lines) == 1, f"stderr 恰好一行 STATS"),
                    (s["buffer_ms"] == 60 and s["plc"] == "repeat", "buffer_ms／plc 與參數一致"),
                    (s["frames_out"] == played + s["concealed"], f"frames_out={s['frames_out']} = ({s['received']}−{s['late']}−{s['dup']}−{s['overflow']}) + {s['concealed']}"),
                    (s["frames_out"] == n_frames(len(cfg.src)), f"frames_out 等於輸入的 frame 數 {n_frames(len(cfg.src))}"),
                    (0 <= s["avg_delay_ms"] <= s["max_delay_ms"], f"avg_delay={s['avg_delay_ms']} ≤ max_delay={s['max_delay_ms']}"),
                    (True, stats_line(s))])


TESTS = [("1 clean", test_clean), ("2 loss", test_loss), ("3 reorder", test_reorder), ("4 dup", test_dup),
         ("5 kill", test_kill), ("6 garbage", test_garbage), ("7 stats", test_stats)]


# ------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description="mock 模式的自動化驗收（README 第 1–7 項）")
    ap.add_argument("send_cmd", help='例如 "python ../python_ref/voicelink.py send"')
    ap.add_argument("recv_cmd", help='例如 "python ../python_ref/voicelink.py recv"')
    ap.add_argument("wav", help="16 kHz／16-bit／單聲道的 WAV")
    ap.add_argument("--port", type=int, default=5004, help="recv 聽這個 port，udp_impair.py 聽 port+1（預設 5004）")
    ap.add_argument("--keep", action="store_true", help="留下輸出的 WAV")
    ap.add_argument("--only", help="只跑這幾個（逗號分隔的編號，例如 1,3,7）")
    cfg = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    cfg.send, cfg.recv = split_cmd(cfg.send_cmd), split_cmd(cfg.recv_cmd)
    # 子命令可以省略："python voicelink.py" 和 "python voicelink.py send" 都接受
    if not cfg.send or cfg.send[-1] != "send":
        cfg.send.append("send")
    if not cfg.recv or cfg.recv[-1] != "recv":
        cfg.recv.append("recv")
    cfg.wav = os.path.abspath(cfg.wav)
    cfg.src = read_pcm(cfg.wav)
    if cfg.src is None:
        print(f"錯誤: {cfg.wav} 不是 WAV 或找不到 data chunk", file=sys.stderr)
        return 2
    if not os.path.exists(IMPAIR):
        print(f"錯誤: 找不到 {IMPAIR}", file=sys.stderr)
        return 2
    cfg.wav_sec = len(cfg.src) / 2 / 16000
    only = set(cfg.only.split(",")) if cfg.only else None
    tmp = tempfile.mkdtemp(prefix="voicelink_tests_")
    print(f"run_mock_tests：send={cfg.send_cmd!r} recv={cfg.recv_cmd!r}")
    print(f"  wav={cfg.wav}（{len(cfg.src) // 2:,} samples，{n_frames(len(cfg.src))} frames，{cfg.wav_sec:.1f} s）port={cfg.port}/{cfg.port + 1} 輸出在 {tmp}", flush=True)
    results = []
    for name, fn in TESTS:
        if only and name.split()[0] not in only:
            continue
        out = os.path.join(tmp, f"test{name.split()[0]}.wav")
        t0 = time.perf_counter()
        try:
            ok, why = fn(cfg, out)
        except Exception as why_exc:                                 # 測試本身炸掉也算 FAIL，不要讓整個 suite 停
            ok, why = False, f"測試腳本例外：{why_exc!r}"
        results.append(ok)
        print(f"[{'PASS' if ok else 'FAIL'}] {name:10s} {why}  ({time.perf_counter() - t0:.1f} s)", flush=True)
    npass = sum(results)
    print(f"summary: {npass}/{len(results)} passed" + ("" if npass == len(results) else f"，{len(results) - npass} failed"))
    if not cfg.keep:
        for f in os.listdir(tmp):
            try:
                os.remove(os.path.join(tmp, f))
            except OSError:
                pass
        try:
            os.rmdir(tmp)
        except OSError:
            pass
    return 0 if npass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
