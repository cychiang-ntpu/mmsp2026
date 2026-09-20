#!/usr/bin/env python3
"""chunk_send.py — 用「故意搗蛋的送法」測你們的 TextLink 收不收得對。只用標準函式庫。

先開你們的程式當監聽端：   textlink chat server 5000
再跑：                     python3 chunk_send.py <IP> <port> [sticky|drip|both]

  sticky  黏包：5 則訊息的 frame 接在一起，一次 sendall 送出
  drip    半包：把 frame 切成 1 byte 1 byte，每 byte 之間停 5 ms
  both    兩個都做（預設）

對方畫面應該出現「一則一則分開、內容完整」的訊息。第 2 週的 chat.c 做不到（它沒有長度前綴）；
你們的 frame 層做對了就可以。V（測試驗證）角色可以從這支程式開始，加上更多搗蛋的送法：
length 填 0、填 FF FF FF FF、type 亂填、送到一半就斷線……

frame 格式（規格固定）：length 4 bytes big-endian（= type + payload 的 bytes 數）｜type 1 byte｜payload
"""
import socket
import struct
import sys
import time

TEXT_RAW = 0x01


def frame(ftype, payload):
    return struct.pack(">I", len(payload) + 1) + bytes([ftype]) + payload     # ">I" = big-endian 32-bit 無號整數


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    ip, port = sys.argv[1], int(sys.argv[2])
    mode = sys.argv[3] if len(sys.argv) > 3 else "both"
    s = socket.create_connection((ip, port), timeout=10)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"已連到 {ip}:{port}")

    if mode in ("sticky", "both"):
        blob = b"".join(frame(TEXT_RAW, f"黏包測試 第{i}則".encode("utf-8")) for i in range(1, 6))
        s.sendall(blob)
        print(f"sticky：5 個 frame 共 {len(blob)} bytes，一次送出。對方應該看到 5 則。")
        time.sleep(1.0)

    if mode in ("drip", "both"):
        msg = "半包測試：多媒體😀𠮷👨‍👩‍👧 這一則被切成一個一個 byte 送"
        blob = frame(TEXT_RAW, msg.encode("utf-8"))
        for b in blob:
            s.send(bytes([b]))
            time.sleep(0.005)
        print(f"drip：1 個 frame 共 {len(blob)} bytes，分 {len(blob)} 次送出。對方應該看到完整的 1 則，emoji 不能破。")
        time.sleep(1.0)

    s.close()
    print("完成，連線已關閉。")


if __name__ == "__main__":
    main()
