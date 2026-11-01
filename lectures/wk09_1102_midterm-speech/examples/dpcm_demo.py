#!/usr/bin/env python3
"""dpcm_demo.py — 預測編碼（DPCM）：語音相鄰的 sample 很像，所以「只送差值」就省很多 bits

用法：python3 dpcm_demo.py <16-bit 單聲道 WAV>

做的事（對應講義 2.3）：
  1. 讀 WAV，算 sample 本身的功率，和「用上一個 sample 當預測」的殘差 e[n] = x[n] − x[n−1] 的功率。
     兩者的比值（dB）叫 prediction gain：殘差小多少，就能少用多少 bits（每 6 dB 約省 1 bit）。
  2. 再試 2 階預測 x̂[n] = 2x[n−1] − x[n−2]（直線外插）看能不能更好。
  3. 用 Huffman 的角度看：對 x 與 e 各算熵（以 16-bit 值為符號），熵小代表 Huffman 能壓得更小——
     這就是 Team 1 的「符號怎麼定」在聲音上的延伸：先預測再編碼，是所有語音 codec 的第一步。
  4. 示範「殘差量化成 4 bits」的 DPCM 編碼／解碼（解碼端用同樣的預測器把訊號加回來），量 SQNR。
只用標準函式庫。
"""
import collections
import math
import struct
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def read_wav(path):
    d = open(path, "rb").read()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise SystemExit("不是 WAV")
    pos, pcm, fs = 12, b"", 0
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack("<I", d[pos + 4:pos + 8])[0]
        body = d[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt, ch, fs, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if fmt != 1 or ch != 1 or bits != 16:
                raise SystemExit("只收 16-bit 單聲道 PCM")
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    return fs, list(struct.unpack("<%dh" % (len(pcm) // 2), pcm[:len(pcm) // 2 * 2]))


def power_db(v):
    return 10 * math.log10(sum(a * a for a in v) / len(v) + 1e-12)


def entropy_bits(v):
    """以整數值為符號的熵（bits／sample）——Huffman 能做到的平均碼長下限。"""
    c = collections.Counter(v)
    n = len(v)
    return -sum(k / n * math.log2(k / n) for k in c.values())


def dpcm_4bit(x, step):
    """1 階 DPCM，殘差量化成 4 bits（16 階，階距 step）。編碼端與解碼端都用「重建後」的 x̂ 做預測，
    這樣兩邊的預測一模一樣，誤差不會累積（這是 DPCM 最重要的一個細節）。"""
    recon, prev = [], 0
    for v in x:
        e = v - prev                                      # 殘差
        q = max(-8, min(7, round(e / step)))              # 4 bits：-8..7
        prev = max(-32768, min(32767, prev + q * step))   # 解碼端能算到的重建值
        recon.append(prev)
    return recon


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    fs, x = read_wav(sys.argv[1])
    e1 = [x[n] - x[n - 1] for n in range(1, len(x))]
    e2 = [x[n] - (2 * x[n - 1] - x[n - 2]) for n in range(2, len(x))]
    px, p1, p2 = power_db(x), power_db(e1), power_db(e2)
    print(f"{sys.argv[1]}：{fs} Hz，{len(x)} 個 sample")
    print(f"  sample 本身的功率          {px:6.1f} dB      熵 {entropy_bits(x):5.2f} bits／sample")
    print(f"  1 階預測殘差 x[n]−x[n−1]   {p1:6.1f} dB      熵 {entropy_bits(e1):5.2f} bits／sample   prediction gain {px - p1:4.1f} dB")
    print(f"  2 階預測殘差（直線外插）    {p2:6.1f} dB      熵 {entropy_bits(e2):5.2f} bits／sample   prediction gain {px - p2:4.1f} dB")
    print("  讀法：殘差的功率每少 6 dB，同樣的 SQNR 可以少用 1 bit；熵少多少，Huffman 就能多壓多少。")
    print()
    print("  把 1 階殘差量化成 4 bits（DPCM），試幾種階距：")
    for step in (64, 128, 256, 512):
        r = dpcm_4bit(x, step)
        pn = sum((a - b) ** 2 for a, b in zip(x, r)) / len(x)
        print(f"    階距 {step:4d}：SQNR {10 * math.log10(10 ** (px / 10) / pn):5.1f} dB   （4 bits／sample = {4 * fs / 1000:.0f} kb/s，原本 16 bits = {16 * fs / 1000:.0f} kb/s）")
    print("  讀法：階距太小會跟不上大的跳動（slope overload），太大會在小聲處粗糙（granular noise）；")
    print("        真的 ADPCM（G.726）讓階距隨訊號自動調整，就是為了同時顧到兩邊。")


if __name__ == "__main__":
    main()
