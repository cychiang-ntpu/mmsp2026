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
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第三節 3.3「長度前綴：把上週的黏包問題解掉」。
# 為什麼需要這支程式：TCP 是位元組串流，沒有「一則訊息」的概念。你 send 一次，對方不一定剛好 recv 一次：
#           可能好幾則黏在一起到（黏包），也可能一則被拆成好幾次才到（半包）。平常在同一台電腦上測很難遇到，
#           所以這支程式「故意」製造這兩種情況，檢查接收端是不是真的照 frame 的 length 在收。
# 怎麼跑：  見上面的說明（Windows 把 python3 換成 python）；不給參數會印出上面那段說明。
# 會看到：  這一端印出「已連到 …」「sticky：5 個 frame 共 125 bytes …」「drip：1 個 frame 共 95 bytes，分 95 次送出 …」
#           「完成，連線已關閉。」；真正要檢查的是對方的畫面：應該出現 5 則＋1 則完整的訊息，emoji 沒有變成亂碼。
#           對方沒有在監聽時，連線那一行會丟出例外（ConnectionRefusedError 或逾時），這是正常的。
# ──────────────────────────────────────────────────────────────────────────────────────
import socket
import struct
import sys
import time

# frame 的 type 欄位：0x01 在 Team 1 規格裡是 TEXT_RAW（沒有壓縮的文字訊息）。
TEXT_RAW = 0x01


# 把一則訊息包成一個 frame（講義 3.3 的圖）：[length 4 bytes][type 1 byte][payload]。
#   len(payload) + 1          length 欄位算的是「type + payload」的 bytes 數，所以要 +1（type 佔 1 byte）。
#   struct.pack(">I", 整數)   把整數變成 4 個 bytes。格式字串的兩個字：
#                             ">" = big-endian（高位的 byte 在前，網路上慣用的順序）；"I" = 無號 32-bit 整數。
#                             例：4 → 00 00 00 04。對照：WAV 檔用的是 "<"（little-endian），同一個 4 會是 04 00 00 00。
#   bytes([ftype])            用「只有一個整數的 list」做出長度 1 的 bytes。
#                             注意不能寫 bytes(ftype)：那會得到 ftype 個 0x00，是常見的陷阱。
#   三段 bytes 用 + 接起來。例：type = 0x01、payload = b"abc" → 00 00 00 04 01 61 62 63。
# payload 必須是 bytes（不是 str）：網路上傳的是位元組，文字要先用 .encode("utf-8") 轉換，
# length 算的也是 bytes 數而不是字元數（一個中文字是 3 bytes、emoji 是 4 bytes）。
def frame(ftype, payload):
    return struct.pack(">I", len(payload) + 1) + bytes([ftype]) + payload     # ">I" = big-endian 32-bit 無號整數


def main():
    # 參數不夠時印出用法。__doc__ 是這個檔案最上面那段三引號字串（docstring），
    # 直接拿來當說明文字，就不必同樣的內容寫兩次。sys.exit(字串) 會把它印到 stderr 再結束程式。
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    # 命令列參數都是字串；port 要轉成整數。第三個參數可以省略，預設 "both"。
    ip, port = sys.argv[1], int(sys.argv[2])
    mode = sys.argv[3] if len(sys.argv) > 3 else "both"

    # 建立 TCP 連線（這支程式是 client）。(ip, port) 是一個 tuple，所以有兩層括號；
    # timeout=10：連不上、或之後的傳送卡住超過 10 秒就放棄並丟出例外，不會永遠等下去。
    s = socket.create_connection((ip, port), timeout=10)
    # 關掉 Nagle 演算法。作業系統預設會把很小的資料「攢一下再一起送」來省封包，
    # 那樣下面 1 byte 1 byte 的送法就會被合併回去，失去測試的意義；TCP_NODELAY = 1 要求每次 send 都盡快送出。
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"已連到 {ip}:{port}")

    # ── 測試一：黏包 ──
    # mode in ("sticky", "both")：mode 是這兩個字串之一就成立。
    if mode in ("sticky", "both"):
        # 先在記憶體裡把 5 個 frame 接成一大塊（5 ×（4 + 1 + 20）= 125 bytes），再「一次」送出去。
        #   f"黏包測試 第{i}則"   f-string，把 i 填進字串；.encode("utf-8") 把 str 轉成 bytes；
        #   ( … for i in range(1, 6) )  generator expression，i = 1、2、3、4、5（range 不含結尾的 6）；
        #   b"".join(…)           把 5 段 bytes 接起來，b"" 表示中間不夾任何東西。
        blob = b"".join(frame(TEXT_RAW, f"黏包測試 第{i}則".encode("utf-8")) for i in range(1, 6))
        # sendall 會一直送到整塊都送完為止（send 只保證送出「一部分」，見下面）。
        # 對方很可能一次 recv 就拿到全部 125 bytes：沒有照 length 去切的程式，會把它當成一則、或印出亂碼。
        s.sendall(blob)
        print(f"sticky：5 個 frame 共 {len(blob)} bytes，一次送出。對方應該看到 5 則。")
        # 停 1 秒，讓對方有時間處理並顯示，兩個測試在對方的畫面上才不會混在一起。
        time.sleep(1.0)

    # ── 測試二：半包 ──
    if mode in ("drip", "both"):
        # 這則訊息刻意放進各種長度的 UTF-8 字元：中文 3 bytes、😀 與 𠮷 各 4 bytes、
        # 👨‍👩‍👧 更是由 5 個 code point（三個 emoji 中間用 U+200D 黏起來）組成，共 18 bytes。
        # 一個字元的幾個 bytes 會「分好幾次」才到齊：接收端如果每 recv 一次就急著當成文字印出來，字就會破掉。
        msg = "半包測試：多媒體😀𠮷👨‍👩‍👧 這一則被切成一個一個 byte 送"
        blob = frame(TEXT_RAW, msg.encode("utf-8"))
        # 用 for 走訪 bytes 時，拿到的 b 是 0–255 的「整數」，不是長度 1 的 bytes，
        # 所以要再用 bytes([b]) 包回去才能 send。每送 1 byte 停 0.005 秒（5 ms），確保它們是分開到的。
        # 連 4 bytes 的 length 欄位也是一個 byte 一個 byte 到：接收端要先「收滿」標頭的 5 bytes，
        # 才能算 payload 的長度，再「收滿」payload（講義 3.3 的 recv_all）。
        for b in blob:
            s.send(bytes([b]))
            time.sleep(0.005)
        print(f"drip：1 個 frame 共 {len(blob)} bytes，分 {len(blob)} 次送出。對方應該看到完整的 1 則，emoji 不能破。")
        time.sleep(1.0)

    s.close()
    print("完成，連線已關閉。")


# 直接執行這個檔案時才呼叫 main()；被別的程式 import 時不會自動執行。
if __name__ == "__main__":
    main()
