#!/usr/bin/env python3
"""window_demo.py — 視窗函數的代價：主瓣寬度 vs 旁瓣高度，以及 leakage。只用標準函式庫，不需要參數。

用法：python3 window_demo.py

做兩件事：
  1. 170 Hz 的弦波（fs = 8000 Hz）取 256 個 sample，分別乘 rectangular／Hann／Hamming 三種視窗，
     補 0 到 1024 點再算 |DFT|（補 0 只是讓頻譜的形狀看得更細，不會多出資訊），
     印出每種視窗的主瓣寬度（兩側第一個谷之間有幾個 bin）、峰值 dB、最高旁瓣相對峰值的 dB。
  2. leakage：同樣 256 點、不補 0，比較 187.5 Hz（剛好是第 6 個 bin：6 × 8000/256 = 187.5）與 170 Hz（落在兩個 bin 之間）
     的 |X[k]|，k = 0..12：bin 對正時 rectangular 只有一個 bin 非 0；沒對正時能量「漏」到所有 bin。

結論（三行）：
  * rectangular 主瓣最窄（2 個 bin）但旁瓣只低 13 dB：頻率分得最細，可是弱的成分會被強成分的旁瓣蓋住。
  * Hann／Hamming 主瓣寬一倍（4 個 bin），旁瓣分別低到約 −31／−41 dB：犧牲一點頻率解析度，換來乾淨的頻譜。
  * leakage 的來源是「frame 的頭尾接不起來」（見 frame_demo.py）；視窗把頭尾壓小，漏出去的能量就少。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：1.x STFT（window 的選擇：主瓣、旁瓣、leakage）。
# 怎麼跑：  python3 window_demo.py（Windows 打 python），不用參數，約 1 秒。
# 會看到：  (1) 三種視窗的表：主瓣寬度 8／16／18 個 bin（1024 點的刻度；換算回 256 點是 2／4／4.5）、
#               旁瓣 −12／−31.5／−37.5 dB（教科書的 Hamming 是 −42 dB；170 Hz 離 bin 中心 0.44 格、只有 256 點，所以略高）；
#           (2) k = 0..12 的 |X[k]| 表：187.5 Hz + rectangular 那欄只有 k = 6 非 0，其他欄每個 bin 都有值。
# 注意：    dB 是相對值：20·log10(|X[k]| / 峰值)。峰值本身的 dB 和振幅、視窗面積有關，比較「相對峰值」才有意義。
# ──────────────────────────────────────────────────────────────────────────────────────
import cmath
import math
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

FS = 8000          # 取樣率（Hz）
P = 256            # 視窗長度（sample）；頻率 bin 的間距 = fs/P = 31.25 Hz
NPAD = 1024        # 補 0 之後的 DFT 長度：bin 間距變成 fs/1024 = 7.8125 Hz，形狀看得更清楚
F_TONE = 170.0     # 沒有對正 bin 的頻率（170 / 31.25 = 5.44 個 bin）
F_BIN = 187.5      # 剛好對正第 6 個 bin 的頻率


def windows(n):
    """三種視窗的係數 w[0..n−1]（都用 numpy 的對稱寫法：分母是 n−1，兩端 Hann = 0、Hamming = 0.08）。"""
    return {
        "rectangular": [1.0] * n,
        "hann":        [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)],
        "hamming":     [0.54 - 0.46 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)],
    }


def dft_mag(x, N):
    """x 補 0 到 N 點後的 |X[k]|，k = 0..N/2。照定義算：X[k] = Σ_n x[n]·e^(−j2πkn/N)（只需加到 x 的長度，後面都是 0）。"""
    tw = [cmath.exp(-2j * math.pi * i / N) for i in range(N)]
    return [abs(sum(x[n] * tw[(k * n) % N] for n in range(len(x)))) for k in range(N // 2 + 1)]


def lobe_stats(mag):
    """從 |X[k]| 找：峰值位置、主瓣寬度（左右兩側第一個谷之間的 bin 數）、主瓣以外最高的旁瓣（相對峰值的 dB）。"""
    kp = max(range(len(mag)), key=lambda k: mag[k])          # 峰值的 bin
    lo = kp
    while lo > 0 and mag[lo - 1] < mag[lo]:                  # 往左走到第一個谷（開始回升就停）
        lo -= 1
    hi = kp
    while hi < len(mag) - 1 and mag[hi + 1] < mag[hi]:       # 往右走到第一個谷
        hi += 1
    side = max(mag[k] for k in range(len(mag)) if k < lo or k > hi)   # 主瓣以外的最大值 = 最高旁瓣
    return kp, hi - lo, 20 * math.log10(mag[kp]), 20 * math.log10(side / mag[kp])


def tone(f, n):
    return [math.sin(2 * math.pi * f * i / FS) for i in range(n)]


# ── 第 1 件事：三種視窗的主瓣與旁瓣 ──
x = tone(F_TONE, P)
print(f"{F_TONE:g} Hz 弦波，fs = {FS} Hz，取 {P} 個 sample，補 0 到 {NPAD} 點算 |DFT|（1 個 bin = {FS / NPAD:.4f} Hz）")
print(f"{'視窗':<12} {'峰值 bin':>8} {'峰值 Hz':>9} {'主瓣寬(bin)':>11} {'=256點的bin':>11} {'峰值 dB':>9} {'最高旁瓣(相對峰值)':>18}")
for name, w in windows(P).items():
    mag = dft_mag([v * c for v, c in zip(x, w)], NPAD)
    kp, width, peak_db, side_db = lobe_stats(mag)
    print(f"{name:<12} {kp:>8d} {kp * FS / NPAD:>9.2f} {width:>11d} {width * P / NPAD:>11.1f} "
          f"{peak_db:>9.2f} {side_db:>14.1f} dB")

# ── 第 2 件事：leakage——bin 對正與沒對正 ──
# 不補 0（N = P = 256）：bin 間距 31.25 Hz。187.5 Hz 是 6.00 個 bin，170 Hz 是 5.44 個 bin。
print(f"\n不補 0 的 {P} 點 DFT（1 個 bin = {FS / P} Hz），|X[k]| 相對該欄峰值的 dB，k = 0..12：")
cols = []
for f in (F_BIN, F_TONE):
    for name in ("rectangular", "hann"):
        w = windows(P)[name]
        mag = dft_mag([v * c for v, c in zip(tone(f, P), w)], P)
        peak = max(mag)
        cols.append((f"{f:g}Hz {name[:4]}", [20 * math.log10(max(m / peak, 1e-12)) for m in mag[:13]]))
print(f"{'k':>3}  {'Hz':>7}  " + "  ".join(f"{title:>13}" for title, _ in cols))
for k in range(13):
    cells = "  ".join(f"{col[k]:>10.1f} dB" if col[k] > -100 else f"{'0 (−∞ dB)':>13}" for _, col in cols)
    print(f"{k:>3}  {k * FS / P:>7.2f}  {cells}")
print("\n187.5 Hz 是 bin 6 的整數倍：rectangular 時 256 個 sample 剛好裝下 6 個完整週期，頭尾接得起來，只有 k = 6 非 0；")
print("170 Hz 裝 5.44 個週期，接縫有跳動，能量漏到每一個 bin（rectangular 在 k = 0 還有 −30 dB 左右）；Hann 把漏出去的壓低很多。")
