#!/usr/bin/env python3
"""數值比對：兩個純文字數字檔（空白分隔）逐值比較，允許浮點誤差。
用法：python3 numcmp.py a.txt b.txt [rel_tol] [abs_tol]
      python3 numcmp.py --sqnr a.log b.log      比較 "SQNR = x dB" 那一行，容差 1e-3 dB
"""
import math, re, sys

def load(path):
    return [float(x) for line in open(path) for x in line.split()]

def main():
    if sys.argv[1] == "--sqnr":
        pat = re.compile(r"SQNR\s*=\s*([-+0-9.eE]+)")
        vals = []
        for p in sys.argv[2:4]:
            m = pat.search(open(p, errors="replace").read())
            if not m:
                sys.exit(f"{p}: 找不到 'SQNR = <數字>' 這一行")
            vals.append(float(m.group(1)))
        if abs(vals[0] - vals[1]) > 1e-3:
            sys.exit(f"SQNR 不同：{vals[0]} vs {vals[1]}（容差 0.001 dB）")
        print(f"SQNR 一致：{vals[0]:.6f} dB")
        return
    a, b = load(sys.argv[1]), load(sys.argv[2])
    rel = float(sys.argv[3]) if len(sys.argv) > 3 else 1e-4
    abs_ = float(sys.argv[4]) if len(sys.argv) > 4 else 1e-2
    if len(a) != len(b):
        sys.exit(f"數值個數不同：{len(a)} vs {len(b)}（檢查列數／每列欄數）")
    bad = [(i, x, y) for i, (x, y) in enumerate(zip(a, b)) if not math.isclose(x, y, rel_tol=rel, abs_tol=abs_)]
    if bad:
        i, x, y = bad[0]
        sys.exit(f"{len(bad)} 個數值超出容差，第一個在第 {i} 個：{x} vs {y}")
    print(f"數值一致：{len(a)} 個值")

main()
