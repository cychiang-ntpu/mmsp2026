#!/usr/bin/env python3
"""entropy.py — entropy.c 的 Python 對照版，輸出逐 byte 相同。

用法：python3 entropy.py <檔案>
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第一節 1.3「熵：壓縮的極限在哪裡」與 1.7「『符號』是什麼，比演算法重要」。
# 示範什麼：同一個檔案，把「符號」定成 byte、或定成 UTF-8 字元，算出來的熵與理想壓縮率不一樣。
# 怎麼跑：  python3 entropy.py ../../wk02_0914_text-utf8/data/sample_zh_en.txt    （Windows 打 python）
#           或 make check（Windows：mingw32-make check）：同一個檔案各跑 C 版與 Python 版，再比對兩份輸出。
# 會看到：  file: …  (162 bytes)
#           [byte]  N=162  K=83  H=6.0120 bits/symbol  ideal=75.15%
#           [char]  N=105  K=64  H=5.6346 bits/symbol  ideal=45.65%
#           接著列出出現最多的 10 個字元。N = 符號總數、K = 符號種類數、H = 熵、
#           ideal = 「每個符號都剛好用 H bits」時，壓縮後 ÷ 原始檔案大小（還沒算 codebook，見講義 1.7）。
# 為什麼要有 Python 版：本課程的模式是「C 實作、Python 對答案」。兩版的輸出要一個 byte 都不差，
#           所以下面有幾個地方是為了「和 C 版完全一致」才那樣寫的，註解裡會特別說明。
# ──────────────────────────────────────────────────────────────────────────────────────
import collections
import math
import sys

# 最後要列出「出現次數最多的前幾名」；寫成常數，要改只需要改這一個地方。
TOP = 10


# 對一張「符號 → 次數」的表算熵，回傳一行報告文字。
#   label       印在中括號裡的名稱（"byte" 或 "char"）
#   counter     collections.Counter：用起來像 dict，key 是符號、value 是它出現的次數
#   file_bytes  原始檔案的 bytes 數，用來算理想壓縮率
def report(label, counter, file_bytes):
    # 符號總數 N = 所有次數相加。
    n = sum(counter.values())
    h = 0.0
    # 熵 H = Σ p·log2(1/p)。因為 log2(1/p) = −log2(p)，所以每一項寫成「減掉 p·log2(p)」是同一件事。
    # p = 次數 ÷ 總數；log2(1/p) 是這個符號「理想上該用幾 bits」，再乘上 p 做加權平均（講義 1.3）。
    # 為什麼要 sorted：浮點數的加法有捨入誤差，相加的「順序」不同，最後幾位可能不同；
    # 照 C 版同樣的順序加，兩邊印出來的數字才保證一樣。
    for sym in sorted(counter):                       # 與 C 版相同的累加順序（符號值由小到大）
        p = counter[sym] / n
        h -= p * math.log2(p)
    # 理想壓縮率（%）= 壓縮後 ÷ 原始 = (H bits × N 個符號 ÷ 8 → bytes) ÷ 檔案的 bytes 數 × 100。
    # 後面的 if file_bytes else 0.0：空檔案時 file_bytes 是 0，整數 0 在 if 裡算 False，直接給 0.0 以免除以零。
    ideal = 100.0 * h * n / 8.0 / file_bytes if file_bytes else 0.0
    # f-string 的格式規格：{h:.4f} 小數 4 位、{ideal:.2f} 小數 2 位，要和 C 版 printf 的 %.4f、%.2f 一致。
    # 結尾自己加 "\n"，因為最後不是用 print，而是把所有行接起來一次寫出去（見 main 的最後一行）。
    return f"[{label}]  N={n}  K={len(counter)}  H={h:.4f} bits/symbol  ideal={ideal:.2f}%\n"


def main():
    # 參數個數不對：sys.exit(字串) 把字串印到 stderr，並以「失敗」的結束碼離開。
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} <file>")

    # "rb" = binary 模式：讀到的 data 是 bytes（原始位元組），不做任何文字解碼或換行轉換。
    data = open(sys.argv[1], "rb").read()

    # ── 第一種定義：以 byte 為符號 ──
    # 走訪 bytes 時，每個元素是一個 0–255 的整數；Counter(data) 就數出每種 byte 值各出現幾次。
    # out 是一個 list，先把要輸出的每一行收集起來。
    out = [f"file: {sys.argv[1]}  ({len(data)} bytes)\n", report("byte", collections.Counter(data), len(data))]

    # ── 第二種定義：以 UTF-8 字元為符號 ──
    # 先試著把 bytes 解碼成文字（str）。不是每個檔案都是合法的 UTF-8（例如 WAV），解不開時會丟出例外；
    # try / except / else：try 裡出錯就跑 except；「沒有」出錯才跑 else。
    try:
        text = data.decode("utf-8")                   # 嚴格解碼：overlong、代理區、超出範圍都會丟例外
    except UnicodeDecodeError:
        out.append("[char]  not valid UTF-8\n")
    else:
        # ord(ch) 把一個字元換成它的 Unicode code point（整數），例如 ord("多") = 0x591A。
        # 用整數當 key，排序時才會和 C 版一樣「依 code point 由小到大」。
        # Counter( … for ch in text ) 裡面是 generator expression：一個一個字元算出來交給 Counter 去數。
        # 這時「多」算 1 個符號，不是 E5 A4 9A 三個——所以 N 比較小、熵也不同。
        c = collections.Counter(ord(ch) for ch in text)
        # 注意第三個參數仍然是檔案的 bytes 數：壓縮率的分母永遠是原始檔案大小。
        out.append(report("char", c, len(data)))
        out.append(f"top {TOP} characters:\n")
        n = sum(c.values())
        # 依次數由大到小取前 TOP 名：
        #   c.items()          一對一對的（code point, 次數）
        #   key=lambda kv: …   lambda 是「沒有名字的小函式」；kv 是其中一對，kv[0] 是 code point、kv[1] 是次數
        #   (-kv[1], kv[0])    排序依據是一個 tuple，tuple 比大小是先比第一項、相同再比第二項：
        #                      次數加負號 → 次數大的排前面；次數相同（平手）→ code point 小的排前面。
        #                      平手規則寫清楚，C 版與 Python 版的順序才會一樣。
        #   [:TOP]             slicing：只取排序後的前 TOP 個（不足 TOP 個就有幾個取幾個）。
        for cp, cnt in sorted(c.items(), key=lambda kv: (-kv[1], kv[0]))[:TOP]:
            p = cnt / n
            # 控制字元（U+0000–U+001F 與 U+007F，例如換行、Tab）印出來會把畫面弄亂，改印 "(ctrl)"。
            # chr(cp) 是 ord() 的反向：由 code point 得到字元。
            shown = "(ctrl)" if cp < 0x20 or cp == 0x7F else chr(cp)
            # {cp:04X}：X = 大寫十六進位、寬度 4、不足的前面補 0，印成 U+591A 這種慣用寫法。
            # -log2(p) 就是 log2(1/p)：這個字元理想上該用幾 bits；越少見的字元數字越大。
            out.append(f"  U+{cp:04X}  {shown}  count={cnt}  p={p:.4f}  -log2(p)={-math.log2(p):.2f}\n")

    # 為什麼不用 print：要和 C 版「逐 byte 相同」。
    #   "".join(out)        把 list 裡的每一行接成一個大字串；
    #   .encode("utf-8")    str → bytes，明確指定用 UTF-8（不看作業系統的預設編碼，Windows 的預設常常不是 UTF-8）；
    #   sys.stdout.buffer   stdout 底下的 binary 介面：寫什麼 bytes 就輸出什麼 bytes，
    #                       不會像文字模式那樣在 Windows 上把 "\n" 換成 "\r\n"。
    sys.stdout.buffer.write("".join(out).encode("utf-8"))


# 直接執行這個檔案時才呼叫 main()；被別的程式 import 時不會自動執行。
if __name__ == "__main__":
    main()
