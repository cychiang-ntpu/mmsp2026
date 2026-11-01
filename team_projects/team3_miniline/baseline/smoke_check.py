#!/usr/bin/env python3
"""smoke_check.py  —  make smoke 用：數 miniline_raw 的 video-send → video-recv 到了幾張、幾張逐 byte 相同。

    python smoke_check.py <原始資料夾> <收到的資料夾>

    原始資料夾裡是 make_sequence 產生的 0000.jpg、0001.jpg…；收到的資料夾是 video-recv 照「到達順序」寫的 NNNN.jpg。
    raw 版沒有 header，所以收到的第 k 個檔不一定是原始的第 k 張（中間掉了就會往前擠），這裡是用「內容」比對：
    收到的每一張，只要和原始的「任何一張」逐 byte 相同就算一張 identical。
    印出：原始幾張、收到幾張、幾張是合法 JPEG（FF D8 開頭、FF D9 結尾）、幾張逐 byte 相同。
    PASS 的條件很寬：至少收到 1 張合法 JPEG（127.0.0.1 上通常 20 張全到）；結束碼 0。否則 FAIL、結束碼 1。
"""
import os
import sys


def jpgs(d):
    try:
        names = sorted(n for n in os.listdir(d) if n.lower().endswith(".jpg"))
    except OSError:
        sys.exit(f"{d}: 不是資料夾")
    return [open(os.path.join(d, n), "rb").read() for n in names]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    orig = jpgs(sys.argv[1])
    got = jpgs(sys.argv[2])
    if not orig:
        print(f"FAIL: {sys.argv[1]} 裡沒有 .jpg")
        sys.exit(1)
    orig_set = set(orig)
    valid = sum(1 for b in got if b[:2] == b"\xff\xd8" and b[-2:] == b"\xff\xd9")
    identical = sum(1 for b in got if b in orig_set)
    print(f"原始 {len(orig)} 張；收到 {len(got)} 張，其中合法 JPEG {valid} 張、和原始逐 byte 相同 {identical} 張")
    if len(got) < len(orig):
        print(f"（少了 {len(orig) - len(got)} 張：整個 datagram 不見了——IP 分片只要掉一片，整張就沒有）")
    if valid == 0:
        print("FAIL: 一張合法的 JPEG 都沒收到")
        sys.exit(1)
    print(f"PASS: 收到 {valid} 張合法 JPEG（raw 版：{identical}/{len(orig)} 張逐 byte 相同）")


if __name__ == "__main__":
    main()
