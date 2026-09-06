#!/usr/bin/env python3
"""MP4 參考實作（Python）：Huffman 編碼／解碼（使用標準函式庫 heapq）。

用法：
  python3 huffman_codec.py encode input.txt codebook.csv encoded.bin
  python3 huffman_codec.py decode encoded.bin codebook.csv output.txt
"""
import argparse
import collections
import heapq
import math

EOF = "<EOF>"

ESCAPE = {"\n": r"\n", "\r": r"\r", "\t": r"\t"}
UNESCAPE = {v: k for k, v in ESCAPE.items()}


def sym_to_field(sym: str) -> str:
    body = "EOF" if sym == EOF else (ESCAPE.get(sym) or sym.replace('"', '""'))
    return f'"{body}"'


def field_to_sym(field: str) -> str:
    body = field.strip()[1:-1]
    if body == "EOF":
        return EOF
    return UNESCAPE.get(body) or body.replace('""', '"')


def build_codes(freq: dict) -> dict:
    """heapq 建 Huffman 樹；直接在 heap 元素上累積 (symbol, code) 清單。"""
    heap = [(f, idx, [(sym, "")]) for idx, (sym, f) in enumerate(sorted(freq.items()))]
    heapq.heapify(heap)
    if len(heap) == 1:  # 只有一種符號的退化情形
        f, idx, leaves = heap[0]
        return {leaves[0][0]: "0"}
    tick = len(heap)
    while len(heap) > 1:
        f1, _, left = heapq.heappop(heap)
        f2, _, right = heapq.heappop(heap)
        merged = [(s, "0" + c) for s, c in left] + [(s, "1" + c) for s, c in right]
        heapq.heappush(heap, (f1 + f2, tick, merged))
        tick += 1
    return dict(heap[0][2])


def encode(args):
    text = open(args.input, encoding="utf-8").read()
    freq = collections.Counter(text)
    freq[EOF] = 1
    total = sum(freq.values())
    codes = build_codes(freq)

    with open(args.codebook, "w", encoding="utf-8", newline="") as f:
        for sym in sorted(codes, key=lambda s: (len(codes[s]), codes[s])):
            p = freq[sym] / total
            f.write(f'{sym_to_field(sym)},{freq[sym]},{p:.15f},"{codes[sym]}",{-math.log2(p):.15f}\n')

    bits = "".join(codes[ch] for ch in text) + codes[EOF]
    bits += "0" * (-len(bits) % 8)
    open(args.output, "wb").write(int(bits, 2).to_bytes(len(bits) // 8, "big"))

    entropy = -sum(f / total * math.log2(f / total) for f in freq.values())
    avg_len = sum(freq[s] * len(codes[s]) for s in codes) / total
    print(f"entropy = {entropy:.4f} bits/symbol, huffman average = {avg_len:.4f}")


def decode(args):
    codes = {}
    for line in open(args.codebook, encoding="utf-8"):
        if not line.strip():
            continue
        field, _count, _p, codeword, _info = line.rsplit(",", 4)
        codes[codeword.strip().strip('"')] = field_to_sym(field)

    data = open(args.input, "rb").read()
    stream = bin(int.from_bytes(data, "big"))[2:].zfill(len(data) * 8)
    out, cur = [], ""
    for bit in stream:                 # 前綴碼：逐 bit 累積直到命中 codebook
        cur += bit
        if cur in codes:
            sym = codes[cur]
            if sym == "<EOF>":
                break
            out.append(sym)
            cur = ""
    open(args.output, "w", encoding="utf-8", newline="").write("".join(out))


def main():
    ap = argparse.ArgumentParser(description="Huffman codec")
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("encode"); e.add_argument("input"); e.add_argument("codebook"); e.add_argument("output"); e.set_defaults(fn=encode)
    d = sub.add_parser("decode"); d.add_argument("input"); d.add_argument("codebook"); d.add_argument("output"); d.set_defaults(fn=decode)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
