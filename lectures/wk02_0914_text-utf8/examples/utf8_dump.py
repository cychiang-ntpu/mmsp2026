#!/usr/bin/env python3
"""utf8_dump.py — utf8_dump.c 的 Python 對照版，輸出格式逐字元相同，供 diff 比對。

用法（三平台相同）：python3 utf8_dump.py ../data/sample_zh_en.txt   （Windows 用 python）

Python 走高階路線：整檔 bytes 讀進來後用 bytes.decode 一次解碼，
和 C 版「看前導 byte 決定長度」的做法不同，所以看得懂它也寫不出 C 版。
"""
import sys

def main():
    if len(sys.argv) != 2:
        sys.exit("用法: python3 utf8_dump.py <檔案>")
    data = open(sys.argv[1], "rb").read()          # rb：Windows 才不會把 \r\n 吃成 \n
    out = sys.stdout.buffer                        # 直接寫 bytes：三平台輸出完全相同
    idx = 0
    pos = 0
    while pos < len(data):
        # 從 1 到 4 bytes 逐一嘗試，第一個能解成「一個字元」的就是它；
        # 四種都失敗 → 非法 byte，當 1 byte 原樣印出（與 C 版一致）
        ch, raw = None, data[pos:pos + 1]
        for n in range(1, 5):
            try:
                ch = data[pos:pos + n].decode("utf-8")
                raw = data[pos:pos + n]
                break
            except UnicodeDecodeError:
                pass
        n = len(raw)
        cp = ord(ch) if ch is not None else raw[0]
        hexs = " ".join(f"{b:02X}" for b in raw)
        head = f"#{idx:<4} {n} byte{'s' if n > 1 else ' '}  hex: {hexs}{' ' * (3 * (4 - n))}  U+{cp:04X}  "
        special = {"\n": "'\\n'", "\r": "'\\r'", "\t": "'\\t'"}
        shown = special[ch].encode() if ch in special else (b"(BOM)" if cp == 0xFEFF and idx == 0 else raw)
        out.write(head.encode("utf-8") + shown + b"\n")
        idx += 1
        pos += n

if __name__ == "__main__":
    main()
