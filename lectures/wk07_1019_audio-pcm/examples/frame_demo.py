#!/usr/bin/env python3
"""frame_demo.py — 一個 32 ms 的語音 frame 長什麼樣子：週期、基頻 F0、為什麼要加窗。只用標準函式庫。

用法：python3 frame_demo.py <wav> [t_sec]
      例：python3 frame_demo.py ../data/aeueo-16kHz.wav 1.0    （沒給 t_sec 就取檔案的正中間）

做三件事：
  1. 從 16-bit 單聲道 WAV 的 t_sec 秒處取 32 ms（16 kHz → 512 個 sample），
     用自相關（autocorrelation）估計基本週期 T0，印出 F0 = fs / T0（Hz）。
  2. 把前兩個週期的 sample 畫成 ASCII 波形（最多 64 列），用眼睛確認「母音是週期性的」。
  3. 把同一個 frame「頭尾相接、週期性地重複」，只看接縫前後各 8 個 sample：
     不加窗（rectangular）接縫處會跳一大步；加 Hann 窗後兩端都壓到 0，接縫就平滑了。
     DFT 把 frame 當成週期訊號來分析，接縫的跳動會變成頻譜上的假成分（leakage），這就是 MP5 要加 Hamming 窗的原因。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：1.x STFT（語音的 frame 與週期性、加窗）。
# 怎麼跑：  python3 frame_demo.py ../data/aeueo-16kHz.wav        （Windows 打 python）
#           python3 frame_demo.py ../data/speech_osr_16k.wav 2.3  自己換一個時間點，看子音或靜音段會變成什麼樣
# 會看到：  (1) F0 的估計值與自相關係數；(2) 兩個週期的 ASCII 波形，兩個週期長得幾乎一樣；
#           (3) 接縫前後 16 個 sample 的表：rectangular 那欄在接縫跳一大步，Hann 那欄平滑地經過 0。
# 注意：    自相關只適合「有週期」的段落（母音）。取到靜音或子音時 F0 沒有意義，看係數 r 很小就知道。
# ──────────────────────────────────────────────────────────────────────────────────────
import math
import struct
import sys

# Windows 上把輸出導到檔案或管線（例如 > out.txt）時，Python 會改用 cp950 編碼，印不出 −、²、≤ 這些符號而當掉；
# 這裡強制用 UTF-8。直接在終端機執行時本來就沒問題。
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

FRAME_MS = 32          # 一個 frame 的長度（ms）
F0_MIN, F0_MAX = 60, 400   # 人聲基頻的合理範圍（Hz）：男低音約 80，女高音約 300


def read_wav(path):
    """讀 16-bit、單聲道的 PCM WAV，回傳 (fs, sample 的 list)。照 RIFF chunk 一個一個走，不假設 data 在第 44 byte。"""
    # WAV 檔 = "RIFF" + 總長度 + "WAVE"，後面接一串 chunk；每個 chunk = 4 bytes 的名字 + 4 bytes 的長度 + 內容。
    # 我們只需要 "fmt "（格式）與 "data"（PCM 本體）兩個 chunk，其他的（例如 "LIST" 註解）跳過。
    # struct.unpack("<I", …) 把 4 bytes 轉成無號整數，"<" 是 little-endian（WAV 的規定，第 3 週 2.6）。
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
            fmt = struct.unpack("<HHIIHH", body[:16])      # (格式代碼, 聲道數, fs, byte rate, block align, bits)
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)                       # chunk 的長度是奇數時後面補 1 byte 對齊
    if fmt is None or pcm is None:
        sys.exit(f"{path}：找不到 fmt 或 data chunk")
    tag, channels, fs, _, _, bits = fmt
    if tag != 1 or channels != 1 or bits != 16:
        sys.exit(f"{path}：只接受 16-bit 單聲道 PCM（這個檔是 tag={tag} ch={channels} bits={bits}）")
    # "<{n}h"：n 個 little-endian 的 16-bit 有號整數（C 的 short），一次全部解開。
    return fs, list(struct.unpack(f"<{len(pcm) // 2}h", pcm))


def autocorr_f0(frame, fs):
    """用自相關估計基本週期：找讓 r[τ] = Σ x[n]·x[n+τ] 最大的 τ（在 F0 的合理範圍內）。回傳 (τ, r[τ]/r[0])。"""
    # 「週期是 T0」的意思是 x[n] ≈ x[n+T0]：把訊號往右平移 T0 個 sample，和原來的幾乎重疊。
    # r[τ] 就是「平移 τ 之後和原來有多像」：相乘再相加，重疊得好就大，錯開就小（正負互相抵消）。
    # 先把平均值扣掉（去除直流），不然 r[τ] 全部偏大，看不出峰。
    mean = sum(frame) / len(frame)
    x = [v - mean for v in frame]
    n = len(x)
    r0 = sum(v * v for v in x) or 1.0                       # τ = 0 時最像自己，用它來正規化成 0..1 的係數
    best_lag, best_r = 0, -1.0
    # τ 的搜尋範圍：F0 = fs/τ 要落在 60..400 Hz，所以 τ 從 fs/400 到 fs/60（16 kHz 是 40..266 個 sample）。
    for lag in range(fs // F0_MAX, min(fs // F0_MIN, n - 1) + 1):
        r = sum(x[i] * x[i + lag] for i in range(n - lag)) / r0
        if r > best_r:
            best_lag, best_r = lag, r
    return best_lag, best_r


def ascii_wave(samples, rows=64, width=30):
    """把一串 sample 畫成一列一列的 ASCII 長條：中線是 0，正值往右畫 |，負值往左畫 |。最多 rows 列（多的就隔幾個取一個）。"""
    step = max(1, math.ceil(len(samples) / rows))           # 2 個週期可能有 200 個 sample，每 step 個取 1 個來畫
    peak = max(abs(v) for v in samples) or 1
    for i in range(0, len(samples), step):
        v = samples[i]
        k = round(abs(v) / peak * width)                     # 長條的長度：振幅相對於這段最大值，最長 width 格
        if v >= 0:
            line = " " * width + "+" + "|" * k
        else:
            line = " " * (width - k) + "|" * k + "+"
        print(f"  {i:>4d} {v:>7d}  {line}")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    fs, x = read_wav(sys.argv[1])
    t_sec = float(sys.argv[2]) if len(sys.argv) > 2 else len(x) / fs / 2
    P = fs * FRAME_MS // 1000                                # 32 ms 有幾個 sample：16 kHz → 512
    start = min(max(0, int(t_sec * fs)), len(x) - P)
    frame = x[start:start + P]
    print(f"{sys.argv[1]}：fs = {fs} Hz，{len(x)} 個 sample（{len(x) / fs:.2f} 秒）")
    print(f"取 t = {start / fs:.3f} s 起的 {FRAME_MS} ms = {P} 個 sample（sample {start} .. {start + P - 1}）\n")

    # ── 第 1 件事：自相關估 F0 ──
    lag, r = autocorr_f0(frame, fs)
    print(f"自相關最大的平移量 τ = {lag} 個 sample = {lag / fs * 1000:.2f} ms，係數 r[τ]/r[0] = {r:.3f}")
    print(f"→ 基頻 F0 = fs / τ = {fs} / {lag} = {fs / lag:.1f} Hz" + ("" if r > 0.5 else "   （r 很小：這段可能不是母音，F0 不可信）"))

    # ── 第 2 件事：畫前兩個週期 ──
    print(f"\n前兩個週期（2 × {lag} = {2 * lag} 個 sample）的波形：第 2 個週期和第 1 個幾乎一樣")
    print("     n     x[n]  " + " " * 18 + "−  …  0  …  +")
    ascii_wave(frame[:2 * lag])

    # ── 第 3 件事：接縫 ──
    # DFT 在數學上是把這 P 個 sample 當成「一直重複」的週期訊號；重複的地方就是 x[P−1] 接 x[0]。
    # Hann 窗 w[n] = 0.5 − 0.5·cos(2πn/(P−1))：中間是 1，兩端剛好是 0（和 numpy.hanning 一樣的對稱寫法）。
    # 乘上去之後 frame 的頭尾都變成 0，接縫自然就沒有跳動；代價是中間的 sample 比兩端「重要」。
    w = [0.5 - 0.5 * math.cos(2 * math.pi * n / (P - 1)) for n in range(P)]
    print(f"\n把這 {P} 個 sample 頭尾相接重複時，接縫（x[{P - 1}] → x[0]）前後各 8 個 sample：")
    print("        n   rectangular      Hann 窗 w[n]   w[n]·x[n]")
    for n in list(range(P - 8, P)) + list(range(8)):
        mark = "   ← 接縫" if n == 0 else ""
        print(f"   {n:>6d}   {frame[n]:>11d}   {w[n]:>13.4f}   {w[n] * frame[n]:>9.1f}{mark}")
    print(f"\n接縫的跳動：rectangular |x[0] − x[{P - 1}]| = {abs(frame[0] - frame[P - 1])}，"
          f"Hann 窗 |w[0]x[0] − w[{P - 1}]x[{P - 1}]| = {abs(w[0] * frame[0] - w[P - 1] * frame[P - 1]):.1f}")


main()
