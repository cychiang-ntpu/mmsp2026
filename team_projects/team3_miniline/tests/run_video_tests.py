#!/usr/bin/env python3
"""run_video_tests.py：mock 模式的自動化驗收（README「最低驗收」第 1–7 項）。只用標準函式庫。

  python run_video_tests.py "<send_cmd>" "<recv_cmd>" <jpegdir> [--port N] [--keep] [--only 1,3]

  <send_cmd>／<recv_cmd> 是「程式名稱（＋子命令）」，後面的參數由本腳本接上；子命令沒寫就自動補 video-send／video-recv：
    Python 對 Python：  python run_video_tests.py "python ../python_ref/miniline.py" "python ../python_ref/miniline.py" seq/
    C 版對 C 版：        python run_video_tests.py "../build/miniline" "../build/miniline" seq/
    混搭（評測就是這樣）："python ../python_ref/miniline.py video-send" 配你們的 recv，反過來再跑一次
  <jpegdir>  0000.jpg、0001.jpg…（baseline 的 make_sequence 產生的真正影像，或 make_fake_sequence.py 的假 JPEG——測試只比 bytes）
  --port N   recv 聽 N、udp_impair.py 聽 N+1（預設 5500）；--keep 留下輸出的資料夾不刪

  七個測試（每個都印 [PASS]／[FAIL]，最後印 summary；有任何 FAIL 結束碼 1）：
    1 clean      沒有搗蛋：每張逐 byte 相同、張數相同、frames_dropped_timeout=0
    2 loss       --loss 0.03：寫出的每個檔都是完整合法的 JPEG 且與原檔相同、frames_dropped_timeout ≥ 1、正常結束
    3 reorder    --jitter 30 --reorder 0.1：每張逐 byte 相同（片的順序被排回來）
    4 burst      --burst：frames_dropped_queue > 0、frames_shown + frames_dropped_queue = frames_complete、recv 很快結束、記憶體 < 200 MB
    5 kill       送端 1 秒後被殺：recv 在 --idle + 2 s 內自己結束、STATS 有印、寫出的檔都完整
    6 garbage    先對 recv 的 port 丟 100 個隨機 bytes 與 20 個 piece_idx ≥ piece_cnt 的假片，再正常送：不當掉、逐 byte 相同、pieces_bad ≥ 20
    7 reconnect  同一個 recv 連收兩段（第二段是另一串序列、新 ssrc、重新 HELLO）：shown.txt 的 frame_id 從 0 重算、最後的檔案 = 第二段
  udp_impair.py 由本腳本自己啟動（../baseline/udp_impair.py，轉呼叫 Team 2 的），--seed 固定，所以每次結果一樣。
"""
import argparse
import os
import random
import re
import shlex
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
IMPAIR = os.path.join(HERE, "..", "baseline", "udp_impair.py")
IMPAIR_TEAM2 = os.path.join(HERE, "..", "..", "team2_voicelink", "baseline", "udp_impair.py")
FRAME_SEC = 0.2
STATS_RE = re.compile(r"STATS role=video-recv queue=(\d+) timeout_ms=(\d+) frames_shown=(\d+) frames_complete=(\d+) frames_dropped_timeout=(\d+) "
                      r"frames_dropped_queue=(\d+) pieces_received=(\d+) pieces_lost=(-?\d+) pieces_bad=(\d+) fps_actual=([\d.]+) "
                      r"avg_assemble_ms=([\d.]+) max_assemble_ms=([\d.]+) display_idle=(\d+)")
STATS_KEYS = ["queue", "timeout_ms", "frames_shown", "frames_complete", "frames_dropped_timeout", "frames_dropped_queue",
              "pieces_received", "pieces_lost", "pieces_bad", "fps_actual", "avg_assemble_ms", "max_assemble_ms", "display_idle"]
FLOAT_KEYS = ("fps_actual", "avg_assemble_ms", "max_assemble_ms")
JPEG_NAME = re.compile(r"^(\d{4})\.jpg$", re.IGNORECASE)
RSS_LIMIT_MB = 200


# ------------------------------------------------------------ 小工具
def split_cmd(cmd):
    """把 "python ../x/miniline.py video-send" 拆成 list。Windows 的反斜線路徑先換成正斜線，shlex 才不會把它吃掉。"""
    if os.name == "nt":
        cmd = cmd.replace("\\", "/")
    return shlex.split(cmd)


def read_sequence(d):
    """{frame_id: bytes}，照 NNNN.jpg 讀。"""
    seq = {}
    if not os.path.isdir(d):
        return seq
    for n in os.listdir(d):
        m = JPEG_NAME.match(n)
        if m:
            with open(os.path.join(d, n), "rb") as f:
                seq[int(m.group(1))] = f.read()
    return seq


def read_shown(outdir):
    """shown.txt → [(frame_id, t_ms), …]；讀不到回傳 None。"""
    try:
        with open(os.path.join(outdir, "shown.txt"), encoding="utf-8", errors="replace") as f:
            lines = [ln.strip() for ln in f if ln.strip()]
    except OSError:
        return None
    out = []
    for ln in lines:
        try:
            a, b = ln.split(",")[:2]
            out.append((int(a), float(b)))
        except ValueError:
            return None
    return out


def valid_jpeg(data):
    return len(data) >= 4 and data[:2] == b"\xff\xd8" and data[-2:] == b"\xff\xd9"


def same_files(src, got, want_all=True):
    """got 的每一張都要和 src 同 id 的那張逐 byte 相同；want_all 時張數也要一樣。回傳 (bool, 說明)。"""
    if not got:
        return False, "一張都沒寫出來"
    for fid, data in sorted(got.items()):
        if fid not in src:
            return False, f"{fid:04d}.jpg 不在輸入裡"
        if data != src[fid]:
            if not valid_jpeg(data):
                return False, f"{fid:04d}.jpg 不是完整的 JPEG（{len(data)} bytes，原檔 {len(src[fid])}）"
            i = next((i for i in range(min(len(data), len(src[fid]))) if data[i] != src[fid][i]), min(len(data), len(src[fid])))
            return False, f"{fid:04d}.jpg 第 {i} byte 開始不同（{len(data)} bytes，原檔 {len(src[fid])}）"
    if want_all and len(got) != len(src):
        return False, f"張數 {len(got)}（輸入 {len(src)}）"
    return True, f"{len(got)} 張逐 byte 相同" + ("" if want_all else f"（輸入 {len(src)} 張）")


def parse_stats(text):
    m = STATS_RE.search(text)
    if not m:
        return None
    v = dict(zip(STATS_KEYS, m.groups()))
    for k in STATS_KEYS:
        v[k] = float(v[k]) if k in FLOAT_KEYS else int(v[k])
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


def sample_rss_mb(pid):
    """這個 pid 現在的 RSS（MB）；量不到回傳 None。Linux 讀 /proc，Windows 用 tasklist，其他用 ps。只用標準函式庫。"""
    try:
        if os.path.exists(f"/proc/{pid}/status"):
            with open(f"/proc/{pid}/status") as f:
                for ln in f:
                    if ln.startswith("VmRSS:"):
                        return int(ln.split()[1]) / 1024
            return None
        if os.name == "nt":
            out = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"], capture_output=True, text=True, timeout=5).stdout
            m = re.search(r'"([\d,.]+) K"', out)
            return int(re.sub(r"[^\d]", "", m.group(1))) / 1024 if m else None
        out = subprocess.run(["ps", "-o", "rss=", "-p", str(pid)], capture_output=True, text=True, timeout=5).stdout
        return int(out.strip()) / 1024 if out.strip() else None
    except (OSError, ValueError, subprocess.SubprocessError):
        return None


class RssWatcher:
    """背景執行緒每 0.2 s 量一次 recv 的 RSS，記最大值。"""
    def __init__(self, proc):
        self.proc, self.max_mb, self.samples, self.stop = proc, None, 0, False
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        while not self.stop and self.proc.poll() is None:
            mb = sample_rss_mb(self.proc.pid)
            if mb is not None:
                self.samples += 1
                self.max_mb = mb if self.max_mb is None else max(self.max_mb, mb)
            time.sleep(0.2)

    def finish(self):
        self.stop = True
        self.thread.join(timeout=2)
        return self.max_mb


# ------------------------------------------------------------ 一次完整的 send → (impair) → recv
class Run:
    """一次實驗的結果：recv 的結束碼、STATS、輸出的檔案、send 的結束碼、經過的時間。"""
    def __init__(self):
        self.recv_rc = self.send_rc = None
        self.recv_out = self.recv_err = self.send_err = self.impair_err = ""
        self.stats = self.files = self.shown = None
        self.elapsed = 0.0
        self.max_rss_mb = None


def run_once(cfg, outdir, impair_opts=None, recv_opts=(), send_opts=(), before_send=None, kill_send_after=None, idle_ms=3000,
             second_dir=None, watch_rss=False):
    """啟動 recv（與 udp_impair）、送、等 recv 結束。
    impair_opts     None = 直連；否則是 udp_impair.py 的參數 list（會自動加 --quiet；請自己給 --seed）
    before_send     recv 開好之後、send 之前要做的事（callable，拿到 recv 的 port）
    kill_send_after 送了這麼多秒之後把 send 殺掉（測試 5）
    second_dir      第一段送完之後馬上再送這個資料夾（新的 send 程序 = 新 ssrc、重新 HELLO）（測試 7）
    """
    r = Run()
    recv_port, impair_port = cfg.port, cfg.port + 1
    send_to = impair_port if impair_opts is not None else recv_port
    shutil.rmtree(outdir, ignore_errors=True)
    os.makedirs(outdir)
    recv_argv = cfg.recv + [str(recv_port), outdir, "--idle", str(idle_ms)] + list(recv_opts)
    impair = None
    t0 = time.perf_counter()
    recv = popen(recv_argv)
    watcher = RssWatcher(recv) if watch_rss else None
    if impair_opts is not None:
        impair = popen([sys.executable, cfg.impair, str(impair_port), "127.0.0.1", str(recv_port), "--quiet"] + impair_opts)
    time.sleep(0.8)                                                 # 讓 recv 與 impair 把 socket 開好
    if before_send:
        before_send(recv_port)
    send = popen(cfg.send + ["127.0.0.1", str(send_to), cfg.jpegdir] + list(send_opts))
    if kill_send_after is not None:
        time.sleep(kill_send_after)
        send.kill()
        r.send_rc, _, r.send_err = wait(send, 5)
    else:
        r.send_rc, _, r.send_err = wait(send, cfg.seq_sec + 30)
        if second_dir is not None:
            send2 = popen(cfg.send + ["127.0.0.1", str(send_to), second_dir] + list(send_opts))
            rc2, _, err2 = wait(send2, cfg.seq_sec + 30)
            r.send_err += err2
            r.send_rc = r.send_rc if rc2 == 0 else rc2
    # recv 的結束：BYE 到了會顯示完就結束；BYE 全掉或 send 被殺就靠 --idle。多給 10 秒。
    limit = idle_ms / 1000.0 + 10 + (cfg.seq_sec * (2 if second_dir else 1) if kill_send_after is None else 0)
    r.recv_rc, r.recv_out, r.recv_err = wait(recv, limit)
    r.elapsed = time.perf_counter() - t0
    if watcher:
        r.max_rss_mb = watcher.finish()
    if impair is not None:
        impair.terminate()
        _, _, r.impair_err = wait(impair, 5)
    r.stats = parse_stats(r.recv_err)
    r.files = read_sequence(outdir)
    r.shown = read_shown(outdir)
    return r


# ------------------------------------------------------------ 七個測試。每個回傳 (ok, 一行說明)
def need(r, checks):
    """共同的前置檢查：recv 要正常結束（結束碼 0）而且有 STATS；之後再跑各測試自己的 checks（list of (bool, msg)）。"""
    if r.recv_rc is None:
        return False, "recv 沒有在時間內結束（逾時被殺）"
    if r.recv_rc != 0:
        return False, f"recv 結束碼 {r.recv_rc}；stderr 最後一行：{r.recv_err.strip().splitlines()[-1] if r.recv_err.strip() else '(空)'}"
    if r.stats is None:
        return False, f"stderr 裡沒有合法的 STATS 行；stderr：{r.recv_err.strip()[-300:]}"
    for ok, msg in checks:
        if not ok:
            return False, msg
    return True, "；".join(msg for _, msg in checks)


def bad_start(r):
    return r.recv_rc is None or r.recv_rc != 0 or r.stats is None


def stats_line(s):
    return (f"shown={s['frames_shown']} complete={s['frames_complete']} drop_timeout={s['frames_dropped_timeout']} drop_queue={s['frames_dropped_queue']} "
            f"pieces={s['pieces_received']}/lost {s['pieces_lost']}/bad {s['pieces_bad']} fps={s['fps_actual']} assemble avg={s['avg_assemble_ms']} max={s['max_assemble_ms']} ms idle={s['display_idle']}")


def test_clean(cfg, out):
    r = run_once(cfg, out)
    if bad_start(r):
        return need(r, [])
    ok, why = same_files(cfg.src, r.files)
    s = r.stats
    return need(r, [(ok, why), (s["frames_dropped_timeout"] == 0, f"frames_dropped_timeout={s['frames_dropped_timeout']}"),
                    (s["frames_shown"] == len(cfg.src), f"frames_shown={s['frames_shown']}"), (True, stats_line(s))])


def test_loss(cfg, out):
    r = run_once(cfg, out, impair_opts=["--loss", "0.03", "--seed", "1"])
    if bad_start(r):
        return need(r, [])
    s = r.stats
    ok, why = same_files(cfg.src, r.files, want_all=False)
    return need(r, [(ok, why),
                    (all(valid_jpeg(d) for d in r.files.values()), "每個寫出的檔都是 FF D8 … FF D9"),
                    (s["frames_dropped_timeout"] >= 1, f"frames_dropped_timeout={s['frames_dropped_timeout']}（搗蛋有生效）"),
                    (s["pieces_lost"] > 0, f"pieces_lost={s['pieces_lost']}"), (True, stats_line(s))])


def test_reorder(cfg, out):
    r = run_once(cfg, out, impair_opts=["--jitter", "30", "--reorder", "0.1", "--seed", "2"])
    if bad_start(r):
        return need(r, [])
    ok, why = same_files(cfg.src, r.files)
    s = r.stats
    return need(r, [(ok, why), (s["frames_dropped_timeout"] == 0, f"frames_dropped_timeout={s['frames_dropped_timeout']}"), (True, stats_line(s))])


def test_burst(cfg, out):
    r = run_once(cfg, out, send_opts=["--burst"], watch_rss=True)
    if bad_start(r):
        return need(r, [])
    s = r.stats
    limit = 0.8 + 3.0 + 3 * FRAME_SEC * s["queue"] + 5.0           # 一口氣送完 → 表、queue 都只剩幾張；recv 應該在 idle(3 s) + 幾拍內結束
    ok, why = same_files(cfg.src, r.files, want_all=False)
    checks = [(s["frames_dropped_queue"] > 0, f"frames_dropped_queue={s['frames_dropped_queue']}"),
              (s["frames_shown"] + s["frames_dropped_queue"] == s["frames_complete"],
               f"frames_shown {s['frames_shown']} + frames_dropped_queue {s['frames_dropped_queue']} = frames_complete {s['frames_complete']}"),
              (ok, why), (r.elapsed < limit, f"recv 在 {r.elapsed:.1f} s 內結束（上限 {limit:.1f} s）")]
    if r.max_rss_mb is None:
        checks.append((True, "記憶體量不到（這個平台沒有 /proc、tasklist 或 ps），跳過 RSS 檢查"))
    else:
        checks.append((r.max_rss_mb < RSS_LIMIT_MB, f"recv 的 RSS 最大 {r.max_rss_mb:.0f} MB（上限 {RSS_LIMIT_MB}）"))
    checks.append((True, stats_line(s)))
    return need(r, checks)


def test_kill(cfg, out):
    idle_ms = 3000
    r = run_once(cfg, out, kill_send_after=1.0, idle_ms=idle_ms)
    if bad_start(r):
        return need(r, [])
    s = r.stats
    limit = 0.8 + 1.0 + idle_ms / 1000.0 + 2.0
    ok, why = same_files(cfg.src, r.files, want_all=False)
    return need(r, [(r.elapsed < limit, f"recv 在 {r.elapsed:.1f} s 內結束（上限 {limit:.1f} s）"),
                    (ok, why), (len(r.files) > 0 and len(r.files) == s["frames_shown"], f"frames_shown={s['frames_shown']} 與檔案數一致"),
                    (r.shown is not None and len(r.shown) == s["frames_shown"], "shown.txt 的行數與 frames_shown 一致"), (True, stats_line(s))])


def test_garbage(cfg, out):
    def throw_garbage(port):
        rng = random.Random(7)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        for _ in range(100):                                         # 各種長度的隨機 bytes，包括剛好 16 與 1216
            n = rng.choice([1, 5, 15, 16, 17, 100, 1215, 1216, 1217, 1400, 65000])
            try:
                sock.sendto(bytes(rng.getrandbits(8) for _ in range(n)), ("127.0.0.1", port))
            except OSError:
                pass
        ssrc = rng.getrandbits(32)
        for i in range(20):                                          # 假片：header 看起來合法，但 piece_idx ≥ piece_cnt
            cnt = rng.randint(1, 20)
            hdr = struct.pack(">BBHIHHI", 0x4D, 0x01, i, rng.randint(0, 5), cnt + rng.randint(0, 3), cnt, ssrc)
            try:
                sock.sendto(hdr + bytes(rng.randint(1, 1200)), ("127.0.0.1", port))
            except OSError:
                pass
        sock.close()
        time.sleep(0.3)
    r = run_once(cfg, out, before_send=throw_garbage)
    if bad_start(r):
        return need(r, [])
    ok, why = same_files(cfg.src, r.files)
    s = r.stats
    return need(r, [(ok, why), (s["pieces_bad"] >= 20, f"pieces_bad={s['pieces_bad']}（≥ 20 個假片）"),
                    (s["frames_dropped_timeout"] == 0, f"frames_dropped_timeout={s['frames_dropped_timeout']}"), (True, stats_line(s))])


def test_reconnect(cfg, out):
    # 第二段：同一串序列倒過來（0000 = 原本最後一張），這樣「最後的檔案 = 第二段」才有東西可以驗
    second = os.path.join(os.path.dirname(out), "seq_reversed")
    shutil.rmtree(second, ignore_errors=True)
    os.makedirs(second)
    n = len(cfg.src)
    rev = {i: cfg.src[n - 1 - i] for i in range(n)}
    for i, data in rev.items():
        with open(os.path.join(second, f"{i:04d}.jpg"), "wb") as f:
            f.write(data)
    r = run_once(cfg, out, second_dir=second, idle_ms=10000)
    if bad_start(r):
        return need(r, [])
    s = r.stats
    ok, why = same_files(rev, r.files)
    ids = [fid for fid, _ in r.shown] if r.shown else []
    restarts = sum(1 for a, b in zip(ids, ids[1:]) if b < a)
    first_seg = ids[:ids.index(0, 1)] if ids.count(0) >= 2 else ids
    return need(r, [(ok, "最後的檔案 = 第二段：" + why),
                    (ids.count(0) == 2 and restarts == 1, f"shown.txt 的 frame_id 從 0 重算一次（{len(ids)} 行，0 出現 {ids.count(0)} 次）"),
                    (first_seg == list(range(n)), f"第一段顯示了 {len(first_seg)} 張（0…{n - 1}）"),
                    (s["frames_shown"] == 2 * n and s["frames_dropped_timeout"] == 0, f"frames_shown={s['frames_shown']}（2 × {n}）frames_dropped_timeout={s['frames_dropped_timeout']}"),
                    (True, stats_line(s))])


TESTS = [("1 clean", test_clean), ("2 loss", test_loss), ("3 reorder", test_reorder), ("4 burst", test_burst),
         ("5 kill", test_kill), ("6 garbage", test_garbage), ("7 reconnect", test_reconnect)]


# ------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description="mock 模式的自動化驗收（README 第 1–7 項）")
    ap.add_argument("send_cmd", help='例如 "python ../python_ref/miniline.py video-send"（子命令可省略）')
    ap.add_argument("recv_cmd", help='例如 "python ../python_ref/miniline.py video-recv"（子命令可省略）')
    ap.add_argument("jpegdir", help="0000.jpg、0001.jpg… 的資料夾")
    ap.add_argument("--port", type=int, default=5500, help="recv 聽這個 port，udp_impair.py 聽 port+1（預設 5500）")
    ap.add_argument("--keep", action="store_true", help="留下輸出的資料夾")
    ap.add_argument("--only", help="只跑這幾個（逗號分隔的編號，例如 1,3,7）")
    cfg = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    cfg.send, cfg.recv = split_cmd(cfg.send_cmd), split_cmd(cfg.recv_cmd)
    # 子命令可以省略："python miniline.py" 和 "python miniline.py video-send" 都接受
    if not cfg.send or cfg.send[-1] != "video-send":
        cfg.send.append("video-send")
    if not cfg.recv or cfg.recv[-1] != "video-recv":
        cfg.recv.append("video-recv")
    cfg.jpegdir = os.path.abspath(cfg.jpegdir)
    cfg.src = read_sequence(cfg.jpegdir)
    if not cfg.src or sorted(cfg.src) != list(range(len(cfg.src))):
        print(f"錯誤: {cfg.jpegdir} 裡沒有從 0000.jpg 開始連號的 JPEG", file=sys.stderr)
        return 2
    cfg.impair = IMPAIR if os.path.exists(IMPAIR) else IMPAIR_TEAM2
    if not os.path.exists(cfg.impair):
        print(f"錯誤: 找不到 {IMPAIR}（也找不到 {IMPAIR_TEAM2}）", file=sys.stderr)
        return 2
    cfg.seq_sec = len(cfg.src) * FRAME_SEC
    only = set(cfg.only.split(",")) if cfg.only else None
    tmp = tempfile.mkdtemp(prefix="miniline_tests_")
    total = sum(len(d) for d in cfg.src.values())
    print(f"run_video_tests：send={cfg.send_cmd!r} recv={cfg.recv_cmd!r}")
    print(f"  jpegdir={cfg.jpegdir}（{len(cfg.src)} 張，{total:,} bytes，{cfg.seq_sec:.1f} s @ 5 fps）port={cfg.port}/{cfg.port + 1} 輸出在 {tmp}", flush=True)
    results = []
    for name, fn in TESTS:
        if only and name.split()[0] not in only:
            continue
        out = os.path.join(tmp, f"test{name.split()[0]}")
        t0 = time.perf_counter()
        try:
            ok, why = fn(cfg, out)
        except Exception as why_exc:                                 # 測試本身炸掉也算 FAIL，不要讓整個 suite 停
            ok, why = False, f"測試腳本例外：{why_exc!r}"
        results.append(ok)
        print(f"[{'PASS' if ok else 'FAIL'}] {name:12s} {why}  ({time.perf_counter() - t0:.1f} s)", flush=True)
    npass = sum(results)
    print(f"summary: {npass}/{len(results)} passed" + ("" if npass == len(results) else f"，{len(results) - npass} failed"))
    if not cfg.keep:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0 if npass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
