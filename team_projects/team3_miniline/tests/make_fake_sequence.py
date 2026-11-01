#!/usr/bin/env python3
"""make_fake_sequence.py：用純 Python 產生一串「合法但沒有畫面」的測試用 JPEG 序列（0000.jpg、0001.jpg…），大小各不相同。

  python make_fake_sequence.py <outdir> [--count 60] [--seed 1] [--min-kb 3] [--max-kb 25]

每張都是一個手工組出來的 8×8 灰色 baseline JPEG（解碼器打得開），再塞進幾個隨機長度的 COM 註解段（FF FE）把大小撐到 3–25 KB，
所以每張的片數（⌈大小／1200⌉）都不一樣，和真的 MJPEG 一樣「每張大小不同」。
驗收測試只比 bytes（切片再接回來要逐 byte 相同），不看畫面；評測用的是 baseline/make_sequence（C）產生的真正影像（會動的圖形＋編號），
這支只是讓你在 make_sequence 還沒編好、或沒有 C 編譯器的機器上也能跑 run_video_tests.py。
每個檔案寫完都會檢查：開頭 FF D8、結尾 FF D9、COM 段的長度欄位對得上。只用標準函式庫。
"""
import argparse
import os
import random
import struct
import sys

# 最小的 baseline JPEG：8×8、單一灰階分量、量化表全 1、DC／AC 各一個 1-bit 的 Huffman 碼（DC 類別 0 = "0"、AC EOB = "0"）。
# 掃描資料只有 2 個 bit「0 0」（DC 差 0、然後 EOB），補 1 到一個 byte = 0x3F。整塊是 128 的灰。
SOI = b"\xff\xd8"
EOI = b"\xff\xd9"
DQT = b"\xff\xdb" + struct.pack(">H", 2 + 1 + 64) + b"\x00" + b"\x01" * 64
SOF0 = b"\xff\xc0" + struct.pack(">HBHHB", 2 + 1 + 2 + 2 + 1 + 3, 8, 8, 8, 1) + b"\x01\x11\x00"
DHT_DC = b"\xff\xc4" + struct.pack(">H", 2 + 1 + 16 + 1) + b"\x00" + b"\x01" + b"\x00" * 15 + b"\x00"
DHT_AC = b"\xff\xc4" + struct.pack(">H", 2 + 1 + 16 + 1) + b"\x10" + b"\x01" + b"\x00" * 15 + b"\x00"
SOS = b"\xff\xda" + struct.pack(">HB", 2 + 1 + 2 + 3, 1) + b"\x01\x00" + b"\x00\x3f\x00"
SCAN = b"\x3f"
HEAD = DQT + SOF0 + DHT_DC + DHT_AC
TAIL = SOS + SCAN + EOI


def comment(rng, n):
    """一個 COM 段：FF FE｜長度（含長度欄位本身 2 bytes）｜n bytes 隨機內容。n ≤ 65533。"""
    body = bytes(rng.getrandbits(8) for _ in range(n))
    return b"\xff\xfe" + struct.pack(">H", n + 2) + body


def make_jpeg(rng, target):
    """組出一張大約 target bytes 的 JPEG：SOI｜若干 COM｜表格｜掃描｜EOI。"""
    out = [SOI]
    size = len(SOI) + len(HEAD) + len(TAIL)
    while size < target:
        n = min(rng.randint(2000, 20000), target - size, 65533)
        if n < 1:
            break
        seg = comment(rng, n - 4 if n > 4 else 1)
        out.append(seg)
        size += len(seg)
    out.append(HEAD + TAIL)
    return b"".join(out)


def check_jpeg(data):
    """開頭 FF D8、結尾 FF D9、每個段的長度對得上。回傳 None 或錯誤訊息。"""
    if data[:2] != SOI or data[-2:] != EOI:
        return "開頭或結尾不對"
    pos = 2
    while pos < len(data) - 2:
        if data[pos] != 0xFF:
            return f"offset {pos} 不是段的開頭"
        marker = data[pos + 1]
        if marker == 0xDA:                                           # SOS 之後是掃描資料，直到 EOI
            return None
        seg_len = struct.unpack(">H", data[pos + 2:pos + 4])[0]
        pos += 2 + seg_len
    return None if pos == len(data) - 2 else "段的長度對不上"


def main():
    ap = argparse.ArgumentParser(description="產生合法但沒有畫面的 JPEG 序列（驗收測試用）")
    ap.add_argument("outdir")
    ap.add_argument("--count", type=int, default=60)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--min-kb", type=float, default=3)
    ap.add_argument("--max-kb", type=float, default=25)
    args = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    if args.count <= 0 or args.min_kb <= 0 or args.max_kb < args.min_kb:
        print("錯誤: --count 要是正整數、0 < --min-kb ≤ --max-kb", file=sys.stderr)
        return 2
    os.makedirs(args.outdir, exist_ok=True)
    rng = random.Random(args.seed)
    total = 0
    for i in range(args.count):
        target = int(rng.uniform(args.min_kb, args.max_kb) * 1024)
        data = make_jpeg(rng, target)
        why = check_jpeg(data)
        if why:
            print(f"錯誤: 第 {i} 張組出來不合法：{why}", file=sys.stderr)
            return 1
        with open(os.path.join(args.outdir, f"{i:04d}.jpg"), "wb") as f:
            f.write(data)
        total += len(data)
    print(f"{args.outdir}：{args.count} 張，共 {total:,} bytes，平均 {total / args.count / 1024:.1f} KB／張（seed={args.seed}）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
