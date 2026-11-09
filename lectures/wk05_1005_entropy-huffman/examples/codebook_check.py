#!/usr/bin/env python3
"""codebook_check.py — 檢查一張 codebook.csv（MP3 定長或 MP4 Huffman 的格式）有沒有道理，順便算 H、L、預期的 bin 大小。

用法：
  python3 codebook_check.py codebook.csv                  檢查 codebook
  python3 codebook_check.py codebook.csv encoded.bin      再和 bin 的大小比對，並用 codebook 把 bin 解一遍
  加 --bits     把 bin 的位元流印出來，標出每個 code 的邊界與最後補位的 0（檔案小的時候用）
  加 --summary  只印一行 N=… K=… H=… L=… bits=… bytes=…（make check 拿它和 huffman_build --summary 比）

它不是 MP3／MP4 的 decoder（那是你要寫的），而是「寫到一半先檢查」的工具：
  codebook 的格式對不對（欄位數、code 只有 0 與 1、沒有重複的符號、有 EOF）
  是不是 prefix code（沒有任何 code 是另一個 code 的開頭；定長編碼自動滿足）
  Kraft 不等式 Σ 2^(−len) ≤ 1（Huffman 的樹是滿的，應該剛好 = 1；定長是 K ÷ 2^b）
  H ≤ L < H + 1（講義 1.6；定長編碼的 L 固定是 7，只印不檢查）
  預期的 encoded.bin 大小 = ⌈(Σ 次數×長度 ＋ EOF 的長度) ÷ 8⌉，有給 bin 就比對

對應講義：第一節 1.4（MP3 的 CSV 格式）、第二節 2.4–2.5（MP4 的 CSV 格式與 CI 怎麼測）。
"""
import math
import sys

if hasattr(sys.stdout, "reconfigure"):          # Windows 導到檔案時仍用 UTF-8（與第 3 週 huffman_demo.py 相同）
    sys.stdout.reconfigure(encoding="utf-8")

UNESCAPE = {r"\n": "\n", r"\r": "\r", r"\t": "\t"}


def field_to_sym(field):
    """CSV 的第一欄 → 符號。規則與 samples_2025-python 相同：外面一層雙引號、\\n \\r \\t 三個跳脫、" 寫成 ""。"""
    body = field.strip()
    if len(body) < 2 or body[0] != '"' or body[-1] != '"':
        raise ValueError(f"符號欄要用雙引號包起來：{field!r}")
    body = body[1:-1]
    if body == "EOF":
        return "<EOF>"
    return UNESCAPE.get(body) or body.replace('""', '"')


def show(sym):
    return {"<EOF>": "EOF", "\n": "\\n", "\r": "\\r", "\t": "\\t", " ": "␠"}.get(sym, sym)


def parse(path):
    """回傳 (格式, rows)；rows 是 [(符號, 次數, code 字串), …]，依檔案裡的順序。
    MP3 一列 4 欄："sym",count,prob,bits         （bits 沒有引號、全部等長）
    MP4 一列 5 欄："sym",count,p,"code",info      （code 有引號）
    符號欄本身可能含逗號（例如 ","），所以從右邊切（rsplit），剩下的整段都是符號欄。"""
    rows, fmt = [], None
    with open(path, encoding="utf-8", newline="") as f:
        for lineno, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line.strip():
                continue
            parts5 = line.rsplit(",", 4)
            if len(parts5) == 5 and parts5[3].strip().startswith('"'):
                sym_f, cnt, _p, code, _info = parts5
                this = "mp4"
                code = code.strip().strip('"')
            else:
                parts4 = line.rsplit(",", 3)
                if len(parts4) != 4:
                    raise ValueError(f"第 {lineno} 列欄位數不對：{line!r}")
                sym_f, cnt, _p, code = parts4
                this = "mp3"
                code = code.strip()
            fmt = fmt or this
            if this != fmt:
                raise ValueError(f"第 {lineno} 列的格式（{this}）和前面（{fmt}）不一樣")
            try:
                n = int(cnt)
            except ValueError:
                raise ValueError(f"第 {lineno} 列的次數不是整數：{cnt!r}")
            if not code or set(code) - {"0", "1"}:
                raise ValueError(f"第 {lineno} 列的 code 只能是 0 與 1 而且不能是空的：{code!r}")
            rows.append((field_to_sym(sym_f), n, code))
    if not rows:
        raise ValueError("codebook 是空的")
    return fmt, rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = {a for a in sys.argv[1:] if a.startswith("--")}
    if not args:
        print(__doc__); sys.exit(2)
    csv_path, bin_path = args[0], (args[1] if len(args) > 1 else None)
    summary, want_bits = "--summary" in opt, "--bits" in opt
    problems = []

    try:
        fmt, rows = parse(csv_path)
    except ValueError as e:
        print(f"❌ {e}"); sys.exit(1)

    syms = [s for s, _, _ in rows]
    codes = {s: c for s, _, c in rows}
    counts = {s: n for s, n, _ in rows}
    K = len(rows)
    if len(set(syms)) != K:
        dup = sorted({s for s in syms if syms.count(s) > 1})
        problems.append(f"重複的符號：{', '.join(show(s) for s in dup)}")
    if len(set(codes.values())) != K:
        problems.append("有兩個符號拿到同一個 code")
    if "<EOF>" not in codes:
        problems.append('沒有 EOF 這一列（MP3、MP4 都要有："EOF"）')

    # prefix code：把 code 排序後，只要相鄰的兩個不是「前者是後者的開頭」就整張表都不是
    ordered = sorted(codes.values())
    for a, b in zip(ordered, ordered[1:]):
        if a != b and b.startswith(a):
            problems.append(f"不是 prefix code：{a} 是 {b} 的開頭")
            break
    kraft = sum(2.0 ** -len(c) for c in codes.values())
    if kraft > 1 + 1e-9:
        problems.append(f"Kraft 不等式不成立：Σ 2^(−len) = {kraft:.6f} > 1，這組長度做不出 prefix code")
    lengths = {len(c) for c in codes.values()}
    if fmt == "mp3" and len(lengths) != 1:
        problems.append(f"MP3 是定長編碼，但 code 長度有 {sorted(lengths)} 幾種")
    if fmt == "mp3" and lengths != {7}:
        problems.append(f"MP3 規定 7 bits，這張表是 {sorted(lengths)} bits")

    # H、L、位元流大小。MP3 的 EOF 次數是 0 但會寫一次；MP4 的 EOF 次數是 1、已經算在次數裡
    N = sum(counts.values())
    H = -sum(n / N * math.log2(n / N) for n in counts.values() if n) if N else 0.0
    total_bits = sum(n * len(codes[s]) for s, n in counts.items())
    L = total_bits / N if N else 0.0                 # 平均碼長：只算有出現的符號（MP3 的 EOF 次數是 0，不算進 L）
    if counts.get("<EOF>", 1) == 0:
        total_bits += len(codes["<EOF>"])            # 但 EOF 會寫進 bin 一次，算位元流大小時要加上
    if fmt == "mp4" and K >= 2 and not (H <= L + 1e-9 and L < H + 1):
        problems.append(f"H ≤ L < H + 1 不成立：H = {H:.4f}、L = {L:.4f}（codebook 不是最佳的，或次數欄不對）")
    exp_bytes = (total_bits + 7) // 8

    if summary:
        print(f"N={N} K={K} H={H:.4f} L={L:.4f} bits={total_bits} bytes={exp_bytes}")
        sys.exit(1 if problems else 0)

    print(f"{csv_path}：{'MP4 Huffman' if fmt == 'mp4' else 'MP3 定長'} 格式，K = {K} 種符號（含 EOF），N = {N} 個")
    print(f"  Kraft Σ 2^(−len) = {kraft:.6f}；H = {H:.4f} bits／符號；L = {L:.4f}；位元流 {total_bits} bits → 預期 bin {exp_bytes} bytes")
    top = sorted(((n, s) for s, n in counts.items() if s != "<EOF>"), reverse=True)[:8]
    for n, s in top:
        print(f"    {show(s):>4}  次數 {n:>8,d}  p={n / N:.4f}  理想 {math.log2(N / n):5.2f} bits  code {codes[s]}（{len(codes[s])} bits）")

    # 有 bin：比大小，再用這張 codebook 把它解一遍（逐 bit 累積、查表；prefix code 保證第一個命中的就是對的）
    if bin_path:
        data = open(bin_path, "rb").read()
        if len(data) != exp_bytes:
            problems.append(f"{bin_path} 是 {len(data)} bytes，依 codebook 應該是 {exp_bytes} bytes")
        lookup = {c: s for s, c in codes.items()}
        bits = "".join(f"{b:08b}" for b in data)
        pos, cur, n_dec, hit_eof, pieces = 0, "", 0, False, []
        for i, bit in enumerate(bits):
            cur += bit
            if cur in lookup:
                s = lookup[cur]
                pieces.append((cur, s))
                cur = ""
                if s == "<EOF>":
                    hit_eof, pos = True, i + 1
                    break
                n_dec += 1
        if not hit_eof:
            problems.append("把 bin 解到底都沒遇到 EOF（bit order 反了？忘了寫 EOF？）")
        else:
            pad = len(bits) - pos
            expect_n = N - (1 if fmt == "mp4" else 0)
            if n_dec != expect_n:
                problems.append(f"解出 {n_dec} 個符號，codebook 的次數加起來是 {expect_n} 個")
            if pad >= 8:
                problems.append(f"EOF 後面還有 {pad} bits：補位最多 7 個 0，多出來的是什麼？")
            print(f"  {bin_path}：{len(data)} bytes，解出 {n_dec} 個符號後遇到 EOF，之後補位 {pad} bits")
            if want_bits:
                print("  位元流（| 是 code 的邊界）：")
                line = " | ".join(f"{c}→{show(s)}" for c, s in pieces)
                print("    " + line + (f" | {bits[pos:]}（補位）" if pad else ""))

    if problems:
        for p in problems:
            print(f"  ❌ {p}")
        sys.exit(1)
    print("  ✅ codebook 沒有問題")


if __name__ == "__main__":
    main()
