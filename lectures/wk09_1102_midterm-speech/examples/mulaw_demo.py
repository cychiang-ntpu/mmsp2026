#!/usr/bin/env python3
"""mulaw_demo.py — 為什麼電話只用 8 bits：μ-law 壓伸（companding）對 8-bit 線性量化的 SQNR 比較

用法：python3 mulaw_demo.py [檔案.wav]
      不給檔案就用 1 kHz 的弦波；給 16-bit 單聲道 WAV 就拿真實語音來量。

做的事（對應講義 2.2）：
  1. 把 16-bit 的 sample 當作「原始訊號」x。
  2. 線性 8-bit：直接丟掉低 8 bits（除以 256 再四捨五入）——大聲時還好，小聲時誤差相對很大。
  3. μ-law 8-bit（G.711 的想法，μ = 255）：先把 x 壓成 log 曲線 y = sign(x)·ln(1 + μ|x|)／ln(1 + μ)，
     再對 y 做 8-bit 均勻量化，解碼時反過來展開。小訊號的階距小、大訊號的階距大，所以 SQNR 幾乎不隨音量變。
  4. 對不同音量（0 dB、−10、−20、−30、−40 dBFS）各算一次 SQNR，印成表。
只用標準函式庫；真的 G.711 是用分段折線近似這條曲線，數字會差一點點，形狀一樣。
"""
import math
import struct
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

MU = 255.0


def read_wav(path):
    """照 chunk 走的 16-bit 單聲道 WAV 讀檔（data 不一定在第 44 byte）。回傳 sample 的 list。"""
    d = open(path, "rb").read()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise SystemExit("不是 WAV")
    pos, pcm = 12, b""
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack("<I", d[pos + 4:pos + 8])[0]
        body = d[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt, ch, _, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if fmt != 1 or ch != 1 or bits != 16:
                raise SystemExit("只收 16-bit 單聲道 PCM")
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    return list(struct.unpack("<%dh" % (len(pcm) // 2), pcm[:len(pcm) // 2 * 2]))


def sqnr_db(x, xq):
    ps = sum(v * v for v in x) / len(x)
    pn = sum((a - b) ** 2 for a, b in zip(x, xq)) / len(x)
    return 10 * math.log10(ps / pn) if pn > 0 else float("inf")


def linear8(x):
    """線性 8-bit：把 16-bit 的範圍切成 256 階（階距 256），四捨五入。"""
    return [max(-128, min(127, round(v / 256))) * 256 for v in x]


def mulaw8(x):
    """μ-law 8-bit：壓縮 → 256 階均勻量化 → 展開。x 先正規化到 [-1, 1]。"""
    out = []
    for v in x:
        u = v / 32768.0
        y = math.copysign(math.log(1 + MU * abs(u)) / math.log(1 + MU), u)      # 壓縮：[-1,1] → [-1,1]，log 曲線
        q = max(-128, min(127, round(y * 128))) / 128.0                           # 8-bit 均勻量化（256 階）
        u2 = math.copysign(((1 + MU) ** abs(q) - 1) / MU, q)                      # 展開：反函數
        out.append(u2 * 32768.0)
    return out


def main():
    if len(sys.argv) > 1:
        src = read_wav(sys.argv[1])
        name = sys.argv[1]
    else:
        fs, f = 16000, 1000.0
        src = [32767 * math.sin(2 * math.pi * f * n / fs) for n in range(fs)]      # 1 秒的 1 kHz 滿刻度弦波
        name = "1 kHz 弦波（滿刻度）"
    peak = max(abs(v) for v in src) or 1
    print(f"訊號：{name}，{len(src)} 個 sample")
    print("把同一段訊號調到不同音量，各用兩種 8-bit 量化，量 SQNR：")
    print("  音量(dBFS)   線性 8-bit    μ-law 8-bit    線性 16-bit（原始，當參考）")
    for db in (0, -10, -20, -30, -40):
        g = 10 ** (db / 20) * 32767 / peak
        x = [v * g for v in src]
        x16 = [max(-32768, min(32767, round(v))) for v in x]
        print(f"  {db:6d}       {sqnr_db(x, linear8(x16)):7.1f} dB    {sqnr_db(x, mulaw8(x16)):7.1f} dB     {sqnr_db(x, x16):7.1f} dB")
    print("讀法：線性 8-bit 每小 10 dB 就掉 10 dB（階距固定，小聲時誤差相對變大）；")
    print("      μ-law 幾乎不變（小訊號用細的階距），這就是電話用 8 bits 也能聽的原因；代價是大聲時比線性差一點。")


if __name__ == "__main__":
    main()
