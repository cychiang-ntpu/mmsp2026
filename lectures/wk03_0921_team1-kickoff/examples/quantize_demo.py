#!/usr/bin/env python3
"""quantize_demo.py — 量化（bit depth）與 SQNR。只用標準函式庫。

用法：python3 quantize_demo.py [振幅A=1.0] [頻率f=440] [取樣率fs=8000]

做三件事：
  1. 用講義的均勻量化器 Q(x) = Δ·floor(x/Δ + 1/2) 把弦波量化成 n = 2..16 bits，
     照定義量 SQNR = 10·log10( Σx² / Σe² )，和兩條公式對照：
        6.02n          投影片的公式：最大訊號振幅 ÷ 最大量化誤差 = 2^(n-1) ÷ (1/2)
        6.02n + 1.76   滿刻度弦波、以「功率」計算的公式
  2. 印出 3-bit 量化的前 12 個 sample，看量化誤差長什麼樣子
  3. 寫出 out_16bit.wav、out_8bit.wav、out_4bit.wav（都存成 16-bit 才能播放，但只用到 2^n 個階），用耳朵聽量化雜訊
MP2 要你用 C 做同一件事（產生波形、量化、算 SQNR、寫 WAV）；這支程式只是觀念示範，參數與輸出格式都和 MP2 不同。
"""
import math
import struct
import sys
import wave

A = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
F = float(sys.argv[2]) if len(sys.argv) > 2 else 440.0
FS = int(sys.argv[3]) if len(sys.argv) > 3 else 8000
N = FS * 2                                                     # 2 秒


def quantize(x, bits):
    """x 在 [-1, 1)。n bits → 2^n 個階，階距 Δ = 2 / 2^n；結果限制在可表示的範圍內。"""
    delta = 2.0 / (1 << bits)
    q = delta * math.floor(x / delta + 0.5)
    return max(-1.0, min(1.0 - delta, q))


x = [A * math.sin(2 * math.pi * F * n / FS) for n in range(N)]
p_signal = sum(v * v for v in x) / N

print(f"弦波 A={A} f={F:g} Hz fs={FS} Hz，{N} 個 sample；訊號功率 P = {p_signal:.6f}（理論值 A²/2 = {A * A / 2:.6f}）\n")
print(" bits   階數 L=2^n      階距 Δ      量到的 SQNR    6.02n    6.02n+1.76+20log10(A)")
for bits in range(2, 17):
    e = [v - quantize(v, bits) for v in x]
    p_noise = sum(v * v for v in e) / N
    sqnr = 10 * math.log10(p_signal / p_noise)
    theory = 6.02 * bits + 1.76 + 20 * math.log10(A)
    print(f"  {bits:>2d}   {1 << bits:>10,d}   {2.0 / (1 << bits):>10.7f}   {sqnr:>10.2f} dB   {6.02 * bits:>6.2f}   {theory:>10.2f}")

print("\n3-bit 量化（8 個階，Δ = 0.25）的前 12 個 sample：")
print("    n      x[n]    Q(x[n])     e[n] = x − Q(x)")
for n in range(12):
    q = quantize(x[n], 3)
    print(f"  {n:>3d}  {x[n]:>8.4f}  {q:>8.4f}  {x[n] - q:>9.4f}")

for bits in (16, 8, 4):
    name = f"out_{bits}bit.wav"
    with wave.open(name, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(b"".join(struct.pack("<h", max(-32768, min(32767, round(quantize(v, bits) * 32768)))) for v in x))
    print(f"寫出 {name}")
