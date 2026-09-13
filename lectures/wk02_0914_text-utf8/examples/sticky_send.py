#!/usr/bin/env python3
"""sticky_send.py — sticky_send.c 的 Python 對照版：對 chat.c 的 TCP server 連續 send，觀察黏包。

用法（三平台相同）：python3 sticky_send.py 127.0.0.1 5000 [則數=5] [間隔毫秒=0]
"""
import socket, sys, time

ip, port = sys.argv[1], int(sys.argv[2])
count = int(sys.argv[3]) if len(sys.argv) > 3 else 5
gap_ms = int(sys.argv[4]) if len(sys.argv) > 4 else 0

msgs = [f"第{i}則".encode("utf-8") for i in range(1, count + 1)]   # 沒有分隔符號，也沒有長度欄位
with socket.create_connection((ip, port)) as s:
    for i, msg in enumerate(msgs, 1):       # 迴圈裡只做 send，不 print：Python 比 C 慢，
        s.sendall(msg)                      # 多做一件事就可能讓對方先收走前一則而「不黏」
        if gap_ms:
            time.sleep(gap_ms / 1000)
    time.sleep(0.3)                         # 讓對方來得及收完再關
for i, msg in enumerate(msgs, 1):
    print(f"send #{i}: {len(msg)} bytes")
# 註：黏不黏取決於時序。間隔 0 時泡泡數會少於則數（常是 1 顆，有時 2 顆），每次跑可能不同。
