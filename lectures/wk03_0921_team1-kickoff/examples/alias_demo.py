#!/usr/bin/env python3
"""alias_demo.py — 取樣率不夠會怎樣（aliasing）。只用標準函式庫。

用法：python3 alias_demo.py [取樣率fs=8000]

以 fs = 8000 Hz 取樣 3000 Hz 與 5000 Hz 兩個弦波（2023 講義 Chapter 2 的例子）：
  3000 Hz < fs/2 = 4000 Hz，沒問題；
  5000 Hz > fs/2，取樣後的數列和 3000 Hz 的「一模一樣、只差正負號」——取樣之後再也分不出來了。
寫出 tone_3000.wav、tone_5000_aliased.wav，播放起來音高相同。
再用 fs 掃過一串頻率，印出「實際聽到的頻率」：超過 fs/2 就折回來。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第二節 2.2「Sampling：取樣率、Nyquist、aliasing」。
# 怎麼跑：  在 examples/ 裡打 python3 alias_demo.py（Windows 打 python alias_demo.py）。
# 會看到：  (1) 一張 n = 0..9 的表：x1[n]、x2[n] 兩欄大小相同、正負號相反，第三欄 x1 + x2 全是 0；
#           (2) 一張「原始頻率 → 取樣後聽到的頻率」的表，超過 fs/2 的列會標上 ← aliasing；
#           (3) 在目前的資料夾寫出 tone_3000.wav 與 tone_5000_aliased.wav，兩個檔聽起來音高一樣。
# 注意：    「x2[n] = −x1[n]」只在 fs = 8000 時成立（因為 3000 + 5000 剛好等於 fs）。
#           自己換別的 fs 來跑時，第一張表與那句說明就不再對得上，要看的是第二張表。
# ──────────────────────────────────────────────────────────────────────────────────────
import math
import struct
import sys
import wave

# Windows 上把輸出導到檔案或管線（例如 > out.txt）時，Python 會改用 cp950 編碼，印不出 −、²、≤ 這些符號而當掉；
# 這裡強制用 UTF-8。直接在終端機執行時本來就沒問題。
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

# sys.argv 是命令列上的字串清單：argv[0] 是程式名稱，argv[1] 起才是使用者給的參數。
# 「A if 條件 else B」是 Python 的條件運算式（相當於 C 的 條件 ? A : B）：有給參數就用它，沒給就用 8000。
# 命令列參數一律是字串，所以要用 int() 轉成整數。
FS = int(sys.argv[1]) if len(sys.argv) > 1 else 8000


# 產生一段頻率為 f 的弦波，回傳一個 list，裡面是一個一個的 sample（浮點數，範圍 −1 到 1）。
#
# 這就是講義 2.2 的 x[n] = x_c(n / fs)：連續的弦波是 sin(2π·f·t)，
#   2π·f 是角頻率（每秒轉過幾 radians：一個週期是 2π，每秒有 f 個週期）；
#   第 n 個 sample 的時刻是 t = n / fs 秒（每隔 1/fs 秒取一次）；
# 兩個合起來就是 sin(2π·f·n/fs)。電腦沒辦法存「連續」的波形，只能存這些取樣點。
#
# 寫法說明：
#   [ 運算式 for n in range(個數) ] 叫 list comprehension，意思是「n 從 0 跑到 個數−1，
#   每次把運算式的值收進 list」，等於一個 for 迴圈加上 append，只是寫成一行。
#   sample 的個數 = 取樣率 × 秒數；1.5 秒乘出來是浮點數，range() 只收整數，所以包一層 int()。
def tone(f, seconds=1.5):
    return [math.sin(2 * math.pi * f * n / FS) for n in range(int(FS * seconds))]


# 把一串 −1 到 1 的浮點數存成 16-bit、單聲道的 WAV 檔，才能用播放器聽。
#
#   with ... as w:  with 區塊結束時會自動把檔案關掉（即使中途出錯也會），不必自己記得 close()。
#                   wave 模組在關檔時才把檔頭裡的長度欄位補上，所以沒關好檔案會是壞的。
#   "wb"            w = 寫入、b = binary（二進位）。WAV 不是文字檔，一定要用 binary 模式。
#   setnchannels(1) 單聲道；setsampwidth(2) 每個 sample 2 bytes（= 16 bits）；setframerate(FS) 取樣率。
#                   這三項就是 WAV 檔頭裡的 NumChannels、BitsPerSample、SampleRate（見講義 2.6）。
#
# 最長的那一行由裡往外讀：
#   0.6 * 32767 * v       16-bit 有號整數的範圍是 −32768 到 32767，所以把 ±1.0 放大到 ±32767 才存得進去；
#                         再乘 0.6 只是把音量調小一點，免得用耳機聽時太大聲。
#   round(...)            四捨五入成整數——這一步其實就是「量化」（講義 2.3）。
#   struct.pack("<h", 整數)  把一個整數變成檔案裡的 bytes。格式字串 "<h" 要分兩個字看：
#                         "<" = little-endian（低位的 byte 放前面，WAV 規定的順序，見講義 2.6）；
#                         "h" = 有號的 16-bit 整數（C 的 short），佔 2 bytes。例如 1000 會變成 b"\xe8\x03"。
#   ( ... for v in x )    和 list comprehension 很像，但外面沒有方括號，叫 generator expression：
#                         不先建好整個 list，而是 join 要一個才算一個，比較省記憶體。
#   b"".join(...)         把許多小段 bytes 接成一大段。b"" 是「空的 bytes」；
#                         Python 把 bytes（原始的位元組）和 str（文字）分成兩種型別，寫進檔案的必須是 bytes。
#
#   f"寫出 {name}"        字串前面加 f 叫 f-string：大括號裡的變數或運算式會被換成它的值。
def save(name, x):
    with wave.open(name, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(b"".join(struct.pack("<h", round(0.6 * 32767 * v)) for v in x))
    print(f"寫出 {name}")


def apparent(f):
    """以 fs 取樣後，頻率 f 的弦波看起來是多少 Hz：折到 [0, fs/2]"""
    # 為什麼分兩步（講義 2.2 末段「以 fs/2 為軸折回來」）：
    #   1. 取樣之後，f 與 f + fs、f + 2fs … 的 sample 完全相同（sin 多轉整數圈，值不變），
    #      所以先用 % 取餘數，把 f 拉回 0 到 fs 之間。
    #   2. 落在 fs/2 到 fs 之間的，看起來和 fs − r 一樣（只差正負號，耳朵聽不出來），所以再折一次。
    # 例：fs = 8000 時，5000 → 8000 − 5000 = 3000；8500 → 8500 % 8000 = 500；12000 → 4000。
    r = f % FS
    return r if r <= FS / 2 else FS - r


# ── 實驗一：把 3000 Hz 與 5000 Hz 的前 10 個 sample 並排印出來 ──
# 「a, b = 值1, 值2」可以一次指定兩個變數。
x1, x2 = tone(3000), tone(5000)
# FS // 2 的 // 是整數除法（結果是整數 4000，不是 4000.0）。
print(f"fs = {FS} Hz，Nyquist frequency = fs/2 = {FS // 2} Hz\n")
print("   n    x1[n] = sin(2π·3000·n/fs)    x2[n] = sin(2π·5000·n/fs)     x1 + x2")
# f-string 的大括號裡，冒號後面是「格式規格」：
#   {n:>2d}        d = 十進位整數、寬度 2、> = 靠右對齊；
#   {x1[n]:>12.6f} f = 固定小數位的浮點數、寬度 12、小數 6 位、靠右。
# 這樣每一列的欄位才會對齊成一張表。
# 表裡偶爾出現 -0.000000 不是錯：浮點數有誤差，算出來是 −0.0000000000000003 之類的極小值，印 6 位小數就長這樣。
for n in range(10):
    print(f"  {n:>2d}   {x1[n]:>12.6f}                 {x2[n]:>12.6f}              {x1[n] + x2[n]:>10.6f}")
print("\n每一個 n 都有 x2[n] = −x1[n]：5000 Hz 的弦波取樣後，和 3000 Hz（反相）完全相同。\n")

# ── 實驗二：掃過一串頻率，看哪些會被「折回來」 ──
# 小括號包起來、用逗號分開的一串值叫 tuple（不能修改的 list），這裡只是拿來當「要試的頻率清單」。
# 留意 3900／4100、1000／7000、500／8500 這幾組：取樣之後聽到的頻率相同。
print("   原始頻率 f     取樣後聽到的頻率")
for f in (500, 1000, 3000, 3900, 4000, 4100, 5000, 7000, 7900, 8000, 8500, 12000):
    # 超過 Nyquist frequency（fs/2）的才標記；剛好等於 fs/2 的不標。
    mark = "  ← aliasing" if f > FS / 2 else ""
    # {apparent(f):>7.0f}：.0f 表示小數 0 位，把可能是浮點數的結果印成整數的樣子。
    print(f"   {f:>7d} Hz      {apparent(f):>7.0f} Hz{mark}")
print()

# ── 實驗三：寫成 WAV，用耳朵確認兩個檔音高相同 ──
save("tone_3000.wav", x1)
save("tone_5000_aliased.wav", x2)
