#!/usr/bin/env python3
"""stft_demo.py — STFT／spectrogram：framing → 加窗 → zero padding → DFT（自己寫的 FFT）。只用標準函式庫。

用法：python3 stft_demo.py <wav> [--win hamming|rectangular] [--w_ms 32] [--dft_ms 32] [--hop_ms 10] [--ascii]
      python3 stft_demo.py --selftest        只檢查 FFT 和「照定義算的 DFT」答案一樣不一樣，不讀檔

輸出 <wav 檔名去掉 .wav>.spec.txt：一列一個 frame、每列 N/2 個 |X[k]|（線性值、空白分隔、小數 6 位），
格式和 samples_2025-python/mini_project_5/spectrogram.py（numpy 版）完全相同，MP5 的 C 程式也要寫成這樣。
加 --ascii 會在螢幕上印一張粗略的 ASCII spectrogram（時間往下、頻率往右、9 個灰階）。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：1.x STFT（frame、window、zero padding、時間與頻率解析度的取捨、FFT）。
# 怎麼跑：  python3 stft_demo.py ../data/aeueo-16kHz.wav --ascii            （Windows 打 python）
#           python3 stft_demo.py ../data/aeueo-16kHz.wav --w_ms 8 --dft_ms 8 --ascii    短窗：時間清楚、頻率模糊
#           python3 stft_demo.py ../data/aeueo-16kHz.wav --w_ms 32 --dft_ms 64 --ascii  zero padding：頻率軸變密，但沒有新資訊
# 會看到：  (1) P、N、M 三個參數換算成 sample 數、頻率解析度 fs/N、frame 數、DFT 與 FFT 的乘法次數；
#           (2) 寫出 aeueo-16kHz.spec.txt；(3) --ascii 時印出 spectrogram，五個母音的 formant（橫向的深色帶）位置不同。
# 注意：    N 不是 2 的次方時（例如 --dft_ms 30 → N = 480）會退回 O(N²) 的直接 DFT，7 秒的檔要跑好幾分鐘。
# ──────────────────────────────────────────────────────────────────────────────────────
import argparse
import cmath
import math
import os
import random
import struct
import sys
import time

# Windows 上把輸出導到檔案或管線時，Python 會改用 cp950 編碼，印不出 −、²、≤ 這些符號而當掉；這裡強制用 UTF-8。
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

GREY = " .:-=+*#@"        # ASCII spectrogram 的 9 個灰階：由弱到強
DB_RANGE = 48.0           # 灰階涵蓋的動態範圍：最大值往下 48 dB（9 階 → 每一階 6 dB）


def read_wav(path):
    """讀 16-bit、單聲道的 PCM WAV，回傳 (fs, sample 的 list)。照 RIFF chunk 走（逐行說明見 frame_demo.py）。"""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        sys.exit(f"{path} 不是 WAV 檔")
    pos, fmt, pcm = 12, None, None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    if fmt is None or pcm is None:
        sys.exit(f"{path}：找不到 fmt 或 data chunk")
    tag, channels, fs, _, _, bits = fmt
    if tag != 1 or channels != 1 or bits != 16:
        sys.exit(f"{path}：只接受 16-bit 單聲道 PCM（這個檔是 tag={tag} ch={channels} bits={bits}）")
    return fs, list(struct.unpack(f"<{len(pcm) // 2}h", pcm))


def dft(x):
    """照定義算 DFT：X[k] = Σ_n x[n]·e^(−j2πkn/N)，N 個 k 各要 N 次複數乘法，共 N² 次。"""
    # e^(−j2πkn/N) 叫 twiddle factor：單位圓上「第 kn 格」的點（把圓分成 N 格）。kn 超過 N 會繞回來，所以 kn % N。
    N = len(x)
    tw = [cmath.exp(-2j * math.pi * i / N) for i in range(N)]   # 先把 N 個點算好，之後只查表
    return [sum(x[n] * tw[(k * n) % N] for n in range(N)) for k in range(N)]


_TWIDDLE = {}     # 每個長度 N 的 twiddle 表只算一次（dict 當快取），700 個 frame 才不會重算 700 次 exp()


def fft(x):
    """radix-2 FFT（遞迴版）：長度 N 必須是 2 的次方。答案和 dft(x) 相同，但只要 (N/2)·log2(N) 次複數乘法。

    想法（Cooley–Tukey）：把 N 點 DFT 的和拆成「偶數位置的 n」和「奇數位置的 n」兩半，
        X[k] = Σ_{n 偶} x[n]·W^(kn) + Σ_{n 奇} x[n]·W^(kn)   （W = e^(−j2π/N)）
             = E[k] + W^k · O[k]
    其中 E、O 分別是 x[0], x[2], x[4], … 與 x[1], x[3], … 這兩個「N/2 點」訊號的 DFT。
    而且 E、O 都是週期 N/2 的，所以 k 與 k + N/2 共用同一組 E[k]、O[k]：
        X[k]       = E[k] + W^k·O[k]
        X[k + N/2] = E[k] − W^k·O[k]           （W^(k+N/2) = −W^k）
    這一對式子叫 butterfly：算一次乘法 W^k·O[k]，同時得到兩個輸出。
    每一層把問題切半、每層只做 N/2 次乘法，切 log2(N) 層就到長度 1（長度 1 的 DFT 就是它自己）。
    """
    N = len(x)
    if N == 1:
        return [x[0]]
    if N not in _TWIDDLE:
        _TWIDDLE[N] = [cmath.exp(-2j * math.pi * k / N) for k in range(N // 2)]
    tw = _TWIDDLE[N]
    E = fft(x[0::2])          # x[0::2]：從 0 開始每隔 2 個取 1 個 → 偶數位置
    O = fft(x[1::2])          # 奇數位置
    half = N // 2
    X = [0j] * N
    for k in range(half):
        t = tw[k] * O[k]      # 唯一的一次乘法
        X[k] = E[k] + t
        X[k + half] = E[k] - t
    return X


def is_pow2(n):
    """n 是 2 的次方嗎：2 的次方的二進位只有一個 1，n & (n−1) 會把最低的 1 清掉，結果是 0 才對。"""
    return n > 0 and (n & (n - 1)) == 0


def selftest():
    """用隨機資料比較 fft() 與 dft()：兩者的最大差距應該在 1e-6 以下（只剩浮點數的捨入誤差）。"""
    rng = random.Random(0)
    worst = 0.0
    for N in (8, 64, 512, 1024):
        x = [complex(rng.uniform(-32768, 32767), 0) for _ in range(N)]
        a, b = fft(x), dft(x)
        err = max(abs(p - q) for p, q in zip(a, b)) / max(abs(v) for v in b)   # 相對誤差（除以最大的 |X[k]|）
        worst = max(worst, err)
        print(f"N = {N:>5d}：|FFT − DFT| 的最大相對誤差 = {err:.2e}")
    print("selftest", "PASS" if worst < 1e-6 else "FAIL")
    sys.exit(0 if worst < 1e-6 else 1)


def ascii_spectrogram(rows, fs, N, hop_sec, max_cols=60, max_rows=80):
    """把 frames × N/2 的 |X| 矩陣縮成最多 max_rows 列 × max_cols 欄，用 9 個灰階印出來（時間往下、頻率往右）。"""
    # 縮小的方法叫 pooling：每 rp 個 frame、每 cp 個頻率 bin 合成一格，取格子裡的「最大值」（最亮的那一點代表這格）。
    rp = math.ceil(len(rows) / max_rows)
    cp = math.ceil(len(rows[0]) / max_cols)
    grid = []
    for r0 in range(0, len(rows), rp):
        block = rows[r0:r0 + rp]
        line = []
        for c0 in range(0, len(rows[0]), cp):
            line.append(max(row[c] for row in block for c in range(c0, min(c0 + cp, len(row)))))
        grid.append(line)
    top = max(v for line in grid for v in line) or 1.0
    top_db = 20 * math.log10(top)
    # 頻率軸：每 10 欄標一個刻度（單位 kHz），一欄代表 cp 個 bin = cp·fs/N Hz。
    ticks = "".join(f"{c * cp * fs / N / 1000:<10.1f}" for c in range(0, len(grid[0]), 10))
    print(f"\nASCII spectrogram：一格 = {rp} 個 frame × {cp} 個 bin（{cp * fs / N:.0f} Hz），"
          f"灰階 '{GREY}' 每階 {DB_RANGE / (len(GREY) - 1):.0f} dB，最強 = '@'")
    print("  時間(s)  kHz " + ticks)
    for i, line in enumerate(grid):
        chars = []
        for v in line:
            db = 20 * math.log10(v) if v > 0 else -999
            level = (db - (top_db - DB_RANGE)) / DB_RANGE * (len(GREY) - 1)
            chars.append(GREY[max(0, min(len(GREY) - 1, int(level)))])
        print(f"  {i * rp * hop_sec:>6.2f}      |" + "".join(chars) + "|")


def main():
    ap = argparse.ArgumentParser(description="STFT spectrogram（純 Python，自己寫的 FFT）")
    ap.add_argument("wav", nargs="?")
    ap.add_argument("--win", choices=["hamming", "rectangular"], default="hamming")
    ap.add_argument("--w_ms", type=int, default=32, help="視窗長度 P（ms）")
    ap.add_argument("--dft_ms", type=int, default=32, help="DFT 長度 N（ms），大於 w_ms 的部分補 0（zero padding）")
    ap.add_argument("--hop_ms", type=int, default=10, help="frame 間隔 M（ms）")
    ap.add_argument("--ascii", action="store_true", help="印 ASCII spectrogram")
    ap.add_argument("--selftest", action="store_true", help="只驗證 FFT 與 DFT 結果相同")
    args = ap.parse_args()
    if args.selftest:
        selftest()
    if not args.wav:
        ap.error("請給 WAV 檔名（或用 --selftest）")

    fs, x = read_wav(args.wav)
    # 三個參數都從 ms 換成 sample 數：sample 數 = fs × 毫秒 / 1000（和 numpy 參考程式一樣用整數除法）。
    P = fs * args.w_ms // 1000        # 視窗長度：一個 frame 有幾個 sample
    N = fs * args.dft_ms // 1000      # DFT 長度：N > P 的部分補 0
    M = fs * args.hop_ms // 1000      # hop：相鄰兩個 frame 的起點差幾個 sample
    frames = (len(x) - P) // M + 1 if len(x) >= P else 0
    use_fft = is_pow2(N)
    print(f"{args.wav}：fs = {fs} Hz，{len(x)} 個 sample（{len(x) / fs:.2f} 秒），視窗 = {args.win}")
    print(f"  P = {args.w_ms} ms = {P} samples（視窗）   N = {args.dft_ms} ms = {N} samples（DFT）   "
          f"M = {args.hop_ms} ms = {M} samples（hop）")
    print(f"  頻率解析度 fs/N = {fs / N:.3f} Hz／bin，輸出每列 N/2 = {N // 2} 個 bin（0 .. {fs / 2 - fs / N:.1f} Hz）")
    print(f"  frame 數 = (樣本數 − P)/M + 1 = ({len(x)} − {P})/{M} + 1 = {frames}（整數除法）")
    print(f"  每個 frame 的複數乘法：直接 DFT N² = {N * N:,d}；FFT (N/2)·log2(N) = {N // 2 * math.log2(N):,.0f}"
          + ("" if use_fft else f"（N = {N} 不是 2 的次方，這次只能用直接 DFT，會很慢）"))

    # 視窗係數：Hamming w[n] = 0.54 − 0.46·cos(2πn/(P−1))，和 numpy.hamming 一樣的對稱寫法；rectangular 全是 1。
    if args.win == "hamming":
        w = [0.54 - 0.46 * math.cos(2 * math.pi * n / (P - 1)) for n in range(P)]
    else:
        w = [1.0] * P

    t0 = time.perf_counter()
    rows = []
    for s in range(0, len(x) - P + 1, M):
        # framing：取 x[s .. s+P−1]；加窗：逐點相乘；zero padding：後面補 0 到 N 個（N < P 時只取前 N 個，和 numpy 一樣）
        frame = [x[s + n] * w[n] for n in range(P)]
        frame = (frame + [0.0] * (N - P)) if N >= P else frame[:N]
        X = fft(frame) if use_fft else dft(frame)
        # 實數訊號的頻譜是對稱的：X[N−k] 是 X[k] 的共軛，所以只留 k < N/2（0 到 fs/2 之間）的大小 |X[k]|。
        rows.append([abs(X[k]) for k in range(N // 2)])
    elapsed = time.perf_counter() - t0

    out = os.path.splitext(os.path.basename(args.wav))[0] + ".spec.txt"
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for row in rows:
            f.write(" ".join(f"{v:f}" for v in row) + "\n")      # "%f"：小數 6 位，和 numpy.savetxt(fmt="%f") 相同
    print(f"  {len(rows)} 個 frame 算完，花 {elapsed:.2f} 秒；寫出 {out}（{len(rows)} 列 × {N // 2} 欄）")
    if args.ascii and rows:
        ascii_spectrogram(rows, fs, N, M / fs)


main()
