#!/usr/bin/env python3
"""TextLink 的純 Python 完整版：功能與 C 版的目標相同，用來「對照著看」。只用標準函式庫。

  python3 textlink.py chat server <port> [--bind <ip>] [--raw|--huff]      聊天：等對方連進來
  python3 textlink.py chat client <ip> <port> [--raw|--huff]               聊天：連到對方
  python3 textlink.py recv <port> <outdir> [--bind <ip>]                   收一個檔案
  python3 textlink.py send <ip> <port> <file> [--raw|--huff]               送一個檔案
  python3 textlink.py inspect <file>                                       不連線：分析一個檔案（N、K、熵 H、平均碼長 L、codebook、壓縮率）
  任何指令都可以加 --probe：把中間過程印出來（切出的符號、機率最高的符號與 code、區塊的每個欄位、每一個 frame）

這支程式刻意用「高階」的寫法，讓你一眼看到每一步在做什麼：
  統計機率   collections.Counter(symbols)
  建樹       heapq：每次 pop 兩個最小的，合併後 push 回去
  位元打包   先串成 "0101…" 的字串，再 int(bits, 2).to_bytes(…)
  解碼       從短到長試 bits[p:p+長度] 在不在 code 表裡（prefix code 保證第一個找到的就是對的）
C 沒有這些現成的東西：Counter 要自己用陣列數、heapq 要自己維護排序或 heap、位元要自己一個一個塞進 byte、字串切片要換成逐 bit 讀。
把這裡的每一行「翻譯」成 C，就是 Team 1 的工作；觀念一樣，但程式不會長得一樣，也不要逐行照翻——Python 的寫法在 C 裡既慢又浪費記憶體。

格式（frame、FILE_BEGIN、Huffman 區塊）與 starter 的殼、課程的參考程式相同，所以 Python 版可以和 C 版互連；
Huffman 區塊的格式是「一種可行的設計」，不是規定：你們可以設計自己的（codebook 存得更省就是加分方向），寫進 docs/interface.md 即可。
"""
# ---------------------------------------------------------------------------------------------------------------
# 怎麼讀這支程式：由下層往上層分成七個區塊，每一塊只用到它上面的區塊。
#   1. 切符號   bytes → 符號的 list（UTF-8 字元／16-bit sample／byte）
#   2. Huffman  統計 → code 長度 → canonical code → 區塊；解碼反過來
#   3. frame    length 4 bytes｜type 1 byte｜payload
#   4. 連線     監聽／連線
#   5. 傳檔     FILE_BEGIN → FILE_DATA × N → FILE_END
#   6. 聊天     兩條執行緒：一條讀鍵盤、一條收訊息
#   7. inspect  離線分析；最後是 main（解析命令列）
#
# 和 starter 的五個 place holder 怎麼對：（註解裡凡是寫「★ 對應 C」的地方，就是你們要寫的那一段）
#   frame_pack_header   → send_frame 裡的 struct.pack(">I", …) + bytes([ftype])
#   frame_parse_header  → recv_frame 裡的 struct.unpack(">IB", …) 與緊接的範圍檢查
#   utf8_validate       → bytes.decode("utf-8")（嚴格解碼）：出現在 split_symbols 與 chat_receiver
#   huff_encode         → huff_encode（用到 split_symbols、Counter、code_lengths、canonical_codes）
#   huff_decode         → huff_decode（用到 canonical_codes、join_symbols）
#
# 建議的讀法：先跑 python3 textlink.py --probe inspect 某個短的.txt，一邊看輸出、一邊從 huff_encode 讀起。
# ---------------------------------------------------------------------------------------------------------------
import argparse
import collections
import heapq
import math
import os
import re
import socket
import struct
import sys
import threading
import time
import unicodedata

# 常數。a, b = 1, 2 是一次指定好幾個變數的寫法（tuple unpacking），下面大量使用。
#   PROBE         main 讀到 --probe 才改成 True（全域變數）
#   T_*           frame 的 type，數值是規格訂的（C 版在 textlink.h）
#   MAX_FRAME     frame 的 length 上限 16 MiB；CHUNK：一個 FILE_DATA 最多 64 KiB；MAX_FILE：檔案上限 64 MiB；
#                 MAX_TEXT：一則聊天訊息最多 4095 bytes
#   SYM_*         三種符號的代號，也是 Huffman 區塊第 1 個 byte 的值（C 版的 tl_sym_t）
#   MAX_CODE_LEN  code 長度的上限。Python 的整數沒有上限，其實用不到；訂 56 是為了和 C 互通：
#                 C 的 bit writer 若用 64-bit 整數當累加器，裡面留著最多 7 bits，再放進 56 bits 共 63 bits，才不會溢位。
PROBE = False
T_TEXT_RAW, T_TEXT_HUFF, T_FILE_BEGIN, T_FILE_DATA, T_FILE_END = 0x01, 0x02, 0x10, 0x11, 0x12
MAX_FRAME, CHUNK, MAX_FILE, MAX_TEXT = 16 * 1024 * 1024, 64 * 1024, 64 * 1024 * 1024, 4095
SYM_BYTE, SYM_CHAR, SYM_S16 = 0, 1, 2
SYM_NAME = {SYM_BYTE: "byte", SYM_CHAR: "char", SYM_S16: "s16"}
SYM_WIDTH = {SYM_BYTE: 1, SYM_CHAR: 3, SYM_S16: 2}          # codebook 裡一個符號佔幾 bytes
MAX_CODE_LEN = 56


# --probe 的輸出都經過這個函式：第一行是 "[probe] 標題"，其餘每行縮排 10 格。
#   *lines             收下「任意多個」參數，在函式裡是一個 tuple
#   print(a, *list)    反過來：把 list 拆開成一個一個參數；sep="\n" 讓每個參數各佔一行
#   file=sys.stderr    印到 stderr，不和正常輸出（stdout）混在一起；用 > 把結果存檔時，probe 的內容不會跑進檔案
def probe(title, *lines):
    if PROBE:
        print(f"[probe] {title}", *[f"          {x}" for x in lines], sep="\n", file=sys.stderr)


# 把符號印成人看得懂的樣子（只給 probe 用）：
#   char → "U+591A '多'"（不可印的字元只印編號）；s16 → 有號的 sample 值，例如 "-16"；byte → "0x41"
# s16 的符號在程式裡是無號的 0–65535（方便當索引），32768 以上其實是負數，減 65536 就換回有號的值。
# f-string 的格式：{sym:04X} 至少 4 位的 16 進位、不足補 0；{ch!r} 印成帶引號的樣子；{x:+d} 一定印出正負號。
def show_symbol(sym, kind):
    if kind == SYM_CHAR:
        ch = chr(sym)
        return f"U+{sym:04X} {ch!r}" if ch.isprintable() else f"U+{sym:04X}"
    if kind == SYM_S16:
        return f"{sym - 65536 if sym >= 32768 else sym:+d}"
    return f"0x{sym:02X}"


# =============================================================== 1. 切符號：對「什麼東西」統計機率
# ★ 對應 C：huff_encode 的第 0 步。Huffman 是對「符號的機率」編碼，所以第一件事是決定符號是什麼、把 bytes 切成符號。

# WAV（RIFF）的結構：12 bytes 的 "RIFF"｜檔案大小｜"WAVE"，之後是一串 chunk；
# 每個 chunk = id 4 bytes｜size 4 bytes（little-endian）｜內容 size bytes｜（size 是奇數時多 1 byte 補位）。
# data chunk 不一定在第 44 byte：fmt 與 data 之間可能夾著 LIST 等 chunk，所以用迴圈一個一個跳過去。
#   struct.unpack("<I", 4 bytes)[0]    "<" = little-endian、"I" = 32-bit 無號整數；unpack 一律回傳 tuple，所以取 [0]
#   struct.unpack("<HHIIHH", 16 bytes) fmt chunk 的前 16 bytes："H" = 16-bit 無號。六個欄位依序是
#                                      格式代碼（1 = PCM）、聲道數、取樣率、每秒 bytes、block align、每個 sample 幾 bits；
#                                      用不到的欄位用 _ 接住
#   C 沒有 struct.unpack：要自己寫 p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24。
# 回傳的 end 已經處理兩種狀況：size 比檔案實際剩下的還大（檔案被截斷、或串流錄音沒回填大小）→ 用 min 取實際長度；
# 長度是奇數 → // 2 * 2 往下取偶數。
# raise ValueError(...) 是 Python 回報「這份資料不適用」的方式，相當於 C 版回傳 TL_ERR_DATA；呼叫端用 try／except 接。
def wav_data_range(data):
    """回傳 16-bit PCM WAV 的 data 區 [start, end)。逐個 chunk 走：data 不一定在第 44 byte。"""
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("不是 RIFF/WAVE")
    pos, fmt_ok = 12, False
    while pos + 8 <= len(data):
        chunk_id, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        if chunk_id == b"fmt ":
            if size < 16 or pos + 24 > len(data):          # fmt 不到 16 bytes：壞掉的檔案，交給呼叫者改用 byte 當符號
                raise ValueError("fmt chunk 太短")
            audio_format, _, _, _, _, bits = struct.unpack("<HHIIHH", data[pos + 8:pos + 24])
            if audio_format != 1 or bits != 16:
                raise ValueError("不是 16-bit PCM")
            fmt_ok = True
        elif chunk_id == b"data":
            if not fmt_ok:
                raise ValueError("data 之前沒有 fmt")
            start = pos + 8
            return start, start + min(size, len(data) - start) // 2 * 2         # 奇數個 byte 時，最後 1 byte 不算 sample
        pos += 8 + size + (size & 1)
    raise ValueError("找不到 data chunk")


# 三種符號各一行：
#   char  data.decode("utf-8") 把 bytes 嚴格解成字串，ord(ch) 取出每個字元的 code point（整數）。
#         ★ 對應 C：utf8_validate 的規則全在這個 decode 裡——Python 替你拒絕了 overlong（例 C0 AF）、
#         代理區（例 ED A0 80）、超過 U+10FFFF（例 F4 90 80 80）與被截斷的序列。C 要自己看前導 byte、續位元組，再檢查這三條。
#   s16   f"<{n}H" 會變成像 "<4H" 的格式字串：一次把 n 個 little-endian 的 16-bit 無號整數解出來。
#         data 區之前的 bytes（head）與之後的（tail）不是 sample，原樣帶著走，還原時才能逐 byte 相同。
#   byte  list(bytes) 直接得到 0–255 的整數 list。
# 回傳 (symbols, head, tail)；只有 s16 的 head、tail 不是空的。
def split_symbols(data, kind):
    """bytes → (符號的 list, 前面原樣保留的 bytes, 後面原樣保留的 bytes)。資料不適用就丟 ValueError。"""
    if kind == SYM_CHAR:
        return [ord(ch) for ch in data.decode("utf-8")], b"", b""          # 嚴格解碼：非法 UTF-8 會丟 UnicodeDecodeError（ValueError 的一種）
    if kind == SYM_S16:
        start, end = wav_data_range(data)
        return list(struct.unpack(f"<{(end - start) // 2}H", data[start:end])), data[:start], data[end:]
    return list(data), b"", b""


# split_symbols 的反運算（解碼的最後一步）：符號的 list → bytes。
#   char：chr 把 code point 換回字元，接成字串再 encode。合法的 UTF-8 每個字元只有一種編法，所以一定和原文逐 byte 相同
#   s16 ：struct.pack(格式, *symbols) 的 * 把 list 拆成一個一個參數
def join_symbols(symbols, kind, head, tail):
    if kind == SYM_CHAR:
        return "".join(map(chr, symbols)).encode("utf-8")
    if kind == SYM_S16:
        return head + struct.pack(f"<{len(symbols)}H", *symbols) + tail
    return bytes(symbols)


# =============================================================== 2. Huffman
# ★ 對應 C：huff_encode（TODO 4）與 huff_decode（TODO 5）。流程：
#   編碼  split_symbols → Counter 統計 → code_lengths（建樹，只留長度）→ canonical_codes（由長度指派 code）
#         → 串成位元、打包成 bytes → 前面加上檔頭與 codebook
#   解碼  讀檔頭與 codebook（逐欄檢查）→ canonical_codes 重建同一張表 → 逐個 code 查表 → join_symbols

# 建樹。heapq 把一個普通的 list 當成 min-heap 用：heappop 取出最小的、heappush 放進新的，各 O(log K)。
# heap 裡的元素是 tuple (次數, 進場順序, [底下的符號])。tuple 比大小是「先比第 0 項，一樣再比第 1 項……」：
#   第 0 項 次數      → 次數小的先出來
#   第 1 項 進場順序  → 平手時的規則。葉依符號值編號 0、1、2……，新節點接著往後編，所以平手時先取葉、葉之間符號值小的先。
#                       每個節點的編號都不同，比較一定在這一項分出勝負，不會比到第 2 項的 list
#   第 2 項 這個節點底下所有的符號。不需要真的建出樹的節點：每合併一次，底下每個符號離根又遠一層，長度 +1 就好
# 語法：enumerate(x) 產生 (0, 第一個)、(1, 第二個)……；sorted(freq.items()) 把 (符號, 次數) 依符號值排序；
#       dict.fromkeys(freq, 0) 做出「每個符號 → 0」的 dict；next(iter(freq)) 取出唯一的那個 key。
# 例 "ABRACADABRA"：起始 (5,0,[A]) (2,1,[B]) (1,2,[C]) (1,3,[D]) (2,4,[R])
#   第 1 輪 C + D → (2,5,[C,D])      第 2 輪 B + R → (4,6,[B,R])    ← (2,1,[B])、(2,4,[R]) 比 (2,5,[C,D]) 先出來
#   第 3 輪 [C,D] + [B,R] → (6,7,…)  第 4 輪 A + 其餘 → 只剩一個，結束。長度：A = 1，B、C、D、R = 3
# ★ 對應 C：不要照這個寫法。group1 + group2 每次都複製 list，符號多、樹又深的時候很慢；在節點裡放 list 在 C 也很麻煩。
#   C 的做法是節點陣列（每個節點記 weight 與 parent），建完之後沿 parent 算每個葉的深度。
#   「取最小的兩個」不能每輪線性找（K 可以到幾萬）：用 heap，或先排序一次、再用兩個佇列（葉一個、內部節點一個）。
def code_lengths(freq):
    """Counter → {符號: code 長度}。heap 裡放 (次數, 進場順序, [這個節點底下的符號])；合併一次，底下每個符號的長度 +1。"""
    if len(freq) == 1:
        return {next(iter(freq)): 1}                                        # 只有一種符號：長度不能是 0
    heap = [(count, order, [sym]) for order, (sym, count) in enumerate(sorted(freq.items()))]
    heapq.heapify(heap)
    length = dict.fromkeys(freq, 0)
    order = len(heap)
    while len(heap) > 1:
        c1, _, group1 = heapq.heappop(heap)
        c2, _, group2 = heapq.heappop(heap)
        for sym in group1 + group2:
            length[sym] += 1
        heapq.heappush(heap, (c1 + c2, order, group1 + group2))
        order += 1
    return length


# canonical Huffman：同一組長度可以配出很多種 code（左右分支怎麼擺都行）。只要兩端約定同一條指派規則，
# codebook 就只需要傳「每個符號的長度」（1 byte），不必傳 code 本身，接收端照同一條規則就能重建一模一樣的表。
# 編碼與解碼都呼叫這個函式，就是這個原因。
# 例：長度 A1 B3 C3 D3 R3 → 排序後 A B C D R
#   A：value = 0            → "0"
#   B：長度 1 → 3，value = 1 左移 2 → 4  → "100"      C："101"   D："110"   R："111"
# format(value, "03b") 把整數印成至少 3 位的 2 進位字串、不足的左邊補 0；這裡的寬度 prev 是變數，所以用 f"0{prev}b" 組出來。
# ★ 對應 C：code 存成整數（例如 uint64_t）加一個長度，不要存成字串。
def canonical_codes(length):
    """{符號: 長度} → {符號: "0101" 字串}。依（長度, 符號值）排序，第一個是全 0，之後每次 +1，長度變長就在右邊補 0。"""
    codes, value, prev = {}, 0, 0
    for sym in sorted(length, key=lambda s: (length[s], s)):
        value <<= length[sym] - prev
        prev = length[sym]
        codes[sym] = format(value, f"0{prev}b")
        value += 1
    return codes


# 熵 H = Σ p·log2(1/p)，p = c/n，所以 log2(1/p) 寫成 log2(n/c)。單位是 bits／符號，是這種符號定義下平均碼長的下限。
# 只用來顯示與寫報告，編碼本身用不到。
def entropy(freq):
    n = sum(freq.values())
    return sum(c / n * math.log2(n / c) for c in freq.values()) if n else 0.0


def huff_encode(data, kind):
    """bytes → 自己帶 codebook 的區塊。格式（多 byte 欄位都是 big-endian）：
         kind 1｜原始長度 8｜符號個數 8｜符號種類數 K 4｜K 組（符號 W bytes、code 長度 1 byte），依符號值遞增｜
         （只有 s16）head 長度 4、head、tail 長度 4、tail｜bitstream（高位先出，最後補 0）"""
    # ★ 對應 C：huff_encode（TODO 4）。下面每一行就是 C 版的一個步驟。
    # 切符號。資料不適用這種符號時 split_symbols 會丟 ValueError，直接往上傳給 encode_auto 處理（C：回傳 TL_ERR_DATA）。
    symbols, head, tail = split_symbols(data, kind)
    # 統計。Counter 是「會數數的 dict」：Counter("ABRA") → {'A': 2, 'B': 1, 'R': 1}。
    # C：以符號值為索引的陣列，count[sym]++；byte 256 格、s16 65,536 格、char 0x110000 格。
    freq = collections.Counter(symbols)
    # 空輸入：沒有符號也就沒有樹，length 是空的 dict，後面每一步自然都得到空的結果（K = 0、沒有 bitstream）。
    length = code_lengths(freq) if freq else {}
    codes = canonical_codes(length)
    # 位元打包，Python 的偷懶寫法，分三步：
    #   (1) 把每個符號的 code（"0"／"1" 字串）依序接起來，例 "ABRACADABRA" → "01001110101011001001110"（23 bits）
    #   (2) 補 0 到 8 的倍數。-len(bits) % 8 算出要補幾個：Python 的 % 結果不會是負的，-23 % 8 = 1
    #   (3) int(padded, 2) 把整串當成一個 2 進位的大整數，.to_bytes(幾 bytes, "big") 再切成 bytes，高位在前 → 4E AC 9C。
    #       byte 數要明講，開頭的 0 才不會不見；空字串不能轉成整數，所以另外處理成 b""
    # 先接起來的 code 落在高位，所以每個 byte 是由高位往低位填（MSB-first）。
    # C：沒有任意長度的整數，1 MB 的檔案也不可能先串成幾百萬個字元的字串。要寫 bit writer：
    #    一個整數累加器 acc 與位元數 n；放進一個 code 是 acc = (acc << len) | code，n 滿 8 就輸出最高的 8 bits。
    bits = "".join(codes[s] for s in symbols)
    padded = bits + "0" * (-len(bits) % 8)
    stream = int(padded, 2).to_bytes(len(padded) // 8, "big") if padded else b""

    # 組出區塊（欄位順序見上面的 docstring）。
    #   sym.to_bytes(width, "big")   整數 → 固定 width bytes 的 big-endian；char 要 3 bytes 才放得下 U+10FFFF
    #   bytes([x])                   一個 0–255 的整數 → 長度 1 的 bytes（和 struct.pack("B", x) 一樣）
    #   struct.pack(">I", n)         ">" = big-endian，"I" = 32-bit 無號整數 → 4 bytes
    #   struct.pack(">QQI", a, b, c) "Q" = 64-bit 無號整數 → 8 + 8 + 4 = 20 bytes；加上 kind 1 byte 就是 21 bytes 的固定檔頭
    #   用 ">" 或 "<" 開頭時欄位之間不會插入對齊用的空 bytes；C 的 struct 會，所以 C 不可以直接把 struct 寫出去。
    # codebook 依符號值遞增（sorted(length)），每組是「符號＋長度」；沒有存 code，接收端用 canonical_codes 重建。
    # C：沒有 struct.pack，用位移一個 byte 一個 byte 寫：p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
    width = SYM_WIDTH[kind]
    book = b"".join(sym.to_bytes(width, "big") + bytes([length[sym]]) for sym in sorted(length))
    extra = struct.pack(">I", len(head)) + head + struct.pack(">I", len(tail)) + tail if kind == SYM_S16 else b""
    block = bytes([kind]) + struct.pack(">QQI", len(data), len(symbols), len(length)) + book + extra + stream

    # --probe 印出來的東西（以 "ABRACADABRA" 為例）：
    #   [probe] huff_encode：符號＝char，N=11，K=5，H=2.0404，L=2.0909 bits／符號
    #       N 符號個數、K 符號種類數、H 熵、L 平均碼長（= bitstream 的 bits 數 ÷ N）。檢查 H ≤ L < H + 1
    #   U+0041 'A'  次數 5  p=0.4545  理想 1.14 bits  code 0
    #       次數最多的前 8 種符號（most_common(8)）。「理想」是 log2(1/p)；Huffman 的長度只能是整數，這裡給了 1 bit
    #   區塊 44 bytes = 檔頭 21 + codebook 20 + 原樣保留 0 + bitstream 3；原始 11 bytes → 400.00%
    #       壓完的每一部分各佔多少（s16 的「原樣保留」含兩個 4 bytes 的長度欄位）。這個例子 codebook 比資料還大，所以反而膨脹
    if PROBE:
        n, top = len(symbols), freq.most_common(8)
        avg = len(bits) / n if n else 0.0
        probe(f"huff_encode：符號＝{SYM_NAME[kind]}，N={n:,}，K={len(freq):,}，H={entropy(freq):.4f}，L={avg:.4f} bits／符號",
              *[f"{show_symbol(s, kind):>14s}  次數 {c:>8,d}  p={c / n:.4f}  理想 {math.log2(n / c):5.2f} bits  code {codes[s]}" for s, c in top],
              f"區塊 {len(block):,} bytes = 檔頭 21 + codebook {len(book):,} + 原樣保留 {len(extra):,} + bitstream {len(stream):,}"
              f"；原始 {len(data):,} bytes → {100 * len(block) / max(1, len(data)):.2f}%")
    return block


def huff_decode(block, max_out):
    """huff_encode 的反運算。block 是對方送來的，每個欄位都要先檢查再用；不合理就丟 ValueError。"""
    # ★ 對應 C：huff_decode（TODO 5）。這個函式一半以上的行數是檢查，這是刻意的：
    # block 來自網路，裡面每個數字都是「對方說的」，可能壞掉、也可能是有人故意捏造。
    # Python 裡切片超出範圍只會得到比較短的結果、索引超出範圍會丟例外；C 裡同樣的錯誤是讀寫到陣列外面
    # （當機，或被人利用來入侵），而且照著對方給的數字去 malloc 可能一次要走幾 GB。
    # 所以 C 版「每個欄位先檢查、再使用」比這裡更重要；下面每一個 raise，在 C 都是一條 return TL_ERR_DATA 的路。

    # 固定檔頭 21 bytes = kind 1 + ">QQI" 的 8 + 8 + 4。先確認長度夠才 unpack。
    if len(block) < 21 or block[0] > 2:
        raise ValueError("區塊太短或符號種類不對")
    kind = block[0]
    orig_len, n_syms, k = struct.unpack(">QQI", block[1:21])
    width = SYM_WIDTH[kind]
    # 數字之間的合理性：還原後的大小不可超過呼叫端給的上限 max_out；每個符號至少還原成 1 byte，所以符號數 ≤ bytes 數；
    # codebook（K 組，每組 width + 1 bytes）必須整個在區塊裡；n_syms 與 K 要嘛都是 0（空輸入），要嘛都不是。
    if orig_len > max_out or n_syms > orig_len or k * (width + 1) > len(block) - 21 or (n_syms > 0) != (k > 0):
        raise ValueError("檔頭的數字不合理")

    # 讀 codebook，逐組檢查：
    #   符號嚴格遞增（prev 記上一個，初值 -1）→ 同時保證沒有重複的符號
    #   長度 1–56；not 1 <= x <= 56 是 Python 的連續比較，C 要寫成 x < 1 || x > 56
    #   char 的符號必須是合法的 code point，否則最後的 chr(…) 或 encode("utf-8") 會出錯（C：會編出非法的 UTF-8）
    #   Kraft 不等式 Σ 2^(−長度) ≤ 1：任何 prefix code 都滿足；超過 1 表示這組長度湊不出 prefix code
    #   （例：三個符號長度都是 1，1/2 × 3 > 1）。這裡用浮點數算，所以留一點誤差；C 可以同乘 2^56 改用整數算，沒有誤差。
    length, prev, pos = {}, -1, 21
    for _ in range(k):
        sym, code_len = int.from_bytes(block[pos:pos + width], "big"), block[pos + width]
        if sym <= prev or not 1 <= code_len <= MAX_CODE_LEN:
            raise ValueError("codebook：符號沒有遞增，或長度不在 1–56")
        if kind == SYM_CHAR and (sym > 0x10FFFF or 0xD800 <= sym <= 0xDFFF):
            raise ValueError("codebook：不是合法的 code point")
        length[sym], prev, pos = code_len, sym, pos + width + 1
    if sum(2.0 ** -l for l in length.values()) > 1.0 + 1e-12:
        raise ValueError("codebook：違反 Kraft 不等式，建不出 prefix code")

    # s16：head、tail 各是「長度 4 bytes big-endian＋內容」。長度同樣是對方說的：先確認 4 bytes 讀得到，
    # 再確認內容沒有超出區塊，才切出來。最後三段的大小加起來必須剛好等於 orig_len（每個 sample 2 bytes）。
    head = tail = b""
    if kind == SYM_S16:
        for which in ("head", "tail"):
            if pos + 4 > len(block):
                raise ValueError("s16：head／tail 不完整")
            n = struct.unpack(">I", block[pos:pos + 4])[0]
            if pos + 4 + n > len(block):
                raise ValueError("s16：head／tail 超出區塊")
            if which == "head":
                head = block[pos + 4:pos + 4 + n]
            else:
                tail = block[pos + 4:pos + 4 + n]
            pos += 4 + n
        if len(head) + 2 * n_syms + len(tail) != orig_len:
            raise ValueError("s16：大小對不起來")
    elif kind == SYM_BYTE and n_syms != orig_len:
        raise ValueError("byte：符號個數應該等於原始長度")

    # 解碼。table 是反過來的表：code 字串 → 符號；sizes 是實際出現過的 code 長度，由短到長。
    # bits 把整個 bitstream 變回 "0101…" 字串：bin(整數) 會得到 "0b1001…"，[2:] 去掉 "0b"；
    # 轉成整數時開頭的 0 不見了，用 zfill(總 bits 數) 補回來。
    # 每個符號：從目前位置 p 由短到長切一段去查表。prefix code 的定義就是「沒有一個 code 是另一個的開頭」，
    # 所以短的先查到就一定是對的，不會誤判。for … else：迴圈沒有被 break（每種長度都查不到）才會執行 else。
    # 只解 n_syms 個就停，最後補的 0 不會被當成符號。
    # C：不能把幾百萬個 bits 變成字串。要寫 bit reader 一次讀 1 bit（第 pos 個 bit 在第 pos / 8 個 byte、由高位數來
    #    第 pos % 8 個），每讀 1 bit 沿著樹走一步；或利用 canonical code「同長度的 code 是連續整數」的性質，
    #    只靠「每種長度有幾個 code」就能判斷目前讀到的是不是一個完整的 code。
    table = {code: sym for sym, code in canonical_codes(length).items()}
    sizes = sorted(set(map(len, table)))
    stream = block[pos:]
    bits = bin(int.from_bytes(stream, "big"))[2:].zfill(len(stream) * 8) if stream else ""
    symbols, p = [], 0
    for _ in range(n_syms):
        for size in sizes:                                                  # 從短到長試；prefix code 保證第一個找到的就是對的
            sym = table.get(bits[p:p + size]) if p + size <= len(bits) else None
            if sym is not None:
                symbols.append(sym)
                p += size
                break
        else:
            raise ValueError("bitstream 不夠長，或出現不在 codebook 裡的 code")
    # 最後對帳：還原出來的 bytes 數必須等於檔頭宣稱的 orig_len（char 的一個符號是 1–4 bytes，只能解完才知道）。
    # C：輸出緩衝區的大小就是 orig_len，所以「每次寫入之前」就要確認不會超過，不能等到最後才比。
    data = join_symbols(symbols, kind, head, tail)
    if len(data) != orig_len:
        raise ValueError("解出來的長度與宣稱的不符")
    probe(f"huff_decode：符號＝{SYM_NAME[kind]}，{n_syms:,} 個符號、{k:,} 種 → {len(data):,} bytes")
    return data


# 依副檔名選符號：.wav → s16、.txt → char、其他 → byte。a if 條件 else b 可以像這樣連著寫。
def kind_for(path):
    return SYM_S16 if path.lower().endswith(".wav") else SYM_CHAR if path.lower().endswith(".txt") else SYM_BYTE


# try／except 接住 huff_encode 裡丟出來的 ValueError（UnicodeDecodeError 也是 ValueError 的一種）。
# ★ 對應 C：starter 的殼做同一件事——huff_encode 回傳 TL_ERR_DATA 時，改用 SYM_BYTE 再呼叫一次。
# 回傳 (區塊, 實際用的符號種類)；STATS 的 sym= 要印實際用的那一種。
def encode_auto(data, kind):
    """照副檔名選的符號不適用（.txt 不是合法 UTF-8、.wav 不是 16-bit PCM）時，退回以 byte 為符號。"""
    try:
        return huff_encode(data, kind), kind
    except ValueError as why:
        probe(f"以 {SYM_NAME[kind]} 為符號不適用（{why}），改用 byte")
        return huff_encode(data, SYM_BYTE), SYM_BYTE


# =============================================================== 3. frame：length 4 bytes big-endian｜type 1 byte｜payload
# TCP 是 byte stream，沒有「一則訊息」的邊界：對方送兩次，這邊收一次可能拿到一則半（黏包）或半則（半包）。
# 所以每則訊息前面先用 4 bytes 講「接下來有幾 bytes」。length = type 的 1 byte + payload 的 bytes 數，不含 length 自己。
# 例：TEXT_RAW 的「嗨」（E5 97 A8）→ 00 00 00 04 | 01 | E5 97 A8。

# ★ 對應 C：struct.pack(">I", len(payload) + 1) + bytes([ftype]) 這 5 bytes 就是 frame_pack_header（TODO 1）。
#   ">I" = big-endian 的 32-bit 無號整數。C 要用位移自己拆：hdr[0] = length >> 24; … hdr[3] = length; hdr[4] = type;
#   不可以把 uint32_t 直接 memcpy 進去：PC 與 Mac 的 CPU 都是 little-endian，byte 順序會剛好相反。
#   整個函式對應 frame_send。C 版的 frame_send 另外用一把 lock 把「標頭＋payload」包起來，
#   因為聊天時兩條執行緒都會送 frame，不能互相插隊；這裡則是把整個 frame 接成一個 bytes、一次交給 sendall。
# 回傳這個 frame 在線上佔幾 bytes（5 + payload），給 STATS 的 wire_bytes 累加。
# --probe：每個 frame 印出 type、payload 長度，以及標頭＋payload 前 16 bytes 的 hex（.hex(" ") 用空白隔開每個 byte）。
def send_frame(sock, ftype, payload=b""):
    probe(f"送出 frame type=0x{ftype:02X} payload={len(payload):,} bytes", (struct.pack(">I", len(payload) + 1) + bytes([ftype]) + payload[:16]).hex(" "))
    sock.sendall(struct.pack(">I", len(payload) + 1) + bytes([ftype]) + payload)         # sendall：Python 幫你做了 send_all 的迴圈
    return 5 + len(payload)


def recv_exact(sock, n):
    """剛好收滿 n bytes。recv 一次可能只給 1 byte（半包），所以要迴圈；這就是 C 版的 recv_all。"""
    buf = bytearray()
    while len(buf) < n:
        part = sock.recv(n - len(buf))
        if not part:
            raise ConnectionError("對方已關閉連線")
        buf += part
    return bytes(buf)


# ★ 對應 C：struct.unpack(">IB", header) 加上下一行的範圍檢查，就是 frame_parse_header（TODO 2）。
#   ">IB" = big-endian 的 32-bit 無號整數 + 1 個 byte（"B" = 8-bit 無號），剛好 5 bytes；
#   C：length = hdr[0] << 24 | hdr[1] << 16 | hdr[2] << 8 | hdr[3]（每個 byte 先轉型成 uint32_t 再位移）。
#   整個函式對應 frame_recv：收滿 5 bytes → 檢查 length → 才照著它去收 payload。
# 檢查不能省：length 是對方說的。C 版接下來會 malloc(length)，不檢查的話對方送 FF FF FF FF 就能讓你配置 4 GB；
# length = 0 也不合法（連 type 都沒有），而且 length - 1 在 C 的無號整數會變成一個超大的數。
def recv_frame(sock):
    header = recv_exact(sock, 5)
    length, ftype = struct.unpack(">IB", header)
    if not 1 <= length <= MAX_FRAME:                                        # length 是對方說的：先檢查，再照著它去收
        raise ValueError(f"frame 的 length 不合法：{length}")
    payload = recv_exact(sock, length - 1)
    probe(f"收到 frame type=0x{ftype:02X} payload={len(payload):,} bytes", (header + payload[:16]).hex(" "))
    return ftype, payload


# =============================================================== 4. 連線
# 對應 C 的 net.c（殼裡已經寫好）。socket 的呼叫和 C 幾乎一對一：socket → bind → listen → accept；連線端是 connect。
#   socket.socket()   預設就是 IPv4 的 TCP（C：socket(AF_INET, SOCK_STREAM, 0)）
#   SO_REUSEADDR      程式剛結束、馬上用同一個 port 重開時，不會因為 "Address already in use" 而失敗
#   bind((ip, port))  "0.0.0.0" = 本機所有網路介面，別台電腦才連得進來；--bind 127.0.0.1 就只收本機的連線
#   listen(1)、accept()  accept 會停在那裡等，直到有人連進來；回傳「和那個人通話用的新 socket」與對方的 (ip, port)。
#                     只服務一個對象，所以 accept 之後就把監聽用的 server 關掉
#   TCP_NODELAY       關掉 Nagle 演算法：小封包不要等著湊成大包才送，聊天訊息才會馬上送出去
def listen_accept(bind_ip, port):
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((bind_ip or "0.0.0.0", port))
    server.listen(1)
    print(f"監聽中：{bind_ip or '0.0.0.0'}:{port}，等待對方連線...", flush=True)
    sock, (ip, peer_port) = server.accept()
    server.close()
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"對方已連線：{ip}:{peer_port}", flush=True)
    return sock, f"{ip}:{peer_port}"


# create_connection 把「建立 socket＋connect」包成一步；timeout=10 是規格要求的「連不上最多等 10 秒」。
# 連上之後 settimeout(None) 改回「一直等」：聊天時對方很久不說話是正常的，不可以因此逾時斷線。
# 連不上會丟 OSError，由 main 最後的 except 接住，印出錯誤訊息並以結束碼 1 結束。
def connect(ip, port):
    print(f"連線到 {ip}:{port} ...（最多等 10 秒）", flush=True)
    sock = socket.create_connection((ip, port), timeout=10)
    sock.settimeout(None)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"連線成功：對方 {ip}:{port}", flush=True)
    return sock, f"{ip}:{port}"


# =============================================================== 5. 傳檔：FILE_BEGIN → FILE_DATA × N → FILE_END →（對方回 FILE_END + 1 byte 狀態）
# 對應 C 的 transfer.c（殼裡已經寫好；裡面會呼叫你們的 huff_encode／huff_decode）。

# 檔名是對方給的，不能直接拿來開檔：對方若送 "../../某個重要的檔案"，就會寫到 outdir 外面去。
# 所以只取最後一段路徑（用 / 或 \ 切開取最後一個），再把 0-9 A-Z a-z . _ - 以外的字元都換成 _。
def safe_name(path):
    name = re.sub(r"[^0-9A-Za-z._-]", "_", re.split(r"[/\\]", path)[-1])
    return name if name not in ("", ".", "..") else "received.bin"


# 傳送端。整個檔案一次讀進記憶體、（huff 模式）一次編碼成一個 Huffman 區塊，再切成每段最多 CHUNK（64 KiB）的 FILE_DATA。
#   FILE_BEGIN 的 payload：mode 1 byte（0 = raw、1 = huff）｜原檔大小 8 bytes｜之後 FILE_DATA 的總 bytes 數 8 bytes｜檔名
#   struct.pack(">QQ", a, b)：兩個 big-endian 的 64-bit 無號整數，共 16 bytes
#   range(0, len(payload), CHUNK)：0、65536、131072……；payload[off:off + CHUNK] 切到結尾會自動變短，不必另外處理最後一段
# time.perf_counter() 是單調時鐘（不受系統改時間影響），相當於 C 的 clock_gettime(CLOCK_MONOTONIC, …)。
# wire 累加每個 frame 實際送上線的 bytes（含 5 bytes 標頭）。回傳的 dict 就是 STATS 那一行要印的欄位。
def send_file(sock, path, huff):
    t0 = time.perf_counter()
    with open(path, "rb") as f:
        data = f.read()
    if len(data) > MAX_FILE:
        raise ValueError("檔案太大（上限 64 MiB）")
    payload, kind, encode_ms = data, None, 0.0
    if huff:
        t = time.perf_counter()
        payload, kind = encode_auto(data, kind_for(path))
        encode_ms = (time.perf_counter() - t) * 1000
    wire = send_frame(sock, T_FILE_BEGIN, bytes([1 if huff else 0]) + struct.pack(">QQ", len(data), len(payload)) + safe_name(path).encode())
    t = time.perf_counter()
    for off in range(0, len(payload), CHUNK):
        wire += send_frame(sock, T_FILE_DATA, payload[off:off + CHUNK])
    wire += send_frame(sock, T_FILE_END)
    send_ms = (time.perf_counter() - t) * 1000
    return dict(name=safe_name(path), mode="huff" if huff else "raw", sym=SYM_NAME[kind] if huff else "none", file_bytes=len(data), wire_bytes=wire,
                ratio=wire / len(data) if data else 0.0, encode_ms=encode_ms, send_ms=send_ms, total_ms=(time.perf_counter() - t0) * 1000)


# 接收端。寫成 class 是因為狀態要跨好幾個 frame 保存（檔名、宣稱的大小、已經收到的資料），
# 而且 recv 指令與聊天兩個地方都要用。C 沒有 class：用一個 struct 加上幾個以它為參數的函式。
# 三個方法都是「先檢查對方說的、再照著做」：
#   begin   FILE_BEGIN 至少 18 bytes（1 + 8 + 8 + 檔名至少 1）；mode 只能是 0 或 1；兩個大小都不可超過 64 MiB；
#           raw 模式兩個大小必須相等。decode("utf-8", "replace")：檔名裡有非法的 UTF-8 就換成 "�"，不丟例外
#   data    累計不可超過 begin 時宣稱的 data_size，否則對方可以一直送、把記憶體塞爆
#   finish  收到的總數必須剛好等於 data_size；huff 模式解碼時把 orig_size 當成 huff_decode 的 max_out
# wire 一樣累加線上的 bytes；finish 裡多加的 5 是傳送端那個 FILE_END frame（payload 是空的，只有標頭）。
class FileReceiver:
    """把收到的 FILE_* frame 依序餵進來；收到傳送端的 FILE_END 時 finish() 會解碼、存檔。"""

    def __init__(self):
        self.active = False

    def begin(self, payload):
        if self.active or len(payload) < 18 or payload[0] > 1:
            raise ValueError("FILE_BEGIN 不合法")
        self.huff = payload[0] == 1
        self.orig_size, self.data_size = struct.unpack(">QQ", payload[1:17])
        if self.orig_size > MAX_FILE or self.data_size > MAX_FILE or (not self.huff and self.orig_size != self.data_size):
            raise ValueError("FILE_BEGIN 宣稱的大小不合理")                 # 大小是對方說的：先檢查，再配置
        self.name, self.buf, self.wire, self.t0, self.active = safe_name(payload[17:].decode("utf-8", "replace")), bytearray(), 5 + len(payload), time.perf_counter(), True

    def data(self, payload):
        if not self.active or len(self.buf) + len(payload) > self.data_size:
            raise ValueError("FILE_DATA 超過宣稱的大小，或前面沒有 FILE_BEGIN")
        self.buf += payload
        self.wire += 5 + len(payload)

    def finish(self, outdir):
        if not self.active or len(self.buf) != self.data_size:
            raise ValueError("FILE_END：收到的大小與宣稱的不符")
        self.active, decode_ms, data = False, 0.0, bytes(self.buf)
        if self.huff:
            t = time.perf_counter()
            data = huff_decode(data, self.orig_size)
            decode_ms = (time.perf_counter() - t) * 1000
        os.makedirs(outdir, exist_ok=True)
        path = os.path.join(outdir, self.name)
        with open(path + ".part", "wb") as f:                               # 先寫 .part，成功才改名：失敗不會留下壞掉的檔案
            f.write(data)
        os.replace(path + ".part", path)
        return dict(path=path.replace("\\", "/"), mode="huff" if self.huff else "raw", file_bytes=self.orig_size, wire_bytes=self.wire + 5,
                    ratio=(self.wire + 5) / self.orig_size if self.orig_size else 0.0, decode_ms=decode_ms, total_ms=(time.perf_counter() - self.t0) * 1000)


# send 指令：連線 → 送檔 → 等對方回一個 FILE_END（payload 1 byte：0 = 已成功還原並存檔、1 = 失敗）。
# 收到 0 才算成功（結束碼 0）。STATS 那一行印到 stderr，欄位名稱是規格固定的，評測腳本會直接讀它。
def cmd_send(args):
    sock, _ = connect(args.ip, args.port)
    st = send_file(sock, args.file, args.huff)
    ftype, payload = recv_frame(sock)
    ok = ftype == T_FILE_END and payload == b"\x00"
    print(f"STATS role=send mode={st['mode']} sym={st['sym']} file_bytes={st['file_bytes']} wire_bytes={st['wire_bytes']} ratio={st['ratio']:.4f} "
          f"encode_ms={st['encode_ms']:.1f} send_ms={st['send_ms']:.1f} total_ms={st['total_ms']:.1f}", file=sys.stderr)
    print(f"壓縮率 {100 * st['ratio']:.2f}%（上線 {st['wire_bytes']:,} bytes ÷ 原檔 {st['file_bytes']:,} bytes）" if ok else "錯誤: 接收端回報失敗")
    return 0 if ok else 1


# recv 指令：監聽 → 一個一個收 frame 交給 FileReceiver → 收到 FILE_END 就解碼、存檔、回報結果。
# 任何一步失敗（格式不對、解碼失敗、斷線、寫檔失敗）都走同一個 except：不產生輸出檔，盡量回報對方失敗（1），結束碼 1。
# 回報時對方可能已經斷線，所以那個 send_frame 自己也包在 try 裡。
def cmd_recv(args):
    sock, _ = listen_accept(args.bind, args.port)
    rx = FileReceiver()
    try:
        while True:
            ftype, payload = recv_frame(sock)
            if ftype == T_FILE_BEGIN:
                rx.begin(payload)
            elif ftype == T_FILE_DATA:
                rx.data(payload)
            elif ftype == T_FILE_END:
                break
            else:
                raise ValueError("不認得的 frame type")
        st = rx.finish(args.outdir)
    except (ValueError, ConnectionError, OSError) as why:
        print(f"接收失敗，沒有產生輸出檔：{why}", file=sys.stderr)
        try:
            send_frame(sock, T_FILE_END, b"\x01")
        except OSError:
            pass
        return 1
    send_frame(sock, T_FILE_END, b"\x00")
    print(f"已存檔：{st['path']}（{st['file_bytes']:,} bytes）")
    print(f"STATS role=recv mode={st['mode']} file_bytes={st['file_bytes']} wire_bytes={st['wire_bytes']} ratio={st['ratio']:.4f} "
          f"decode_ms={st['decode_ms']:.1f} total_ms={st['total_ms']:.1f}", file=sys.stderr)
    return 0


# =============================================================== 6. 聊天：主執行緒讀鍵盤、另一條執行緒收訊息
# 為什麼要兩條執行緒：input() 會停在那裡等你打字，sock.recv() 會停在那裡等對方送資料；只有一條執行緒的話，
# 等鍵盤時收不到訊息、等訊息時不能打字。所以各用一條，兩個等待同時進行。對應 C 的 chat.c（殼裡已經寫好）。
def clean(text):
    # 控制字元（Unicode 類別 Cc，含 ESC）不可以原樣印到終端機。不能用 isprintable()：它會把組合 emoji 裡的零寬連接字 U+200D 也濾掉
    return "".join(" " if unicodedata.category(ch) == "Cc" else ch for ch in text)


# 接收執行緒：一直 recv_frame，依 type 分派。
#   文字   TEXT_HUFF 先 huff_decode（max_out = 4095：聊天訊息的上限，對方宣稱更大就直接拒絕），再 raw.decode("utf-8")。
#          ★ 對應 C：這個嚴格的 decode 就是 utf8_validate（TODO 3）的位置——網路上收到的文字一定要先檢查才顯示。
#          不合法的訊息只丟棄那一則並顯示系統訊息（裡層的 try），連線繼續，程式不可以因此當掉。
#          "[HUFF 3 B -> 31 B on wire]" 這種標記：原始 bytes → 實際上線的 bytes（payload + 5 bytes 標頭）。
#          這是「嗨」一個字的數字：3 bytes 變成 21（檔頭）+ 4（codebook）+ 1（bitstream）+ 5（frame 標頭）= 31
#   檔案   交給 FileReceiver。FILE_END 有兩種意思，靠 payload 長度與「我是不是正在收檔」分辨：
#          payload 1 byte 而且我沒有在收檔 → 對方對「我送的檔案」的回覆；否則 → 對方送完了，我該解碼存檔並回覆
#   其他   不認得的 type 或 frame 層的錯誤 → 外層的 except：結束連線
# "\r" 把游標拉回行首，蓋掉畫面上的 "訊息> " 提示，印完再重印一次提示。
# state 是主執行緒傳進來的同一個 dict（不是複本），兩條執行緒靠 state["running"] 互相通知「該結束了」。
def chat_receiver(sock, state):
    rx = FileReceiver()
    try:
        while True:
            ftype, payload = recv_frame(sock)
            if ftype in (T_TEXT_RAW, T_TEXT_HUFF):
                try:
                    raw = payload if ftype == T_TEXT_RAW else huff_decode(payload, MAX_TEXT)
                    if len(raw) > MAX_TEXT or b"\x00" in raw:
                        raise ValueError("太長或含有 NUL")
                    print(f"\r    對方> {clean(raw.decode('utf-8'))}    [{'HUFF' if ftype == T_TEXT_HUFF else 'RAW'} {len(raw)} B -> {len(payload) + 5} B on wire]")
                except ValueError as why:                                   # UnicodeDecodeError 也是 ValueError
                    print(f"\r    [系統] 收到不合法的訊息，已丟棄：{why}")
            elif ftype == T_FILE_BEGIN:
                rx.begin(payload)
                print(f"\r    [檔案] 對方開始傳 {rx.name}（原始 {rx.orig_size:,} B，{'HUFF' if rx.huff else 'RAW'}）")
            elif ftype == T_FILE_DATA:
                rx.data(payload)
            elif ftype == T_FILE_END and len(payload) == 1 and not rx.active:
                print(f"\r    [檔案] 對方{'已成功還原並存檔' if payload == bytes(1) else '回報還原失敗'}")
            elif ftype == T_FILE_END:
                try:
                    st = rx.finish("received")
                    print(f"\r    [檔案] 已存檔 {st['path']}：原始 {st['file_bytes']:,} B，上線 {st['wire_bytes']:,} B，壓縮率 {100 * st['ratio']:.2f}%，decode {st['decode_ms']:.1f} ms")
                    send_frame(sock, T_FILE_END, b"\x00")
                except ValueError as why:
                    rx.active = False
                    print(f"\r    [檔案] 無法還原：{why}")
                    send_frame(sock, T_FILE_END, b"\x01")
            else:
                raise ValueError("不認得的 frame type")
            print("訊息> ", end="", flush=True)
    except (ConnectionError, OSError, ValueError) as why:
        if state["running"]:
            print(f"\r    [系統] 連線結束：{why}（按 Enter 離開）")
        state["running"] = False


# 主執行緒：建立連線 → 啟動接收執行緒 → 迴圈讀鍵盤。
#   threading.Thread(target=函式, args=(參數…), daemon=True).start()
#       另開一條執行緒去跑 chat_receiver(sock, state)。daemon=True：主執行緒結束時，這條執行緒跟著結束，不會卡住程式
#       （C：pthread_create，或 Windows 的執行緒 API）
#   /files  列出資料夾裡的 .txt 與 .wav，編號記在 picks，之後 /send 3 就是送第 3 個；enumerate(picks, 1) 從 1 開始編號
#   /send   line[6:] 是 "/send " 之後的字；是 picks 範圍內的數字就當編號，否則當路徑
#   其他    當成訊息：encode 成 UTF-8 bytes，超過 4095 bytes 不送；huff 模式就以 UTF-8 字元為符號編碼成一個區塊。
#           一則短訊息也帶完整的 codebook，所以幾乎一定比原文大——看 "[HUFF … B -> … B on wire]" 的數字就知道
def cmd_chat(args):
    sock, peer = listen_accept(args.bind, args.port) if args.role == "server" else connect(args.ip, args.port)
    state, picks = {"running": True, "huff": args.huff}, []
    threading.Thread(target=chat_receiver, args=(sock, state), daemon=True).start()
    print(f"TextLink（Python 版）｜對方 {peer}｜指令：/files [資料夾]  /send <編號或路徑>  /raw  /huff  /quit")
    while state["running"]:
        try:
            line = input("訊息> ").strip()
        except EOFError:
            break
        if not state["running"] or line == "/quit":
            break
        if line in ("/raw", "/huff"):
            state["huff"] = line == "/huff"
            print(f"    [系統] 之後送出的訊息與檔案：{'先經 Huffman 編碼' if state['huff'] else '不壓縮'}")
        elif line.split(" ")[0] == "/files":
            folder = line[7:].strip() or "."
            picks = sorted(os.path.join(folder, f).replace("\\", "/") for f in os.listdir(folder) if f.lower().endswith((".txt", ".wav"))) if os.path.isdir(folder) else []
            print("\n".join(f"    [檔案] {i:2d}) {p}  {os.path.getsize(p) / 1048576:.2f} MB" for i, p in enumerate(picks, 1)) or f"    [檔案] {folder} 裡沒有 .txt 或 .wav")
        elif line.split(" ")[0] == "/send":
            target = line[6:].strip().strip('"')
            path = picks[int(target) - 1] if target.isdigit() and 1 <= int(target) <= len(picks) else target
            try:
                st = send_file(sock, path, state["huff"])
                print(f"    [檔案] 已送出 {st['name']}（{st['mode']}，符號={st['sym']}）：原始 {st['file_bytes']:,} B，上線 {st['wire_bytes']:,} B，"
                      f"壓縮率 {100 * st['ratio']:.2f}%，encode {st['encode_ms']:.1f} ms")
            except (OSError, ValueError) as why:
                print(f"    [檔案] 沒有送出：{why}")
        elif line:
            raw = line.encode("utf-8")
            if len(raw) > MAX_TEXT:
                print("    [系統] 訊息太長")
                continue
            payload = huff_encode(raw, SYM_CHAR) if state["huff"] else raw          # 文字：符號＝UTF-8 字元
            wire = send_frame(sock, T_TEXT_HUFF if state["huff"] else T_TEXT_RAW, payload)
            print(f"      我> {line}    [{'HUFF' if state['huff'] else 'RAW'} {len(raw)} B -> {wire} B on wire]")
    state["running"] = False
    sock.close()
    print("再見！")
    return 0


# =============================================================== 7. inspect：不連線，直接分析一個檔案（報告裡的「壓縮率分析表」就是這些數字）
# 對同一個檔案列兩列：依副檔名選的符號，以及「改以 byte 為符號」的對照。
# dict.fromkeys([chosen, SYM_BYTE]) 是「去掉重複、保留順序」的小技巧：chosen 本來就是 byte 時只會跑一次。
# 每一欄：
#   N、K          符號個數、符號種類數
#   H             熵（bits／符號）
#   L             平均碼長 = Σ 次數 × 長度 ÷ N；應該滿足 H ≤ L < H + 1（只有一種符號時例外：H = 0、L = 1）
#   H×N÷8÷原始    理論壓縮率，不含 codebook
#   codebook      K × (符號寬度 + 1) bytes
#   區塊、壓縮率   huff_encode 輸出的 bytes 數，與它 ÷ 原始 bytes 數（含檔頭與 codebook，不含 frame 標頭）
#   還原          真的 huff_decode 回來，和原檔逐 byte 比較
# f-string 的 {n:>10,d}：靠右對齊、寬 10 格、加千分位逗號；{h:>8.4f}：寬 8 格、小數 4 位。
# 加上 --probe 時，這裡呼叫的 huff_encode／huff_decode 也會印出它們的 [probe] 內容（在 stderr）。
def cmd_inspect(args):
    with open(args.file, "rb") as f:
        data = f.read()
    chosen = kind_for(args.file)
    print(f"{args.file}：{len(data):,} bytes；依副檔名，符號＝{SYM_NAME[chosen]}")
    print(f"{'符號':6s} {'N':>10s} {'K':>7s} {'H':>8s} {'L':>8s} {'H×N÷8÷原始':>12s} {'codebook':>10s} {'區塊':>11s} {'壓縮率':>8s}  還原")
    for kind in dict.fromkeys([chosen, SYM_BYTE]):
        try:
            symbols, _, _ = split_symbols(data, kind)
        except ValueError as why:
            print(f"{SYM_NAME[kind]:6s} 不適用：{why}")
            continue
        freq = collections.Counter(symbols)
        length = code_lengths(freq) if freq else {}
        n, h = len(symbols), entropy(freq)
        avg = sum(freq[s] * length[s] for s in freq) / n if n else 0.0
        block = huff_encode(data, kind)
        same = huff_decode(block, len(data)) == data
        print(f"{SYM_NAME[kind]:6s} {n:>10,d} {len(freq):>7,d} {h:>8.4f} {avg:>8.4f} {100 * h * n / 8 / max(1, len(data)):>11.2f}% {len(freq) * (SYM_WIDTH[kind] + 1):>10,d} "
              f"{len(block):>11,d} {100 * len(block) / max(1, len(data)):>7.2f}%  {'逐 byte 相同' if same else '不同！'}")
    return 0


# 命令列。argparse 的 subcommand 就像 git commit、git push：第一個字決定後面接受哪些參數。
#   sub = ap.add_subparsers(dest="cmd")    子指令的名字存進 args.cmd（chat／recv／send／inspect）
#   sub.add_parser("chat").add_subparsers(dest="role")    chat 底下再分 server／client，存進 args.role
#   p.add_argument("port", type=int)       沒有 -- 的是「依位置」的必填參數，type=int 會自動轉成整數、不是數字就報錯
#   p.add_argument("--bind")               有 -- 的是選填，沒給就是 None
#   parents=[mode]                         --raw／--huff／--probe 每個子指令都要，寫在 mode 一次，再讓各子指令繼承
#   --raw 與 --huff 寫進同一個變數 args.huff（dest="huff"）：store_false／store_true，預設 True，也就是預設 --huff
#   --probe 在最外層與 mode 各定義一次，所以寫在子指令前面或後面都可以。mode 裡的 default=argparse.SUPPRESS 表示
#       「沒寫就不要設定這個屬性」，否則子指令的預設值 False 會蓋掉寫在前面的 --probe
# C 沒有 argparse：自己比對 argv[1]、argv[2]……，數字用 strtol 轉換並檢查範圍。
#   global PROBE：要在函式裡「改」全域變數必須先宣告，否則 Python 會當成新的區域變數
#   reconfigure(encoding="utf-8")：Windows 的終端機預設可能不是 UTF-8，印中文或 emoji 會出錯，所以強制用 UTF-8
#   getattr(args, "port", 1)：inspect 沒有 port 這個參數，給預設值 1 讓檢查通過
#   {"chat": cmd_chat, …}[args.cmd](args)：用 dict 查出對應的函式再呼叫，代替一長串 if／elif
#   回傳值交給最後一行的 sys.exit 當結束碼：0 = 成功、非 0 = 失敗（規格要求，評測腳本會檢查）
def main():
    global PROBE
    ap = argparse.ArgumentParser(description="TextLink 純 Python 完整版（對照 C 版用）")
    ap.add_argument("--probe", action="store_true", help="印出中間過程")
    sub = ap.add_subparsers(dest="cmd", required=True)
    mode = argparse.ArgumentParser(add_help=False)
    mode.add_argument("--raw", dest="huff", action="store_false")
    mode.add_argument("--huff", dest="huff", action="store_true", default=True)
    mode.add_argument("--probe", action="store_true", default=argparse.SUPPRESS)
    chat = sub.add_parser("chat").add_subparsers(dest="role", required=True)
    p = chat.add_parser("server", parents=[mode]); p.add_argument("port", type=int); p.add_argument("--bind")
    p = chat.add_parser("client", parents=[mode]); p.add_argument("ip"); p.add_argument("port", type=int); p.set_defaults(bind=None)
    p = sub.add_parser("recv", parents=[mode]); p.add_argument("port", type=int); p.add_argument("outdir"); p.add_argument("--bind")
    p = sub.add_parser("send", parents=[mode]); p.add_argument("ip"); p.add_argument("port", type=int); p.add_argument("file")
    p = sub.add_parser("inspect", parents=[mode]); p.add_argument("file")
    args = ap.parse_args()
    PROBE = args.probe
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    if getattr(args, "port", 1) not in range(1, 65536):
        ap.error("port 要是 1–65535 的整數")
    try:
        return {"chat": cmd_chat, "send": cmd_send, "recv": cmd_recv, "inspect": cmd_inspect}[args.cmd](args)
    except (OSError, ValueError) as why:
        print(f"錯誤: {why}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
