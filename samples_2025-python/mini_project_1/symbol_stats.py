#!/usr/bin/env python3
"""MP1 參考實作（Python）：統計文字中每個符號的出現次數與機率，輸出 CSV。

用法：python3 symbol_stats.py input.txt output.csv
"""
import argparse
import collections

SPECIAL = {"\n": r"\n", "\r": r"\r", "\t": r"\t"}


def to_csv_field(ch: str) -> str:
    """把符號包成 CSV 欄位：控制字元轉義、雙引號依 CSV 慣例雙寫。"""
    if ch in SPECIAL:
        return f'"{SPECIAL[ch]}"'
    return '"' + ch.replace('"', '""') + '"'


def main():
    ap = argparse.ArgumentParser(description="symbol statistics -> CSV")
    ap.add_argument("input")
    ap.add_argument("output")
    args = ap.parse_args()

    text = open(args.input, encoding="utf-8").read()
    counter = collections.Counter(text)
    total = sum(counter.values())

    # 排序：次數多→少；同次數者位元組長度短→長；再依位元組值
    ordered = sorted(counter.items(),
                     key=lambda kv: (-kv[1], len(kv[0].encode()), kv[0].encode()))

    with open(args.output, "w", encoding="utf-8", newline="") as f:
        for ch, count in ordered:
            f.write(f"{to_csv_field(ch)},{count},{count / total:.15f}\n")


if __name__ == "__main__":
    main()
