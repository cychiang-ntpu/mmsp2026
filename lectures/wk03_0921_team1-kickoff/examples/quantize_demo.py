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
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第二節 2.3「Quantization：bit depth 與量化誤差」、2.4「分貝、SNR 與 SQNR」。
# 怎麼跑：  python3 quantize_demo.py          （Windows 打 python；三個參數都可以省略）
#           python3 quantize_demo.py 0.5      振幅減半再跑一次，每一列的 SQNR 大約少 6 dB（講義 2.4）
# 會看到：  (1) bits = 2..16 的一張表：量到的 SQNR 每多 1 bit 大約多 6 dB，
#               6 bits 以上和最右欄 6.02n + 1.76 + 20·log10(A) 很接近（多數列差不到 1 dB）；
#           (2) 3-bit 量化的前 12 個 sample：n = 4、5、6 三列的誤差超過 Δ/2 = 0.125，那是 clipping；
#           (3) 寫出三個 WAV，依 16 → 8 → 4 bits 的順序播放，4-bit 的沙沙聲就是量化雜訊。
# ──────────────────────────────────────────────────────────────────────────────────────
import math
import struct
import sys
import wave

# Windows 上把輸出導到檔案或管線（例如 > out.txt）時，Python 會改用 cp950 編碼，印不出 −、²、≤ 這些符號而當掉；
# 這裡強制用 UTF-8。直接在終端機執行時本來就沒問題。
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

# 命令列參數（sys.argv[1]、[2]、[3]）都是字串，要自己轉成數字；沒給就用預設值。
# 「A if 條件 else B」是條件運算式：條件成立取 A，否則取 B。
A = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
F = float(sys.argv[2]) if len(sys.argv) > 2 else 440.0
FS = int(sys.argv[3]) if len(sys.argv) > 3 else 8000
N = FS * 2                                                     # 2 秒


def quantize(x, bits):
    """x 在 [-1, 1)。n bits → 2^n 個階，階距 Δ = 2 / 2^n；結果限制在可表示的範圍內。"""
    # 這個函式把「−1 到 1 之間的任意實數」歸到 2^bits 個等級中最近的一個，回傳的仍是浮點數（那個等級代表的值）。
    #
    # 第 1 行：1 << bits 是把 1 左移 bits 位，也就是 2^bits（整數運算，比 2 ** bits 更接近 C 的寫法）。
    #          −1 到 +1 的總寬度是 2，平分成 2^bits 格，每格的寬度就是階距 Δ。3 bits → Δ = 2/8 = 0.25。
    # 第 2 行：講義 2.3 的均勻量化器 Q(x) = Δ·⌊x/Δ + 1/2⌋。
    #          x/Δ 是「x 等於幾個 Δ」；加 1/2 再 floor（無條件捨去）就是四捨五入到最近的整數 k；
    #          最後乘回 Δ，得到第 k 個等級的值。例：x = 0.6374、Δ = 0.25 → 2.55 + 0.5 → floor = 3 → 0.75。
    # 第 3 行：bits 個 bit 只放得下 2^bits 個等級：−1、−1+Δ、…、0、…、1−Δ。注意最大的是 1 − Δ，不是 1
    #          （二補數也是這樣：16 bits 是 −32768 到 +32767，正的那一邊少一個）。
    #          所以 x 接近 +1 時，第 2 行會算出 1.0 這個「不存在的等級」，只能用 min() 壓回 1 − Δ。
    #          這就是講義 2.3 的 clipping（overload）：3-bit 時 0.98 被壓到 0.75，誤差 0.23，遠超過 Δ/2。
    #          max(下限, min(上限, q)) 是「把 q 夾在上下限之間」的慣用寫法。
    delta = 2.0 / (1 << bits)
    q = delta * math.floor(x / delta + 0.5)
    return max(-1.0, min(1.0 - delta, q))


# ── 產生要被量化的訊號：振幅 A、頻率 F 的弦波（講義 2.2 的 x[n] = A·sin(2π·f·n/fs)）──
# 2π·F 是角頻率（radians／秒），n / FS 是第 n 個 sample 的時刻（秒）。
# [ 運算式 for n in range(N) ] 是 list comprehension：n 從 0 到 N−1，把每次的運算式收成一個 list。
# 這些浮點數有 53 bits 的精確度，在這支程式裡扮演「量化之前的真值」（講義 2.8 表格的最後一列）。
x = [A * math.sin(2 * math.pi * F * n / FS) for n in range(N)]

# 訊號功率 P = (1/N)·Σ x²[n]，「平方的平均」（講義 2.4 (b)）。弦波的理論值是 A²/2。
# sum( 運算式 for v in x ) 裡面是 generator expression：和 list comprehension 一樣的寫法，
# 但不加方括號、不先建 list，sum 要一個才算一個。
p_signal = sum(v * v for v in x) / N

# f-string（字串前加 f）：大括號裡的運算式會被換成值；冒號後面是格式，
# {F:g} 用最短的寫法印浮點數（440.0 印成 440），{p_signal:.6f} 印到小數 6 位。
print(f"弦波 A={A} f={F:g} Hz fs={FS} Hz，{N} 個 sample；訊號功率 P = {p_signal:.6f}（理論值 A²/2 = {A * A / 2:.6f}）\n")

# ── 第 1 件事：bit depth 從 2 到 16，照定義「實際量」SQNR，和公式比 ──
print(" bits   階數 L=2^n      階距 Δ      量到的 SQNR    6.02n    6.02n+1.76+20log10(A)")
# range(2, 17) 是 2, 3, …, 16：range 的結尾不包含在內。
for bits in range(2, 17):
    # 量化誤差 e[n] = x[n] − Q(x[n])；程式裡 x 與 Q(x) 都在手上，所以誤差算得出來（真實的錄音做不到）。
    e = [v - quantize(v, bits) for v in x]
    # 雜訊功率 P₀ = (1/N)·Σ e²[n]，和訊號功率同一個算法。
    p_noise = sum(v * v for v in e) / N
    # SQNR = 10·log10(P / P₀)：功率的比取對數再乘 10 就是 dB（講義 2.4）。
    # 注意分子是「量化前」的訊號功率、分母是誤差的功率；去年 MP2 常見的錯就是這兩個放錯。
    sqnr = 10 * math.log10(p_signal / p_noise)
    # 理論公式：6.02 來自 20·log10(2)，每多 1 bit 等級加倍；1.76 是弦波功率 A²/2 與均勻分布雜訊功率 Δ²/12 的常數；
    # 20·log10(A) 是「音量沒有開到滿刻度」的損失：A = 0.5 時是 −6.02 dB。
    theory = 6.02 * bits + 1.76 + 20 * math.log10(A)
    # 格式規格：{bits:>2d} 寬 2 靠右的整數；{1 << bits:>10,d} 的逗號會加千分位（65,536）；
    # {sqnr:>10.2f} 寬 10、小數 2 位的浮點數。
    print(f"  {bits:>2d}   {1 << bits:>10,d}   {2.0 / (1 << bits):>10.7f}   {sqnr:>10.2f} dB   {6.02 * bits:>6.2f}   {theory:>10.2f}")

# ── 第 2 件事：只用 3 bits（8 個等級：−1.00、−0.75、…、+0.50、+0.75），逐個 sample 看誤差 ──
# 正常情況 |e[n]| ≤ Δ/2 = 0.125；x[n] 超過 0.875 時沒有夠大的等級可用，誤差就超過這個上限（clipping）。
print("\n3-bit 量化（8 個階，Δ = 0.25）的前 12 個 sample：")
print("    n      x[n]    Q(x[n])     e[n] = x − Q(x)")
for n in range(12):
    q = quantize(x[n], 3)
    print(f"  {n:>3d}  {x[n]:>8.4f}  {q:>8.4f}  {x[n] - q:>9.4f}")

# ── 第 3 件事：把 16、8、4 bits 量化後的波形各寫成一個 WAV，用耳朵聽 ──
# 三個檔案「都」存成 16-bit PCM（播放器不認得 4-bit 的 WAV），差別只在裡面出現的值有幾種：
# 4-bit 的那個檔雖然每個 sample 佔 16 bits，實際上只會出現 16 種值，所以聽起來就是 4-bit 的品質。
for bits in (16, 8, 4):
    name = f"out_{bits}bit.wav"
    # with 區塊結束時自動關檔；"wb" = 以 binary 模式寫入（WAV 不是文字檔）。
    # setnchannels(1) 單聲道、setsampwidth(2) 每個 sample 2 bytes、setframerate(FS) 取樣率：
    # wave 模組會依這三項幫我們寫好講義 2.6 的 44 bytes 檔頭（MP2 要你在 C 裡自己寫）。
    with wave.open(name, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        # 下面這一行由裡往外讀：
        #   quantize(v, bits) * 32768   量化後的值在 −1 到 1−Δ 之間，乘上 2^15 = 32768 變成 16-bit 整數的刻度
        #                               （講義 2.3「乘上 2^(M−1)」的那一種慣例；alias_demo.py 與 MP2 用的是另一種：乘 32767）。
        #   round(...)                  轉成整數。這裡的值本來就是整數倍，round 只是去掉浮點數的小誤差。
        #   max(-32768, min(32767, …))  夾在 16-bit 有號整數的範圍內。quantize() 已經把上限壓在 1−Δ，
        #                               所以這裡其實不會真的夾到，只是保險：超出範圍的值交給 struct.pack 會直接丟例外。
        #   struct.pack("<h", 整數)     把整數變成 2 個 bytes："<" = little-endian（低位 byte 在前，WAV 的規定），
        #                               "h" = 有號 16-bit 整數（C 的 short）。
        #   b"".join( … for v in x )    把每個 sample 的 2 bytes 接成一整段 bytes。b"" 是空的 bytes；
        #                               bytes 是「原始位元組」，和文字的 str 是不同型別，寫進二進位檔要用 bytes。
        w.writeframes(b"".join(struct.pack("<h", max(-32768, min(32767, round(quantize(v, bits) * 32768)))) for v in x))
    print(f"寫出 {name}")
