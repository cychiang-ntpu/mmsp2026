#!/usr/bin/env python3
"""wav_info.py — 把 WAV 檔拆開來看：RIFF 的每個 chunk、fmt 的每個欄位、sample 值的 histogram 與熵。
只用標準函式庫（刻意不用 wave 模組，自己走 chunk，和你們在 C 裡要做的事一樣）。

用法：python3 wav_info.py <檔案.wav>

WAV = RIFF 容器：12 bytes 的 "RIFF" <大小> "WAVE"，後面接一串 chunk；
每個 chunk = 4 bytes 的 id + 4 bytes 的大小（little-endian）+ 內容，內容長度是奇數時補 1 byte。
"fmt " 描述格式、"data" 放 PCM sample。data 不一定緊接在第 44 byte：中間可能有 "LIST" 等 chunk。
"""
import collections
import math
import struct
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} <file.wav>")
    d = open(sys.argv[1], "rb").read()
    if len(d) < 12 or d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        sys.exit("不是 RIFF/WAVE 檔")
    print(f"{sys.argv[1]}：{len(d):,} bytes；RIFF 大小欄位 = {struct.unpack('<I', d[4:8])[0]:,}（= 檔案大小 − 8）\n")

    fmt, data_off, data_len, p = None, None, 0, 12
    print("  位移      chunk   大小")
    while p + 8 <= len(d):
        cid, size = d[p:p + 4], struct.unpack("<I", d[p + 4:p + 8])[0]
        print(f"  {p:>6d}    {cid.decode('latin-1')!r:8s}{size:>10,d}")
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", d[p + 8:p + 24])
        elif cid == b"data":
            data_off, data_len = p + 8, min(size, len(d) - (p + 8))
        p += 8 + size + (size & 1)

    if fmt is None or data_off is None:
        sys.exit("找不到 fmt 或 data chunk")
    tag, ch, rate, byte_rate, align, bits = fmt
    n_frames = data_len // align if align else 0
    print(f"""
  fmt chunk（16 bytes，全部 little-endian）
    AudioFormat    {tag}   （1 = PCM，未壓縮）
    NumChannels    {ch}
    SampleRate     {rate} Hz          ← 取樣率 fs
    ByteRate       {byte_rate} bytes/s    = SampleRate × BlockAlign
    BlockAlign     {align} bytes          = NumChannels × BitsPerSample ÷ 8（一個時間點的所有聲道）
    BitsPerSample  {bits}                ← bit depth
  data 從第 {data_off} byte 開始，共 {data_len:,} bytes = {n_frames:,} 個時間點 = {n_frames / rate if rate else 0:.3f} 秒
  資料率 = {rate} × {bits} × {ch} = {rate * bits * ch:,} bits/s = {rate * bits * ch / 1000:.1f} kb/s""")

    if tag != 1 or bits != 16:
        print("\n  不是 16-bit PCM，以下的 sample 統計略過。")
        return
    body = d[data_off:data_off + data_len // 2 * 2]
    s = struct.unpack(f"<{len(body) // 2}h", body)
    if not s:
        return

    def entropy(counter, n):
        return -sum(c / n * math.log2(c / n) for c in counter.values())

    hs, hb = entropy(collections.Counter(s), len(s)), entropy(collections.Counter(body), len(body))
    k = len(set(s))
    print(f"""
  sample 值：最小 {min(s)}、最大 {max(s)}（16-bit 的範圍是 −32768 到 32767）；用掉了 {k:,} 種不同的值（最多 65,536 種）
  以 sample 為符號：H = {hs:.3f} bits／sample → 理論壓縮率 {hs / 16:.1%}；但 codebook 要記 {k:,} 種符號
  以 byte   為符號：H = {hb:.3f} bits／byte   → 理論壓縮率 {hb / 8:.1%}
""")
    bins = collections.Counter(min(15, (v + 32768) // 4096) for v in s)
    top = max(bins.values())
    print("  sample 值的 histogram（分成 16 格，每格寬 4096）")
    for b in range(16):
        lo = b * 4096 - 32768
        print(f"  {lo:>7d} ~ {lo + 4095:>6d} | {'#' * round(50 * bins.get(b, 0) / top):50s} {bins.get(b, 0):>9,d}")


if __name__ == "__main__":
    main()
