#!/usr/bin/env python3
"""alias_demo.py — 取樣率不夠會怎樣（aliasing）。只用標準函式庫。

用法：python3 alias_demo.py [取樣率fs=8000]

以 fs = 8000 Hz 取樣 3000 Hz 與 5000 Hz 兩個弦波（2023 講義 Chapter 2 的例子）：
  3000 Hz < fs/2 = 4000 Hz，沒問題；
  5000 Hz > fs/2，取樣後的數列和 3000 Hz 的「一模一樣、只差正負號」——取樣之後再也分不出來了。
寫出 tone_3000.wav、tone_5000_aliased.wav，播放起來音高相同。
再用 fs 掃過一串頻率，印出「實際聽到的頻率」：超過 fs/2 就折回來。
"""
import math
import struct
import sys
import wave

FS = int(sys.argv[1]) if len(sys.argv) > 1 else 8000


def tone(f, seconds=1.5):
    return [math.sin(2 * math.pi * f * n / FS) for n in range(int(FS * seconds))]


def save(name, x):
    with wave.open(name, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(b"".join(struct.pack("<h", round(0.6 * 32767 * v)) for v in x))
    print(f"寫出 {name}")


def apparent(f):
    """以 fs 取樣後，頻率 f 的弦波看起來是多少 Hz：折到 [0, fs/2]"""
    r = f % FS
    return r if r <= FS / 2 else FS - r


x1, x2 = tone(3000), tone(5000)
print(f"fs = {FS} Hz，Nyquist frequency = fs/2 = {FS // 2} Hz\n")
print("   n    x1[n] = sin(2π·3000·n/fs)    x2[n] = sin(2π·5000·n/fs)     x1 + x2")
for n in range(10):
    print(f"  {n:>2d}   {x1[n]:>12.6f}                 {x2[n]:>12.6f}              {x1[n] + x2[n]:>10.6f}")
print("\n每一個 n 都有 x2[n] = −x1[n]：5000 Hz 的弦波取樣後，和 3000 Hz（反相）完全相同。\n")

print("   原始頻率 f     取樣後聽到的頻率")
for f in (500, 1000, 3000, 3900, 4000, 4100, 5000, 7000, 7900, 8000, 8500, 12000):
    mark = "  ← aliasing" if f > FS / 2 else ""
    print(f"   {f:>7d} Hz      {apparent(f):>7.0f} Hz{mark}")
print()
save("tone_3000.wav", x1)
save("tone_5000_aliased.wav", x2)
