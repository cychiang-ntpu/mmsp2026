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
import collections
import heapq
import math
import sys


def show(sym):
    if isinstance(sym, int):
        return f"{sym:02X}"
    return {"\n": "\\n", "\r": "\\r", "\t": "\\t", " ": "␠"}.get(sym, sym)


def main():
    args = sys.argv[1:]
    as_bytes = "--bytes" in args
    table = args[args.index("--freq") + 1] if "--freq" in args else None
    args = [a for a in args if a not in ("--bytes", "--freq", table)]

    if table:                                              # 直接給頻率表：沒有原始序列
        freq = collections.OrderedDict((k.strip(), int(v)) for k, v in (item.split(":") for item in table.split(",")))
        symbols, text = None, None
        n = sum(freq.values())
        print(f"輸入：頻率表　共 {n} 個符號、{len(freq)} 種\n")
    else:
        text = args[0] if args else "ABRACADABRA"
        symbols = list(text.encode("utf-8")) if as_bytes else list(text)
        n = len(symbols)
        freq = collections.Counter(symbols)
        print(f"輸入：{text!r}　符號＝{'byte' if as_bytes else '字元'}　共 {n} 個符號、{len(freq)} 種\n")
    width = max(len(show(s)) for s in freq)

    print("1. 統計機率，算熵  H = Σ p·log2(1/p)")
    h = 0.0
    for sym, c in sorted(freq.items(), key=lambda kv: (-kv[1], str(kv[0]))):
        p = c / n
        h += p * math.log2(1 / p)
        print(f"   {show(sym):>{width}s}  次數 {c:>4d}  p = {p:.4f}  log2(1/p) = {math.log2(1 / p):.3f} bits  p·log2(1/p) = {p * math.log2(1 / p):.3f}")
    print(f"   熵 H = {h:.4f} bits／符號\n")

    print("2. 建樹：每次取出頻率最小的兩個節點，合併成一個新節點放回去")
    heap = [(c, i, show(s), s) for i, (s, c) in enumerate(sorted(freq.items(), key=lambda kv: str(kv[0])))]
    heapq.heapify(heap)
    code_len = {s: 0 for s in freq}
    members = {i: [s] for _, i, _, s in heap}
    tick, step = len(heap), 1
    if len(heap) == 1:
        code_len[heap[0][3]] = 1
        print("   只有一種符號：code 長度定為 1（不能是 0，否則解碼端不知道有幾個）")
    while len(heap) > 1:
        c1, i1, n1, _ = heapq.heappop(heap)
        c2, i2, n2, _ = heapq.heappop(heap)
        for s in members[i1] + members[i2]:
            code_len[s] += 1                               # 被合併一次，離根就多一層
        members[tick] = members.pop(i1) + members.pop(i2)
        name = f"({n1}+{n2})"
        print(f"   第 {step} 步：{n1}[{c1}] + {n2}[{c2}] → [{c1 + c2}]")
        heapq.heappush(heap, (c1 + c2, tick, name, None))
        tick += 1
        step += 1

    # canonical code：依（長度, 符號）排序後，從 0 開始每次 +1，長度變長就左移補 0
    print("\n3. 由 code 長度指派 code（canonical Huffman：兩端只要知道「長度」就能得到同一組 code）")
    code, prev, cur = {}, 0, 0
    for sym in sorted(freq, key=lambda s: (code_len[s], str(s))):
        cur <<= code_len[sym] - prev
        prev = code_len[sym]
        code[sym] = format(cur, f"0{prev}b")
        cur += 1
        print(f"   {show(sym):>{width}s}  長度 {prev}  code {code[sym]}")

    total_bits = sum(freq[s] * code_len[s] for s in freq)
    avg = total_bits / n
    fixed = max(1, math.ceil(math.log2(len(freq))))
    if symbols is not None:
        bits = "".join(code[s] for s in symbols)
        print(f"\n4. 編碼結果（{len(bits)} bits）")
        print("   " + " ".join(code[s] for s in symbols[:40]) + (" ..." if n > 40 else ""))
        padded = bits + "0" * (-len(bits) % 8)
        print("   打包成 bytes（高位先出、末尾補 0）：" + " ".join(f"{int(padded[i:i + 8], 2):02X}" for i in range(0, len(padded), 8)))
    print("\n5. 比較")
    print(f"   平均碼長 L = Σ p·len = {total_bits} ÷ {n} = {avg:.4f} bits／符號；熵 H = {h:.4f}；H ≤ L < H+1："
          + ("例外（只有一種符號：H = 0，但 code 長度不能是 0，所以 L = 1）" if len(freq) == 1 else str(h <= avg + 1e-12 < h + 1)))
    print(f"   編碼效率 H ÷ L = {h / avg:.1%}")
    print(f"   定長編碼（FLC）每個符號要 {fixed} bits → {fixed * n} bits；Huffman {total_bits} bits；"
          f"壓縮率 {total_bits / (fixed * n):.1%}（{fixed * n}:{total_bits} ≈ {fixed * n / total_bits:.2f}:1）")
    if text is not None:
        orig = len(text.encode("utf-8"))
        print(f"   原文 {orig} bytes；Huffman 位元流 {math.ceil(total_bits / 8)} bytes（還沒算 codebook！）")


if __name__ == "__main__":
    main()
