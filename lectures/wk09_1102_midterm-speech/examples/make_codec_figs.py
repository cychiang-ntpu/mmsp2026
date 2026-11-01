#!/usr/bin/env python3
"""make_codec_figs.py — 產生 audio_coding.md 用的圖（純 Python 寫 SVG，不需要 numpy／matplotlib）

用法：python3 make_codec_figs.py            會在 ../figs/ 寫出：
  compand_curves.svg     μ-law、A-law、線性 的壓縮曲線 y = f(x)
  step_size.svg          三者「輸入振幅 ↔ 量化階距」：對數量化為什麼小聲時階距細
  sqnr_vs_level.svg      弦波從 0 到 −60 dBFS，線性 8-bit／μ-law／A-law／線性 16-bit 的 SQNR
  g711_segments.svg      G.711 μ-law 的 8 段折線近似（每段 16 階）
  dpcm_block.svg         DPCM 編碼器／解碼器的方塊圖（預測器在迴路裡）
  adpcm_demo.svg         一段語音：原訊號、1 階預測殘差、4-bit ADPCM 重建與階距的自動調整
  ima_step_table.svg     IMA ADPCM 的 89 個階距（對數成長）

所有數字都是程式當場算的（公式見 audio_coding.md）。
"""
import math
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "figs")
os.makedirs(OUT, exist_ok=True)
MU, A_LAW = 255.0, 87.6


# ------------------------------------------------------------------ 編碼曲線
def mu_c(x):   # 壓縮：[-1,1] → [-1,1]
    return math.copysign(math.log(1 + MU * abs(x)) / math.log(1 + MU), x)


def mu_e(y):   # 展開
    return math.copysign(((1 + MU) ** abs(y) - 1) / MU, y)


def a_c(x):
    ax = abs(x)
    if ax < 1 / A_LAW:
        return math.copysign(A_LAW * ax / (1 + math.log(A_LAW)), x)
    return math.copysign((1 + math.log(A_LAW * ax)) / (1 + math.log(A_LAW)), x)


def a_e(y):
    ay = abs(y)
    if ay < 1 / (1 + math.log(A_LAW)):
        return math.copysign(ay * (1 + math.log(A_LAW)) / A_LAW, y)
    return math.copysign(math.exp(ay * (1 + math.log(A_LAW)) - 1) / A_LAW, y)


def quant8(y):  # 8-bit 均勻量化 [-1,1] → 256 階
    return max(-128, min(127, round(y * 128))) / 128.0


# ------------------------------------------------------------------ SVG 小工具
class SVG:
    def __init__(self, w, h, title):
        self.w, self.h = w, h
        self.parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" '
                      f'font-family="Noto Sans CJK TC, Microsoft JhengHei, Arial, sans-serif" font-size="13">',
                      f'<rect width="{w}" height="{h}" fill="white"/>',
                      f'<text x="{w/2}" y="22" text-anchor="middle" font-size="16" font-weight="bold">{title}</text>']

    def add(self, s):
        self.parts.append(s)

    def save(self, name):
        self.parts.append("</svg>")
        open(os.path.join(OUT, name), "w", encoding="utf-8").write("\n".join(self.parts))
        print("wrote", name)


class Axes:
    """把資料座標映到畫布：x0..x1 → left..right，y0..y1 → bottom..top"""
    def __init__(self, svg, left, top, right, bottom, x0, x1, y0, y1, xlabel="", ylabel="", xticks=(), yticks=()):
        self.s, self.l, self.t, self.r, self.b = svg, left, top, right, bottom
        self.x0, self.x1, self.y0, self.y1 = x0, x1, y0, y1
        svg.add(f'<rect x="{left}" y="{top}" width="{right-left}" height="{bottom-top}" fill="none" stroke="#333"/>')
        for xv, lab in xticks:
            X = self.X(xv)
            svg.add(f'<line x1="{X}" y1="{bottom}" x2="{X}" y2="{bottom+5}" stroke="#333"/>'
                    f'<line x1="{X}" y1="{top}" x2="{X}" y2="{bottom}" stroke="#eee"/>'
                    f'<text x="{X}" y="{bottom+18}" text-anchor="middle">{lab}</text>')
        for yv, lab in yticks:
            Y = self.Y(yv)
            svg.add(f'<line x1="{left-5}" y1="{Y}" x2="{left}" y2="{Y}" stroke="#333"/>'
                    f'<line x1="{left}" y1="{Y}" x2="{right}" y2="{Y}" stroke="#eee"/>'
                    f'<text x="{left-8}" y="{Y+4}" text-anchor="end">{lab}</text>')
        if xlabel:
            svg.add(f'<text x="{(left+right)/2}" y="{bottom+36}" text-anchor="middle">{xlabel}</text>')
        if ylabel:
            svg.add(f'<text x="{left-48}" y="{(top+bottom)/2}" text-anchor="middle" transform="rotate(-90 {left-48} {(top+bottom)/2})">{ylabel}</text>')

    def X(self, x):
        return self.l + (x - self.x0) / (self.x1 - self.x0) * (self.r - self.l)

    def Y(self, y):
        return self.b - (y - self.y0) / (self.y1 - self.y0) * (self.b - self.t)

    def line(self, pts, color, width=2, dash=""):
        d = " ".join(f"{self.X(x):.1f},{self.Y(y):.1f}" for x, y in pts)
        self.s.add(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="{width}" '
                   f'{"stroke-dasharray=\"" + dash + "\"" if dash else ""}/>')

    def legend(self, items, x, y):
        for i, (lab, color, dash) in enumerate(items):
            yy = y + i * 18
            self.s.add(f'<line x1="{x}" y1="{yy}" x2="{x+28}" y2="{yy}" stroke="{color}" stroke-width="3" '
                       f'{"stroke-dasharray=\"" + dash + "\"" if dash else ""}/><text x="{x+34}" y="{yy+4}">{lab}</text>')


# ------------------------------------------------------------------ 圖 1：壓縮曲線
def fig_curves():
    s = SVG(640, 420, "壓縮曲線：輸入 x（正規化振幅）→ 壓縮後 y（要拿去做 8-bit 均勻量化的值）")
    ax = Axes(s, 70, 40, 610, 360, 0, 1, 0, 1, "輸入 |x|（0 = 無聲，1 = 滿刻度）", "壓縮後 |y|",
              [(i / 10, f"{i/10:.1f}") for i in range(0, 11, 2)], [(i / 10, f"{i/10:.1f}") for i in range(0, 11, 2)])
    xs = [i / 400 for i in range(401)]
    ax.line([(x, x) for x in xs], "#999", 2, "6,4")
    ax.line([(x, mu_c(x)) for x in xs], "#d62728", 3)
    ax.line([(x, a_c(x)) for x in xs], "#1f77b4", 3, "8,3")
    ax.legend([("線性（y = x）", "#999", "6,4"), ("μ-law（μ = 255）", "#d62728", ""), ("A-law（A = 87.6）", "#1f77b4", "8,3")], 330, 300)
    s.add('<text x="90" y="60" fill="#d62728">小聲的輸入（x 很小）被放大很多倍 → 之後均勻量化時等於用了很細的階距</text>')
    s.add('<text x="90" y="78" fill="#555">大聲的輸入曲線變平 → 階距粗，但大聲時相對誤差仍小</text>')
    s.save("compand_curves.svg")


# ------------------------------------------------------------------ 圖 2：階距 vs 振幅
def fig_step():
    s = SVG(640, 420, "每一階在「輸入」軸上有多寬：8-bit 線性 vs μ-law vs A-law（對數座標）")
    ax = Axes(s, 80, 40, 610, 360, -4, 0, -4.5, -0.5, "輸入振幅 log10|x|（−4 = 0.0001，0 = 滿刻度）", "階距 log10 Δ（在輸入軸上）",
              [(i, f"{i}") for i in range(-4, 1)], [(i, f"{i}") for i in range(-4, 0)])
    pts_lin, pts_mu, pts_a = [], [], []
    for k in range(-400, 1):
        lx = k / 100
        x = 10 ** lx
        pts_lin.append((lx, math.log10(1 / 128)))
        y = mu_c(x); pts_mu.append((lx, math.log10(max(1e-9, mu_e(min(1, y + 1 / 128)) - mu_e(y)))))
        y = a_c(x); pts_a.append((lx, math.log10(max(1e-9, a_e(min(1, y + 1 / 128)) - a_e(y)))))
    ax.line(pts_lin, "#999", 2, "6,4"); ax.line(pts_mu, "#d62728", 3); ax.line(pts_a, "#1f77b4", 3, "8,3")
    ax.legend([("線性 8-bit：Δ 固定 1/128", "#999", "6,4"), ("μ-law：Δ 隨振幅變，小聲時細 60 倍", "#d62728", ""), ("A-law：小聲區固定、之後對數", "#1f77b4", "8,3")], 110, 70)
    s.save("step_size.svg")


# ------------------------------------------------------------------ 圖 3：SQNR vs 音量
def sqnr_sine(level_db, codec):
    n, f, fs = 32000, 997.0, 16000           # 997 Hz 不是 bin 的整數倍，誤差才像雜訊；2 秒夠平均
    g = 10 ** (level_db / 20)
    ps = pn = 0.0
    for i in range(n):
        x = g * math.sin(2 * math.pi * f * i / fs)
        if codec == "lin8":
            xq = max(-128, min(127, round(x * 128))) / 128
        elif codec == "lin16":
            xq = max(-32768, min(32767, round(x * 32768))) / 32768
        elif codec == "mu":
            xq = mu_e(quant8(mu_c(x)))
        else:
            xq = a_e(quant8(a_c(x)))
        ps += x * x; pn += (x - xq) ** 2
    return 10 * math.log10(ps / pn) if pn > 0 else 100


def fig_sqnr():
    s = SVG(640, 420, "SQNR 隨音量的變化（1 kHz 弦波）：為什麼電話用 8 bits 也能聽")
    ax = Axes(s, 70, 40, 610, 360, -60, 0, 0, 100, "輸入音量（dBFS）", "SQNR（dB）",
              [(v, f"{v}") for v in range(-60, 1, 10)], [(v, f"{v}") for v in range(0, 101, 20)])
    levels = list(range(-60, 1, 2))
    rows = {}
    for codec, color, dash in [("lin16", "#2ca02c", "2,3"), ("lin8", "#999", "6,4"), ("mu", "#d62728", ""), ("a", "#1f77b4", "8,3")]:
        pts = [(L, sqnr_sine(L, codec)) for L in levels]
        rows[codec] = dict(pts)
        ax.line(pts, color, 3 if codec in ("mu", "a") else 2, dash)
    ax.legend([("線性 16-bit", "#2ca02c", "2,3"), ("線性 8-bit", "#999", "6,4"), ("μ-law 8-bit", "#d62728", ""), ("A-law 8-bit", "#1f77b4", "8,3")], 400, 300)
    s.add(f'<text x="90" y="60" fill="#555">線性每小 10 dB 就掉 10 dB；μ-law／A-law 在 40 dB 的音量範圍內幾乎不變</text>')
    s.save("sqnr_vs_level.svg")
    return rows


# ------------------------------------------------------------------ 圖 4：G.711 的 8 段折線
def fig_segments():
    s = SVG(640, 440, "G.711 μ-law 的做法：用 8 段折線近似曲線，每段 16 階（共 128 階 × 正負號 = 8 bits）")
    ax = Axes(s, 70, 40, 610, 360, 0, 4096, 0, 128, "輸入的絕對值（13 bits，0–4095；G.711 另有 +33 的偏移，這裡省略）", "輸出碼（0–127）",
              [(v, f"{v}") for v in range(0, 4097, 512)], [(v, f"{v}") for v in range(0, 129, 16)])
    # 段的邊界：0,32,64,128,256,...,8192（每段長度加倍，每段 16 階）
    edges = [0, 32, 64, 128, 256, 512, 1024, 2048, 4096]        # 8 段：寬 32、32、64、128、…、2048
    pts = []
    for seg in range(8):
        lo, hi = edges[seg], edges[seg + 1]
        for k in range(17):
            pts.append((lo + (hi - lo) * k / 16, seg * 16 + k))
    ax.line(pts, "#d62728", 3)
    xs = [i * 4096 / 400 for i in range(401)]
    ax.line([(x, 128 * mu_c(x / 4096)) for x in xs], "#1f77b4", 1.5, "4,3")
    for seg in range(8):
        X = ax.X(edges[seg + 1]); s.add(f'<line x1="{X}" y1="{ax.t}" x2="{X}" y2="{ax.b}" stroke="#bbb" stroke-dasharray="2,3"/>')
    ax.legend([("8 段折線（真正的 G.711）", "#d62728", ""), ("連續的 μ-law 曲線", "#1f77b4", "4,3")], 330, 320)
    s.add('<text x="90" y="60" fill="#555">段 0 的 16 階每階寬 2；段 7 的每階寬 128 → 階距隨振幅加倍，等於「相對誤差固定」</text>')
    s.add('<text x="90" y="78" fill="#555">一個 byte = 符號 1 bit ＋ 段號 3 bits ＋ 段內 4 bits；查表就能編解碼</text>')
    s.add(f'<text x="{(ax.l+ax.r)/2}" y="{ax.b+52}" text-anchor="middle" fill="#555">（正半邊；負半邊對稱，由符號 bit 區分）</text>')
    s.save("g711_segments.svg")


# ------------------------------------------------------------------ 圖 5：DPCM 方塊圖
def box(s, x, y, w, h, text, fill="#f4f6fa"):
    s.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="{fill}" stroke="#333"/>')
    lines = text.split("\n")
    for i, t in enumerate(lines):
        s.add(f'<text x="{x+w/2}" y="{y+h/2+5+(i-(len(lines)-1)/2)*16}" text-anchor="middle">{t}</text>')


def arrow(s, x1, y1, x2, y2, label="", above=True):
    s.add(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#333" stroke-width="1.5" marker-end="url(#ah)"/>')
    if label:
        s.add(f'<text x="{(x1+x2)/2}" y="{(y1+y2)/2 + (-6 if above else 16)}" text-anchor="middle" fill="#1f77b4">{label}</text>')


def fig_dpcm():
    from codec_fig_dpcm import draw
    draw(SVG, box, arrow)


IMA_STEPS = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IMA_INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]


def read_wav(path):
    d = open(path, "rb").read()
    pos, pcm = 12, b""
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack("<I", d[pos + 4:pos + 8])[0]
        if cid == b"data":
            pcm = d[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)
    return list(struct.unpack("<%dh" % (len(pcm) // 2), pcm[:len(pcm) // 2 * 2]))


def ima_encode_decode(x):
    """IMA ADPCM（4 bits／sample）：回傳重建值與每個 sample 用的階距。"""
    pred, idx, recon, steps = 0, 0, [], []
    for v in x:
        step = IMA_STEPS[idx]
        diff = v - pred
        code = 0
        if diff < 0:
            code, diff = 8, -diff
        q = 0
        if diff >= step: code |= 4; diff -= step; q += step
        if diff >= step // 2: code |= 2; diff -= step // 2; q += step // 2
        if diff >= step // 4: code |= 1; q += step // 4
        q += step // 8
        pred = max(-32768, min(32767, pred - q if code & 8 else pred + q))
        idx = max(0, min(88, idx + IMA_INDEX[code & 7]))
        recon.append(pred); steps.append(step)
    return recon, steps


def fig_adpcm():
    wav = os.path.join(HERE, "..", "..", "wk07_1019_audio-pcm", "data", "aeueo-16kHz.wav")
    x = read_wav(wav)[16000:16480]                      # t = 1.0 s 起 30 ms（母音，看得到週期）
    e1 = [x[0]] + [x[n] - x[n - 1] for n in range(1, len(x))]
    recon, steps = ima_encode_decode(x)
    s = SVG(760, 585, "真實語音 30 ms：原訊號、1 階預測殘差、4-bit IMA ADPCM 的重建與自動調整的階距")
    s.add('<text x="60" y="48" fill="#555">檔案 aeueo-16kHz.wav，t = 1.0 s 起 480 個 sample（母音，F₀ ≈ 157 Hz 的週期看得到）</text>')
    ax1 = Axes(s, 60, 60, 740, 200, 0, 480, -12000, 12000, "", "sample 值", [], [(-10000, "−10000"), (0, "0"), (10000, "10000")])
    ax1.line(list(enumerate(x)), "#333", 1.5); ax1.line(list(enumerate(recon)), "#d62728", 1.2, "3,2")
    ax1.legend([("原訊號 x[n]（16-bit）", "#333", ""), ("ADPCM 4-bit 重建 x̃[n]", "#d62728", "3,2")], 480, 80)
    ax2 = Axes(s, 60, 230, 740, 370, 0, 480, -12000, 12000, "", "sample 值", [], [(-10000, "−10000"), (0, "0"), (10000, "10000")])
    ax2.line(list(enumerate(e1)), "#1f77b4", 1.5)
    ax2.legend([("1 階預測殘差 e[n] = x[n] − x[n−1]（小很多 → 可以用少的 bits）", "#1f77b4", "")], 120, 250)
    ax3 = Axes(s, 60, 400, 740, 520, 0, 480, 0, 3000, "sample（n）", "階距 Δ", [(k, str(k)) for k in range(0, 481, 80)], [(0, "0"), (1000, "1000"), (2000, "2000"), (3000, "3000")])
    ax3.line(list(enumerate(steps)), "#2ca02c", 2)
    ax3.legend([("ADPCM 的階距 Δ[n]：波形變化大時自動放大、平緩時縮小", "#2ca02c", "")], 120, 420)
    ps = sum(v * v for v in x) / len(x); pn = sum((a - b) ** 2 for a, b in zip(x, recon)) / len(x)
    pe = sum(v * v for v in e1) / len(x)
    s.add(f'<text x="60" y="572" fill="#555">這一段：殘差功率比原訊號少 {10*math.log10(ps/pe):.1f} dB；4-bit ADPCM 的 SQNR = {10*math.log10(ps/pn):.1f} dB（64 kb/s，原本 256 kb/s）</text>')
    s.save("adpcm_demo.svg")


def fig_ima_table():
    s = SVG(640, 400, "IMA ADPCM 的階距表：89 個 Δ，每一格約 ×1.1，從 7 到 32767（對數成長）")
    ax = Axes(s, 70, 40, 610, 340, 0, 88, 0, 4.6, "表的索引（上一個 q 大 → 索引 +2～+8；小 → −1）", "log10 Δ",
              [(k, str(k)) for k in range(0, 89, 11)], [(k, str(k)) for k in range(0, 5)])
    ax.line([(i, math.log10(v)) for i, v in enumerate(IMA_STEPS)], "#2ca02c", 3)
    s.add('<text x="90" y="60" fill="#555">用索引加減而不是直接乘除，是為了用整數就能做、而且兩邊永遠一致</text>')
    s.save("ima_step_table.svg")


if __name__ == "__main__":
    fig_curves(); fig_step(); rows = fig_sqnr(); fig_segments(); fig_dpcm(); fig_adpcm(); fig_ima_table()
    print("SQNR table (dB):")
    print("  level   lin8   mu   a   lin16")
    for L in (0, -10, -20, -30, -40, -50):
        print(f"  {L:5d}  {rows['lin8'][L]:5.1f} {rows['mu'][L]:5.1f} {rows['a'][L]:5.1f} {rows['lin16'][L]:6.1f}")
