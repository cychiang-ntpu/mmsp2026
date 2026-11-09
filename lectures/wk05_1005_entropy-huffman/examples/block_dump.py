#!/usr/bin/env python3
"""block_dump.py — 把 Team 1 python_ref 的 Huffman 區塊逐 byte、逐欄位印出來（像第 3 週讀 WAV 檔頭那樣讀「區塊」）。

用法：
  python3 block_dump.py "ABRACADABRA"                       字串，符號 = char
  python3 block_dump.py --file 檔案.txt                     檔案，符號依副檔名（.txt char、.wav s16、其他 byte）
  python3 block_dump.py --file 檔案.wav --sym byte          指定符號種類
  python3 block_dump.py --file 檔案 --out 區塊.bin           順便把區塊存成檔案（拿去餵你們的 huff_decode）

它呼叫 ../../../team_projects/team1_textlink/python_ref/textlink.py 的 huff_encode（不複製那段程式），
把回傳的區塊依 python_ref 的格式拆開：
  kind 1｜原始長度 8｜符號個數 N 8｜符號種類數 K 4｜K 組（符號 W bytes＋長度 1 byte，依符號值遞增）｜
  （只有 s16）head 長度 4、head、tail 長度 4、tail｜bitstream（高位先出，最後補 0）
每一欄印出 offset、原始 bytes（十六進位）與解讀後的意思。對應講義第三節 3.1：
你們的格式可以和它不同，但 interface.md 裡每一欄都要像這樣說得出「幾個 byte、什麼順序、代表什麼」。
"""
import importlib.util
import os
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "..", "..", "..", "team_projects", "team1_textlink", "python_ref", "textlink.py")


def load_ref():
    spec = importlib.util.spec_from_file_location("textlink", REF)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def hexs(b, limit=16):
    s = " ".join(f"{x:02X}" for x in b[:limit])
    return s + (f" …（共 {len(b)} bytes）" if len(b) > limit else "")


def main():
    args = sys.argv[1:]
    path = text = out = None
    kind_name = None
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--file":
            path = args[i + 1]; i += 2
        elif a == "--sym":
            kind_name = args[i + 1]; i += 2
        elif a == "--out":
            out = args[i + 1]; i += 2
        else:
            text = a; i += 1
    tl = load_ref()
    kinds = {"byte": tl.SYM_BYTE, "char": tl.SYM_CHAR, "s16": tl.SYM_S16}
    if path:
        data = open(path, "rb").read()
        if kind_name is None:
            ext = os.path.splitext(path)[1].lower()
            kind_name = "char" if ext == ".txt" else "s16" if ext == ".wav" else "byte"
    else:
        data = (text or "ABRACADABRA").encode("utf-8")
        kind_name = kind_name or "char"
    kind = kinds[kind_name]
    try:
        block = tl.huff_encode(data, kind)
    except ValueError as e:
        print(f"這份資料不適用符號 {kind_name}（{e}）：Team 1 的殼會改用 byte 再試一次"); sys.exit(1)
    if out:
        open(out, "wb").write(block)

    print(f"輸入 {len(data):,} bytes，符號 = {kind_name} → 區塊 {len(block):,} bytes（{len(block) / len(data) * 100:.2f}%）" if data else f"輸入 0 bytes → 區塊 {len(block)} bytes")
    print(f"  {'offset':>7}  {'bytes':<26}  意思")
    pos = 0

    def field(n, meaning):
        nonlocal pos
        print(f"  {pos:>7}  {hexs(block[pos:pos + n]):<26}  {meaning}")
        pos += n

    field(1, f"kind = {block[0]}（{kind_name}）")
    orig = int.from_bytes(block[1:9], "big"); field(8, f"原始長度 = {orig:,}（big-endian）→ 解碼端先和 max_out 比")
    n = int.from_bytes(block[9:17], "big"); field(8, f"符號個數 N = {n:,} → 解碼的停止條件（講義 1.3 (a)）")
    k = int.from_bytes(block[17:21], "big"); field(4, f"符號種類數 K = {k:,} → 後面有 K 組 codebook")
    width = tl.SYM_WIDTH[kind]
    print(f"  ----- codebook：K × （符號 {width} byte{'s' if width > 1 else ''} ＋ 長度 1 byte）= {k * (width + 1):,} bytes，依符號值遞增；沒有 code（canonical，講義 2.2）")
    show = 8 if k > 12 else k                      # K 很大時只印前 8 組與最後 2 組，但長度要全部讀進來（算 bitstream 的 bits 數用）
    lengths = {}
    for j in range(k):
        at = 21 + j * (width + 1)
        sym = int.from_bytes(block[at:at + width], "big"); ln = block[at + width]
        lengths[sym] = ln
        if j < show or j >= k - 2:
            field(width + 1, f"符號 {tl.show_symbol(sym, kind)}，長度 {ln}")
        elif j == show:
            print(f"  {'…':>7}  {'':<26}  （略過 {k - show - 2} 組）")
            pos += (width + 1) * (k - show - 2)
    codes = tl.canonical_codes(lengths)
    if k:
        sample = list(sorted(lengths, key=lambda s: (lengths[s], s)))[:6]
        print("  ----- 解碼端用同一條規則重建的 code（前幾個）：" + "  ".join(f"{tl.show_symbol(s, kind)}={codes[s]}" for s in sample))
    if kind == tl.SYM_S16:
        hn = int.from_bytes(block[pos:pos + 4], "big"); field(4, f"head 長度 = {hn}（data 區之前的 bytes：RIFF 檔頭、fmt、其他 chunk）")
        field(hn, "head 原樣保留")
        tn = int.from_bytes(block[pos:pos + 4], "big"); field(4, f"tail 長度 = {tn}（data 區之後的 bytes）")
        field(tn, "tail 原樣保留")
    stream = block[pos:]
    total_bits = sum(lengths[s] for s in tl.split_symbols(data, kind)[0]) if k else 0
    pad = len(stream) * 8 - total_bits
    print(f"  {pos:>7}  {hexs(stream):<26}  bitstream {len(stream):,} bytes = ⌈{total_bits:,} bits ÷ 8⌉；最後 {pad} 個 0 是補位")
    if k and len(stream) <= 8:
        bits = "".join(f"{b:08b}" for b in stream)
        print(f"           {bits[:total_bits]} [{bits[total_bits:]}]")
    print(f"  合計 {len(block):,} bytes = 檔頭 21 + codebook {k * (width + 1):,}" + (f" + 原樣保留 {8 + hn + tn:,}" if kind == tl.SYM_S16 else "") + f" + bitstream {len(stream):,}")


if __name__ == "__main__":
    main()
