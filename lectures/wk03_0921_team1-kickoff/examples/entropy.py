#!/usr/bin/env python3
"""entropy.py — entropy.c 的 Python 對照版，輸出逐 byte 相同。

用法：python3 entropy.py <檔案>
"""
import collections
import math
import sys

TOP = 10


def report(label, counter, file_bytes):
    n = sum(counter.values())
    h = 0.0
    for sym in sorted(counter):                       # 與 C 版相同的累加順序（符號值由小到大）
        p = counter[sym] / n
        h -= p * math.log2(p)
    ideal = 100.0 * h * n / 8.0 / file_bytes if file_bytes else 0.0
    return f"[{label}]  N={n}  K={len(counter)}  H={h:.4f} bits/symbol  ideal={ideal:.2f}%\n"


def main():
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} <file>")
    data = open(sys.argv[1], "rb").read()
    out = [f"file: {sys.argv[1]}  ({len(data)} bytes)\n", report("byte", collections.Counter(data), len(data))]
    try:
        text = data.decode("utf-8")                   # 嚴格解碼：overlong、代理區、超出範圍都會丟例外
    except UnicodeDecodeError:
        out.append("[char]  not valid UTF-8\n")
    else:
        c = collections.Counter(ord(ch) for ch in text)
        out.append(report("char", c, len(data)))
        out.append(f"top {TOP} characters:\n")
        n = sum(c.values())
        for cp, cnt in sorted(c.items(), key=lambda kv: (-kv[1], kv[0]))[:TOP]:
            p = cnt / n
            shown = "(ctrl)" if cp < 0x20 or cp == 0x7F else chr(cp)
            out.append(f"  U+{cp:04X}  {shown}  count={cnt}  p={p:.4f}  -log2(p)={-math.log2(p):.2f}\n")
    sys.stdout.buffer.write("".join(out).encode("utf-8"))


if __name__ == "__main__":
    main()
