#!/usr/bin/env python3
"""MP3 參考實作（Python）：定長編碼（fixed-length coding）＋ bit packing。

用法：
  python3 flc_codec.py encode input.txt codebook.csv encoded.bin
  python3 flc_codec.py decode encoded.bin codebook.csv output.txt
"""
import argparse
import collections

CODE_BITS = 7
EOF_MARK = None  # 以 None 代表 EOF 符號

ESCAPE = {"\n": r"\n", "\r": r"\r", "\t": r"\t"}
UNESCAPE = {v: k for k, v in ESCAPE.items()}


def sym_to_field(sym: str) -> str:
    body = ESCAPE.get(sym) or sym.replace('"', '""')
    return f'"{body}"'


def field_to_sym(field: str) -> str:
    body = field.strip()[1:-1]
    return UNESCAPE.get(body) or body.replace('""', '"')


def build_codebook(text: str):
    """依出現次數（少→多）排序後，依序指派 7-bit 編號；EOF 排最後。"""
    counter = collections.Counter(text)
    ordered = sorted(counter, key=lambda ch: (counter[ch], len(ch.encode()), ch.encode()))
    symbols = ordered + [EOF_MARK]
    if len(symbols) > 2 ** CODE_BITS:
        raise SystemExit(f"too many symbols for {CODE_BITS}-bit codes")
    return {sym: format(i, f"0{CODE_BITS}b") for i, sym in enumerate(symbols)}, counter


def encode(args):
    text = open(args.input, encoding="utf-8").read()
    code, counter = build_codebook(text)
    total = len(text)

    with open(args.codebook, "w", encoding="utf-8", newline="") as f:
        for sym, bits in code.items():
            if sym is EOF_MARK:
                f.write(f'"EOF",0,0.0000000,{bits}\n')
            else:
                f.write(f"{sym_to_field(sym)},{counter[sym]},{counter[sym] / total:.7f},{bits}\n")

    # 全部編碼串成一條 bit 字串，補零到整數 byte，一次轉 bytes
    bits = "".join(code[ch] for ch in text) + code[EOF_MARK]
    bits += "0" * (-len(bits) % 8)
    payload = int(bits, 2).to_bytes(len(bits) // 8, "big") if bits else b""
    open(args.output, "wb").write(payload)


def decode(args):
    # codebook：反查 bit pattern -> 符號
    lookup = {}
    for line in open(args.codebook, encoding="utf-8"):
        if not line.strip():
            continue
        field, _count, _prob, bits = line.rsplit(",", 3)
        lookup[bits.strip()] = field_to_sym(field)

    data = open(args.input, "rb").read()
    stream = bin(int.from_bytes(data, "big"))[2:].zfill(len(data) * 8)
    out = []
    for i in range(0, len(stream) - CODE_BITS + 1, CODE_BITS):
        sym = lookup.get(stream[i:i + CODE_BITS])
        if sym == "EOF" or sym is None:
            break
        out.append(sym)
    open(args.output, "w", encoding="utf-8", newline="").write("".join(out))


def main():
    ap = argparse.ArgumentParser(description="fixed-length codec with bit packing")
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("encode"); e.add_argument("input"); e.add_argument("codebook"); e.add_argument("output"); e.set_defaults(fn=encode)
    d = sub.add_parser("decode"); d.add_argument("input"); d.add_argument("codebook"); d.add_argument("output"); d.set_defaults(fn=decode)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
