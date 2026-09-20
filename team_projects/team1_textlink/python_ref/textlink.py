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

PROBE = False
T_TEXT_RAW, T_TEXT_HUFF, T_FILE_BEGIN, T_FILE_DATA, T_FILE_END = 0x01, 0x02, 0x10, 0x11, 0x12
MAX_FRAME, CHUNK, MAX_FILE, MAX_TEXT = 16 * 1024 * 1024, 64 * 1024, 64 * 1024 * 1024, 4095
SYM_BYTE, SYM_CHAR, SYM_S16 = 0, 1, 2
SYM_NAME = {SYM_BYTE: "byte", SYM_CHAR: "char", SYM_S16: "s16"}
SYM_WIDTH = {SYM_BYTE: 1, SYM_CHAR: 3, SYM_S16: 2}          # codebook 裡一個符號佔幾 bytes
MAX_CODE_LEN = 56


def probe(title, *lines):
    if PROBE:
        print(f"[probe] {title}", *[f"          {x}" for x in lines], sep="\n", file=sys.stderr)


def show_symbol(sym, kind):
    if kind == SYM_CHAR:
        ch = chr(sym)
        return f"U+{sym:04X} {ch!r}" if ch.isprintable() else f"U+{sym:04X}"
    if kind == SYM_S16:
        return f"{sym - 65536 if sym >= 32768 else sym:+d}"
    return f"0x{sym:02X}"


# =============================================================== 1. 切符號：對「什麼東西」統計機率
def wav_data_range(data):
    """回傳 16-bit PCM WAV 的 data 區 [start, end)。逐個 chunk 走：data 不一定在第 44 byte。"""
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("不是 RIFF/WAVE")
    pos, fmt_ok = 12, False
    while pos + 8 <= len(data):
        chunk_id, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        if chunk_id == b"fmt ":
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


def split_symbols(data, kind):
    """bytes → (符號的 list, 前面原樣保留的 bytes, 後面原樣保留的 bytes)。資料不適用就丟 ValueError。"""
    if kind == SYM_CHAR:
        return [ord(ch) for ch in data.decode("utf-8")], b"", b""          # 嚴格解碼：非法 UTF-8 會丟 UnicodeDecodeError（ValueError 的一種）
    if kind == SYM_S16:
        start, end = wav_data_range(data)
        return list(struct.unpack(f"<{(end - start) // 2}H", data[start:end])), data[:start], data[end:]
    return list(data), b"", b""


def join_symbols(symbols, kind, head, tail):
    if kind == SYM_CHAR:
        return "".join(map(chr, symbols)).encode("utf-8")
    if kind == SYM_S16:
        return head + struct.pack(f"<{len(symbols)}H", *symbols) + tail
    return bytes(symbols)


# =============================================================== 2. Huffman
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


def canonical_codes(length):
    """{符號: 長度} → {符號: "0101" 字串}。依（長度, 符號值）排序，第一個是全 0，之後每次 +1，長度變長就在右邊補 0。"""
    codes, value, prev = {}, 0, 0
    for sym in sorted(length, key=lambda s: (length[s], s)):
        value <<= length[sym] - prev
        prev = length[sym]
        codes[sym] = format(value, f"0{prev}b")
        value += 1
    return codes


def entropy(freq):
    n = sum(freq.values())
    return sum(c / n * math.log2(n / c) for c in freq.values()) if n else 0.0


def huff_encode(data, kind):
    """bytes → 自己帶 codebook 的區塊。格式（多 byte 欄位都是 big-endian）：
         kind 1｜原始長度 8｜符號個數 8｜符號種類數 K 4｜K 組（符號 W bytes、code 長度 1 byte），依符號值遞增｜
         （只有 s16）head 長度 4、head、tail 長度 4、tail｜bitstream（高位先出，最後補 0）"""
    symbols, head, tail = split_symbols(data, kind)
    freq = collections.Counter(symbols)
    length = code_lengths(freq) if freq else {}
    codes = canonical_codes(length)
    bits = "".join(codes[s] for s in symbols)
    padded = bits + "0" * (-len(bits) % 8)
    stream = int(padded, 2).to_bytes(len(padded) // 8, "big") if padded else b""

    width = SYM_WIDTH[kind]
    book = b"".join(sym.to_bytes(width, "big") + bytes([length[sym]]) for sym in sorted(length))
    extra = struct.pack(">I", len(head)) + head + struct.pack(">I", len(tail)) + tail if kind == SYM_S16 else b""
    block = bytes([kind]) + struct.pack(">QQI", len(data), len(symbols), len(length)) + book + extra + stream

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
    if len(block) < 21 or block[0] > 2:
        raise ValueError("區塊太短或符號種類不對")
    kind = block[0]
    orig_len, n_syms, k = struct.unpack(">QQI", block[1:21])
    width = SYM_WIDTH[kind]
    if orig_len > max_out or n_syms > orig_len or k * (width + 1) > len(block) - 21 or (n_syms > 0) != (k > 0):
        raise ValueError("檔頭的數字不合理")

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
    data = join_symbols(symbols, kind, head, tail)
    if len(data) != orig_len:
        raise ValueError("解出來的長度與宣稱的不符")
    probe(f"huff_decode：符號＝{SYM_NAME[kind]}，{n_syms:,} 個符號、{k:,} 種 → {len(data):,} bytes")
    return data


def kind_for(path):
    return SYM_S16 if path.lower().endswith(".wav") else SYM_CHAR if path.lower().endswith(".txt") else SYM_BYTE


def encode_auto(data, kind):
    """照副檔名選的符號不適用（.txt 不是合法 UTF-8、.wav 不是 16-bit PCM）時，退回以 byte 為符號。"""
    try:
        return huff_encode(data, kind), kind
    except ValueError as why:
        probe(f"以 {SYM_NAME[kind]} 為符號不適用（{why}），改用 byte")
        return huff_encode(data, SYM_BYTE), SYM_BYTE


# =============================================================== 3. frame：length 4 bytes big-endian｜type 1 byte｜payload
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


def recv_frame(sock):
    header = recv_exact(sock, 5)
    length, ftype = struct.unpack(">IB", header)
    if not 1 <= length <= MAX_FRAME:                                        # length 是對方說的：先檢查，再照著它去收
        raise ValueError(f"frame 的 length 不合法：{length}")
    payload = recv_exact(sock, length - 1)
    probe(f"收到 frame type=0x{ftype:02X} payload={len(payload):,} bytes", (header + payload[:16]).hex(" "))
    return ftype, payload


# =============================================================== 4. 連線
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


def connect(ip, port):
    print(f"連線到 {ip}:{port} ...（最多等 10 秒）", flush=True)
    sock = socket.create_connection((ip, port), timeout=10)
    sock.settimeout(None)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"連線成功：對方 {ip}:{port}", flush=True)
    return sock, f"{ip}:{port}"


# =============================================================== 5. 傳檔：FILE_BEGIN → FILE_DATA × N → FILE_END →（對方回 FILE_END + 1 byte 狀態）
def safe_name(path):
    name = re.sub(r"[^0-9A-Za-z._-]", "_", re.split(r"[/\\]", path)[-1])
    return name if name not in ("", ".", "..") else "received.bin"


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


def cmd_send(args):
    sock, _ = connect(args.ip, args.port)
    st = send_file(sock, args.file, args.huff)
    ftype, payload = recv_frame(sock)
    ok = ftype == T_FILE_END and payload == b"\x00"
    print(f"STATS role=send mode={st['mode']} sym={st['sym']} file_bytes={st['file_bytes']} wire_bytes={st['wire_bytes']} ratio={st['ratio']:.4f} "
          f"encode_ms={st['encode_ms']:.1f} send_ms={st['send_ms']:.1f} total_ms={st['total_ms']:.1f}", file=sys.stderr)
    print(f"壓縮率 {100 * st['ratio']:.2f}%（上線 {st['wire_bytes']:,} bytes ÷ 原檔 {st['file_bytes']:,} bytes）" if ok else "錯誤: 接收端回報失敗")
    return 0 if ok else 1


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
def clean(text):
    # 控制字元（Unicode 類別 Cc，含 ESC）不可以原樣印到終端機。不能用 isprintable()：它會把組合 emoji 裡的零寬連接字 U+200D 也濾掉
    return "".join(" " if unicodedata.category(ch) == "Cc" else ch for ch in text)


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
