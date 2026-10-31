#!/usr/bin/env python3
"""smoke_check.py  —  make smoke 用：確認 voicelink_raw 的 send → recv 沒有改到任何一個 byte。

    python smoke_check.py <原始.wav> <收到的.wav>

    檢查：收到的 PCM 的前面 = 原始 PCM（逐 byte）；多出來的尾巴（最後一包補的 0）全部是 0，
    而且總長度 = 原始長度無條件進位到 640 的倍數。通過印 PASS、結束碼 0；否則印出哪裡不同、結束碼 1。
    WAV 一樣是照 chunk 走（data 不一定在第 44 byte）。
"""
import struct
import sys


def read_pcm(path):
    d = open(path, "rb").read()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        sys.exit(f"{path}: 不是 WAV")
    p = 12
    while p + 8 <= len(d):
        cid = d[p:p + 4]
        size = struct.unpack("<I", d[p + 4:p + 8])[0]
        if cid == b"data":
            return d[p + 8:p + 8 + size]
        p += 8 + size + (size & 1)
    sys.exit(f"{path}: 找不到 data chunk")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    a = read_pcm(sys.argv[1])
    b = read_pcm(sys.argv[2])
    frame = 640
    expect_len = (len(a) + frame - 1) // frame * frame
    if len(b) != expect_len:
        print(f"FAIL: 長度不對：原始 {len(a)} bytes（補到 640 的倍數 = {expect_len}），收到 {len(b)} bytes")
        sys.exit(1)
    if b[:len(a)] != a:
        first = next(i for i in range(len(a)) if a[i] != b[i])
        print(f"FAIL: 第 {first} 個 byte 開始不同（第 {first // frame} 包）")
        sys.exit(1)
    if any(b[len(a):]):
        print("FAIL: 最後一包補的部分不是 0")
        sys.exit(1)
    print(f"PASS: {len(a)} bytes 的 PCM 逐 byte 相同（{expect_len // frame} 包，最後一包補 {expect_len - len(a)} bytes 的 0）")


if __name__ == "__main__":
    main()
