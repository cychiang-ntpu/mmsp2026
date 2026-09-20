#!/usr/bin/env python3
"""huffman_demo.py — 把 Huffman 建樹的每一步印出來（上課手算對答案用）。

用法：
  python3 huffman_demo.py                       # 預設字串 ABRACADABRA
  python3 huffman_demo.py "多媒體多媒多"          # 符號是「字元」，不是 byte
  python3 huffman_demo.py --bytes "多媒體多"      # 改以 byte 為符號，比較看看
  python3 huffman_demo.py --freq "black:100,white:100,yellow:20,blue:20,orange:5,red:5,purple:3,green:3"
                                                # 直接給頻率表（講義 1.3 的 8 色影像例子）

平手規則：頻率相同時，先取「比較早進佇列」的節點（葉依符號排序在前，新合併的節點排在後）。
平手規則不同，每個符號的 code 長度可能不同，但平均碼長 L 一定相同；Huffman code 不是唯一的。
這支程式是教學示範，不做檔案與位元打包；完整的編解碼見 samples_2025-python/mini_project_4。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第一節 1.3（熵）、1.6（Huffman：由下往上合併）、1.7（符號的定義與 codebook 的成本）。
# 怎麼跑：  見上面的用法（Windows 把 python3 換成 python）。
# 會看到五段輸出：
#   1. 每種符號的次數、機率 p、log2(1/p)，以及熵 H
#   2. 建樹的每一步：「誰[次數] + 誰[次數] → [相加]」，K 種符號會有 K−1 步
#   3. 每個符號的 code 長度與 code
#   4. 編碼後的位元串與打包成的 bytes（給頻率表時沒有原始序列，這一段不印）
#   5. 平均碼長 L、H ≤ L < H+1 的檢查、編碼效率、與定長編碼相比的壓縮率
#   預設的 ABRACADABRA：H = 2.0404、L = 23 ÷ 11 = 2.0909、位元流 3 bytes；
#   8 色影像的頻率表：H = 2.0063、L = 536 ÷ 256 = 2.0938，和講義 1.6 的數字相同。
# 和 huffman_trace.c 的差別：C 版用「節點陣列＋排序好的佇列」，把記憶體的變化印給你看；這支用 Python 的高階工具
#   （Counter 統計、heapq 取最小）寫得很短，而且只記每個符號的「code 長度」，沒有真的把樹存下來。
# 注意：第 3 段印的是 canonical code，和講義 1.6 表格裡「沿著樹走出來」的 code 不一定一樣（例如 black 是 10 而不是 11），
#   但每個符號的長度相同，所以 L 相同——正好示範「Huffman code 不是唯一的」。
# ──────────────────────────────────────────────────────────────────────────────────────
import collections
import heapq
import math
import sys

# Windows 上把輸出導到檔案或管線（例如 > out.txt）時，Python 會改用 cp950 編碼，印不出 −、²、≤ 這些符號而當掉；
# 這裡強制用 UTF-8。直接在終端機執行時本來就沒問題。
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


# 把一個符號變成「印得出來、看得見」的字串。
#   以 byte 為符號時，符號是 0–255 的整數：印成兩位的十六進位（{sym:02X}：X = 大寫十六進位、寬 2、前面補 0）。
#   以字元為符號時，換行、Tab、空白印出來看不見，所以換成 \n、\t、␠ 這些看得見的寫法。
#   isinstance(sym, int) 問「sym 是不是整數」；字典.get(key, 預設值) 是「查得到就用查到的，查不到就用預設值」，
#   這裡的預設值就是符號自己，也就是一般字元原樣印出。
def show(sym):
    if isinstance(sym, int):
        return f"{sym:02X}"
    return {"\n": "\\n", "\r": "\\r", "\t": "\\t", " ": "␠"}.get(sym, sym)


def main():
    # ── 解析命令列 ──
    # sys.argv[1:] 是 slicing（切片）：從索引 1 取到最後，也就是去掉 argv[0]（程式名稱）之後的所有參數。
    args = sys.argv[1:]
    # 「x in list」問 list 裡有沒有 x，結果是 True 或 False。
    as_bytes = "--bytes" in args
    # --freq 的「下一個」參數才是頻率表：args.index("--freq") 找出它的位置，+ 1 就是下一個。
    table = args[args.index("--freq") + 1] if "--freq" in args else None
    # list comprehension 加上 if：只留下「不是選項、也不是頻率表」的參數，剩下的第一個就是要編碼的字串。
    args = [a for a in args if a not in ("--bytes", "--freq", table)]

    # ── 準備頻率表 freq（符號 → 次數）。Huffman 只需要這張表，不需要知道符號原本的順序 ──
    if table:                                              # 直接給頻率表：沒有原始序列
        # 這一行由右往左讀："black:100,white:100" 先用 split(",") 切成 ["black:100", "white:100"]；
        # 裡面那層括號是 generator expression，把每一項再用 split(":") 切成 ["black", "100"]；
        # 「for k, v in …」把每一對拆成 k、v 兩個變數；k.strip() 去掉前後空白、int(v) 把字串轉成整數；
        # 最後交給 OrderedDict：一種記得「放進去的順序」的 dict。
        freq = collections.OrderedDict((k.strip(), int(v)) for k, v in (item.split(":") for item in table.split(",")))
        # 沒有原始序列，所以後面第 4 段（編碼結果）與最後的「原文幾 bytes」都會略過。
        symbols, text = None, None
        n = sum(freq.values())
        print(f"輸入：頻率表　共 {n} 個符號、{len(freq)} 種\n")
    else:
        text = args[0] if args else "ABRACADABRA"
        # 「符號怎麼定」在這一行決定（講義 1.7）：
        #   list(text)                  把字串拆成一個一個「字元」，"多" 是 1 個符號；
        #   list(text.encode("utf-8"))  先把 str 編成 bytes 再拆，得到 0–255 的整數，"多" 變成 E5、A4、9A 三個符號。
        symbols = list(text.encode("utf-8")) if as_bytes else list(text)
        n = len(symbols)
        # collections.Counter(一串東西) 會數每一種值各出現幾次，結果像 dict：{符號: 次數}。
        freq = collections.Counter(symbols)
        # {text!r} 的 !r 是印 repr()：帶引號的寫法，字串頭尾有空白也看得出來。
        print(f"輸入：{text!r}　符號＝{'byte' if as_bytes else '字元'}　共 {n} 個符號、{len(freq)} 種\n")
    # 最長的符號名稱有幾個字，後面印表格時用它當欄寬，才對得齊。
    # max( … for s in freq ) 裡是 generator expression；走訪 dict 得到的是它的 key（符號）。
    width = max(len(show(s)) for s in freq)

    # ── 1. 熵：這組機率下，平均每個符號「至少」要幾 bits（講義 1.3）──
    print("1. 統計機率，算熵  H = Σ p·log2(1/p)")
    h = 0.0
    # 排序只是為了印得好看：次數多的排前面，次數相同的照符號排。
    #   freq.items() 是一對一對的（符號, 次數）；key=lambda kv: … 是排序依據，lambda 是沒有名字的小函式；
    #   依據是一個 tuple (-次數, 符號)：tuple 比大小先比第一項、相同再比第二項；次數加負號，大的才會排在前面。
    #   str(kv[0]) 讓「字元」與「byte 整數」兩種符號都能用同一套規則比較。
    for sym, c in sorted(freq.items(), key=lambda kv: (-kv[1], str(kv[0]))):
        p = c / n
        # log2(1/p)：機率 p 的符號理想上該用幾 bits（p = 1/2 → 1 bit、p = 1/8 → 3 bits）；乘上 p 再加總，就是加權平均。
        h += p * math.log2(1 / p)
        # {show(sym):>{width}s}：欄寬本身也可以是變數，多包一層大括號即可。
        print(f"   {show(sym):>{width}s}  次數 {c:>4d}  p = {p:.4f}  log2(1/p) = {math.log2(1 / p):.3f} bits  p·log2(1/p) = {p * math.log2(1 / p):.3f}")
    print(f"   熵 H = {h:.4f} bits／符號\n")

    # ── 2. 建樹（講義 1.6 的步驟 1–3）──
    print("2. 建樹：每次取出頻率最小的兩個節點，合併成一個新節點放回去")
    # 講義說要一個「依次數由小到大排好的佇列」。這裡用 heapq 模組：它把一個普通的 list 維護成 heap，
    # heappop 永遠取出最小的元素、heappush 放進新元素，兩者都很快（O(log K)），不必每次重新排序。
    #
    # heap 裡的每個元素是一個 tuple：(次數, 編號, 顯示用的名稱, 符號)。
    # heapq 用「tuple 比大小」決定誰最小：先比次數；次數相同（平手）再比編號。
    #   編號 i：葉節點依符號排序後編成 0、1、2、…；之後合併出來的新節點接著編 K、K+1、…（下面的 tick）。
    #   編號不會重複，所以比較一定在第二項就分出勝負，輪不到後面的名稱與符號
    #   （新節點的符號是 None，None 和字串不能比大小，真的比到會出錯）。
    #   編號小 = 比較早進佇列，這就是檔頭說的平手規則。平手規則一定要固定，結果才會每次都一樣。
    # enumerate(…) 在走訪時順便給編號：每次得到 (i, 元素)；這裡的元素又是一對 (s, c)，所以寫成 i, (s, c)。
    heap = [(c, i, show(s), s) for i, (s, c) in enumerate(sorted(freq.items(), key=lambda kv: str(kv[0])))]
    # heapify 把 list 就地整理成 heap 的排列方式（之後 heap[0] 永遠是最小的）。
    heapq.heapify(heap)
    # 這支程式不存樹的形狀，只記兩件事就夠算出 code 長度：
    #   code_len[符號]  這個符號目前的 code 長度（= 它在樹裡的深度），一開始都是 0；
    #   members[編號]   這個節點底下有哪些葉（符號）。葉節點底下只有它自己。
    # { key: value for … } 是 dict comprehension，和 list comprehension 同樣的寫法，只是做出來的是 dict。
    # 「for _, i, _, s in heap」把每個 tuple 拆成四個變數；用不到的欄位照慣例用底線 _ 接住。
    code_len = {s: 0 for s in freq}
    members = {i: [s] for _, i, _, s in heap}
    tick, step = len(heap), 1
    # 特例：只有一種符號時不會有任何合併，長度會停在 0；但 0 bits 的 code 沒辦法表示「有幾個符號」（講義 1.6）。
    if len(heap) == 1:
        code_len[heap[0][3]] = 1
        print("   只有一種符號：code 長度定為 1（不能是 0，否則解碼端不知道有幾個）")
    # 佇列裡只剩一個節點時，它就是樹根，結束。
    while len(heap) > 1:
        # 連續 pop 兩次 = 取出最小的兩個。每個 tuple 拆成：次數、編號、名稱、（用不到的）符號。
        c1, i1, n1, _ = heapq.heappop(heap)
        c2, i2, n2, _ = heapq.heappop(heap)
        # 兩個節點被接到同一個新的父節點底下，它們底下「所有的葉」離根都遠了一層，code 各多 1 bit。
        # 兩個 list 用 + 相加是「接起來」。
        for s in members[i1] + members[i2]:
            code_len[s] += 1                               # 被合併一次，離根就多一層
        # 新節點底下的葉 = 兩邊的葉合起來。dict.pop(key) 取出那一項的值並把它從 dict 刪掉（舊節點不會再用到）。
        members[tick] = members.pop(i1) + members.pop(i2)
        name = f"({n1}+{n2})"
        print(f"   第 {step} 步：{n1}[{c1}] + {n2}[{c2}] → [{c1 + c2}]")
        # 新節點的次數 = 兩者相加，放回 heap；heapq 會自己把它擺到該在的位置。
        heapq.heappush(heap, (c1 + c2, tick, name, None))
        tick += 1
        step += 1

    # ── 3. 由長度決定 code ──
    # 上面只算出每個符號的「長度」，還沒有 0 與 1。canonical Huffman 規定一種固定的指派方式：
    # 只要兩端知道每個符號的長度，就能各自算出同一組 code，所以 codebook 只需要傳長度、不必傳 code 本身（codebook 的成本見講義 1.7）。
    # 做法：符號依（長度短的在前，長度相同照符號排）排好；cur 是「下一個要發出去的 code」（當成整數）；
    #   每發一個就 +1；遇到長度變長，先左移補 0（cur <<= 差幾位，相當於乘 2 的幾次方）再發。
    #   例（ABRACADABRA）：A 長度 1 → 0；cur = 1，B 長度 3 → 左移 2 位成 100；之後 C = 101、D = 110、R = 111。
    #   這樣發出來的 code 一定滿足 prefix code 的條件（講義 1.4）。
    # canonical code：依（長度, 符號）排序後，從 0 開始每次 +1，長度變長就左移補 0
    print("\n3. 由 code 長度指派 code（canonical Huffman：兩端只要知道「長度」就能得到同一組 code）")
    code, prev, cur = {}, 0, 0
    for sym in sorted(freq, key=lambda s: (code_len[s], str(s))):
        cur <<= code_len[sym] - prev
        prev = code_len[sym]
        # format(整數, "03b")：b = 二進位、寬 3、前面補 0；寬度用 f-string 填進去。前面的 0 是 code 的一部分，不能省。
        code[sym] = format(cur, f"0{prev}b")
        cur += 1
        print(f"   {show(sym):>{width}s}  長度 {prev}  code {code[sym]}")

    # ── 4、5. 算總 bits 數、平均碼長，並和定長編碼比較 ──
    # 總 bits = Σ 次數 × 長度；平均碼長 L = 總 bits ÷ 符號總數（= Σ p·len）。
    total_bits = sum(freq[s] * code_len[s] for s in freq)
    avg = total_bits / n
    # 定長編碼（FLC）：K 種符號要 ⌈log2 K⌉ bits 才分得開（8 種 → 3 bits、5 種 → 3 bits）；
    # K = 1 時 log2(1) = 0，但至少要 1 bit，所以用 max(1, …)。
    fixed = max(1, math.ceil(math.log2(len(freq))))
    # 「is not None」：有原始序列（不是 --freq 模式）才做得出實際的編碼結果。
    if symbols is not None:
        # 編碼 = 把每個符號換成它的 code，全部接起來。這裡的 code 是 "0"、"1" 組成的字串，方便觀察；
        # 真正的程式（MP3、MP4、Team 1）要用位元運算把它們塞進 byte 裡。
        bits = "".join(code[s] for s in symbols)
        print(f"\n4. 編碼結果（{len(bits)} bits）")
        # symbols[:40] 只取前 40 個符號來印，太長就在後面加 " ..."。
        print("   " + " ".join(code[s] for s in symbols[:40]) + (" ..." if n > 40 else ""))
        # 檔案的最小單位是 byte，所以位元串要補 0 補到 8 的倍數。
        # -len(bits) % 8 是「還差幾位」：Python 的 % 對負數也回傳 0 到 7，例如 23 bits → -23 % 8 = 1；剛好整除時是 0。
        padded = bits + "0" * (-len(bits) % 8)
        # range(0, 長度, 8) 是 0、8、16、…；padded[i:i + 8] 每次切 8 個字元；
        # int("01001110", 2) 把它當二進位數讀成整數；:02X 印成兩位的十六進位。
        print("   打包成 bytes（高位先出、末尾補 0）：" + " ".join(f"{int(padded[i:i + 8], 2):02X}" for i in range(0, len(padded), 8)))
    print("\n5. 比較")
    # 檢查講義 1.6 的 H ≤ L < H + 1（算出來不滿足，就表示程式有錯）。
    # Python 可以把比較串起來寫：a <= b < c 等於 (a <= b) and (b < c)。
    # 加上 1e-12 是容許浮點數的誤差：機率全是 2 的負次方時 L 應該「等於」H，
    # 但兩邊各自累加出來可能差 0.0000000000000002，不加這一點容差會被誤判成 H > L。
    print(f"   平均碼長 L = Σ p·len = {total_bits} ÷ {n} = {avg:.4f} bits／符號；熵 H = {h:.4f}；H ≤ L < H+1："
          + ("例外（只有一種符號：H = 0，但 code 長度不能是 0，所以 L = 1）" if len(freq) == 1 else str(h <= avg + 1e-12 < h + 1)))
    # {h / avg:.1%}：% 格式會自動乘 100 並加上百分比符號。
    print(f"   編碼效率 H ÷ L = {h / avg:.1%}")
    # 壓縮率 = 壓縮後 ÷ 原始，越小越好（講義 1.1）。這裡的「原始」是定長編碼的 bits 數。
    # 兩個字串常值相鄰（中間只有換行與空白）時，Python 會自動把它們接成一個字串，長的字串可以這樣拆成兩行。
    print(f"   定長編碼（FLC）每個符號要 {fixed} bits → {fixed * n} bits；Huffman {total_bits} bits；"
          f"壓縮率 {total_bits / (fixed * n):.1%}（{fixed * n}:{total_bits} ≈ {fixed * n / total_bits:.2f}:1）")
    if text is not None:
        # 和「原文的 UTF-8 bytes 數」比。位元流看起來很小，但解碼端還需要 codebook 才解得開，
        # 把它算進去之後，短訊息常常反而變大（講義 1.7）。
        orig = len(text.encode("utf-8"))
        print(f"   原文 {orig} bytes；Huffman 位元流 {math.ceil(total_bits / 8)} bytes（還沒算 codebook！）")


# 直接執行這個檔案時才呼叫 main()；被別的程式 import 時不會自動執行。
if __name__ == "__main__":
    main()
