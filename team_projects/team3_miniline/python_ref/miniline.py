#!/usr/bin/env python3
"""MiniLINE 的純 Python 完整版（mock 模式）：功能與 C 版的目標相同，用來「對照著看」，也是評測的對手端。只用標準函式庫。

  python3 miniline.py video-send <ip> <port> <jpegdir> [--fps 5] [--quality 75] [--burst] [--loop]
      讀資料夾裡的 0000.jpg、0001.jpg…，每張切成 ≤ 1200 bytes 的片，照 fps 送（預設每 200 ms 一張）；--burst 一口氣全送；--loop 送完從頭再送
  python3 miniline.py video-recv <port> <outdir> [--bind <ip>] [--queue <n>] [--timeout <ms>] [--idle <ms>] [--display none|window]
      收片 → 重組表 → 逾時丟棄 → bounded display queue → 每 200 ms「顯示」一張（寫成 outdir/NNNN.jpg，記進 outdir/shown.txt）→ 結束印 STATS
  任何指令都可以加 --probe：把每一片、每張收齊／丟掉、每一拍 queue 裡有誰印出來（stderr）
  call 與 devices（live 模式）、<jpegdir> 給 camera、--display window 都不在 Python 版裡，要做的是 C。

這支程式刻意用「高階」的寫法，讓你一眼看到每一步在做什麼：
  封包    struct.pack(">BBHIHHI", …) 一行打包 16 bytes 的 header；解析用 struct.unpack
  重組表  dict：{frame_id: Frame}，Frame 裡又是 dict {piece_idx: bytes}；「收齊了沒」就是 len(pieces) == piece_cnt
  queue   list，最多 --queue 張；滿了 pop(0) 丟最舊的
  時鐘    time.perf_counter() 的絕對時刻：第 i 拍 = 開播時刻 + i × 0.2，睡到那一刻再動作（不是 sleep(0.2)，誤差不會累積）
  等待    select.select([sock], [], [], 到下一件事還有多久)：有封包就收、到拍子就顯示、到逾時就丟，一條執行緒就夠（mock）
C 沒有這些現成的東西：dict 要換成固定大小的陣列（重組表 8 格、queue n 格的環狀陣列）、select 要換成 recvfrom 加逾時（wrapper 的 udp.h）、
perf_counter 換成 clock.h 的單調時鐘；live 模式還要把收視訊、收語音、顯示拆成各自的執行緒。把這裡的每一步「翻譯」成 C，就是 Team 3 的工作。
"""
# ---------------------------------------------------------------------------------------------------------------
# 怎麼讀這支程式：由下層往上層分成五個區塊。
#   1. JPEG 序列   讀資料夾裡的 NNNN.jpg（只是 bytes，不解碼）；寫出就是把 bytes 原樣存檔
#   2. 封包        16 bytes header｜≤ 1200 bytes JPEG 片段；pack／parse；seq 的 16-bit 繞回 → 延伸序號與遺失計數
#   3. video-send  3 次 HELLO → 每 200 ms 一張（切片逐片送）→ 3 次 BYE，印 STATS role=video-send
#   4. video-recv  重組表（Reassembler）、顯示 queue 與顯示時鐘（Display）、主迴圈（cmd_video_recv）
#   5. main        命令列
#
# 和規格的對應（★ 對應 C 的地方就是你們要寫的那一段）：
#   封包格式            pack_header／parse_packet
#   切片                cmd_video_send 裡的 range(0, len(jpeg), PIECE_MAX)
#   重組表與逾時        Reassembler.put（放片、收齊）、Reassembler.expire（--timeout 與 frame_id 落後 2 的規則）、表的上限 8 張
#   bounded queue       Display.push（滿了丟最舊）
#   顯示時鐘            Display.tick（每 200 ms 取最舊的一張寫檔；空的計 display_idle）、cmd_video_recv 的主迴圈
#   HELLO／ssrc／BYE    cmd_video_recv 主迴圈裡的三個 if
#   STATS               cmd_video_send／cmd_video_recv 最後的 print(…, file=sys.stderr)
#
# 建議的讀法：先開兩個視窗跑 video-recv --probe 與 video-send，看每一片、每一拍印出來的東西；再夾上 udp_impair.py --loss 0.03 看哪張被 timeout 丟掉。
# ---------------------------------------------------------------------------------------------------------------
import argparse
import os
import random
import re
import select
import socket
import struct
import sys
import time

# 常數（數值是規格訂的；C 版請放在 header 檔）。
#   MAGIC／T_PIECE／T_BYE／T_HELLO   header 的第 0、1 byte
#   HDR_LEN                          header 16 bytes；PIECE 封包 ≤ 1216 bytes；HELLO／BYE 封包只有 header
#   PIECE_MAX                        一片最多 1200 bytes 的 JPEG 資料
#   FRAME_SEC                        顯示時鐘一拍 = 200 ms（5 fps）
#   HELLO_REPEAT／BYE_REPEAT         開始送 3 次 HELLO、結束送 3 次 BYE（UDP 可能掉）
#   TABLE_CAP                        重組表最多同時放 8 張
#   BYE_GRACE_SEC                    mock 收到 BYE、queue 顯示完之後，再多等這麼久看有沒有新的 HELLO（掛斷再連，驗收第 7 項）；沒有就結束
PROBE = False
MAGIC, T_PIECE, T_BYE, T_HELLO = 0x4D, 0x01, 0x02, 0x03
HDR_LEN, PIECE_MAX = 16, 1200
FRAME_SEC = 0.2
HELLO_REPEAT = BYE_REPEAT = 3
TABLE_CAP = 8
BYE_GRACE_SEC = 1.0
JPEG_NAME = re.compile(r"^(\d{4})\.jpg$", re.IGNORECASE)


# --probe 的輸出都經過這個函式，印到 stderr，不和正常輸出混在一起。
def probe(msg):
    if PROBE:
        print(f"[probe] {msg}", file=sys.stderr, flush=True)


# =============================================================== 1. JPEG 序列
# mock 的輸入是資料夾裡的 0000.jpg、0001.jpg…（四位數，照數字排序）。JPEG 只是 bytes：送端切片、收端接回來，不解碼、不重新壓縮。
# 找不到資料夾或一張都沒有就丟 ValueError；main 接住後印錯誤、結束碼 2。
# ★ 對應 C：opendir／_findfirst 列出檔案，或直接從 0000 往上開到開不到為止；整張讀進一塊 malloc 的 buffer。
def read_sequence(jpegdir):
    """回傳 [(檔名, bytes), …]，照 NNNN 排序。"""
    if not os.path.isdir(jpegdir):
        raise ValueError(f"找不到資料夾 {jpegdir}")
    names = sorted((n for n in os.listdir(jpegdir) if JPEG_NAME.match(n)), key=lambda n: int(n[:4]))
    if not names:
        raise ValueError(f"{jpegdir} 裡沒有 NNNN.jpg（四位數，例如 0000.jpg）")
    frames = []
    for n in names:
        with open(os.path.join(jpegdir, n), "rb") as f:
            data = f.read()
        if len(data) < 4 or data[:2] != b"\xff\xd8":
            raise ValueError(f"{n} 不是 JPEG（開頭不是 FF D8）")
        frames.append((n, data))
    return frames


# =============================================================== 2. 封包：16 bytes header｜≤ 1200 bytes JPEG 片段
# header 全部 big-endian（網路位元組序）。
#   ">BBHIHHI"：B = 1 byte、H = 16-bit、I = 32-bit，共 1 + 1 + 2 + 4 + 2 + 2 + 4 = 16 bytes：
#   magic、type、seq、frame_id、piece_idx、piece_cnt、ssrc
#   seq & 0xFFFF：所有 type 共用，送到 65535 之後繞回 0（規格）。
# ★ 對應 C：不可以把 struct 直接 memcpy 到封包（對齊與 byte 順序都會錯）；一個 byte 一個 byte 用位移填：p[4] = frame_id >> 24; …
def pack_header(ptype, seq, frame_id, piece_idx, piece_cnt, ssrc):
    return struct.pack(">BBHIHHI", MAGIC, ptype, seq & 0xFFFF, frame_id & 0xFFFFFFFF, piece_idx, piece_cnt, ssrc)


# 解析收到的 datagram。不合法就回傳 None——呼叫端計 pieces_bad 後丟掉，程式不可以當掉（規格：壞封包測試）。
# 不合法的定義（規格）：長度 < 16、magic 不是 0x4D、type 不是 1／2／3、PIECE 的 piece_idx ≥ piece_cnt、片段 > 1200 bytes。
# 這支程式的約定：PIECE 的片段不可以是空的（0 bytes 的片沒有意義）；HELLO／BYE 的 payload 應該是空的，多了就忽略（不算壞）。
# ★ 對應 C：這就是「先檢查長度、再讀欄位」：長度不到 16 就去讀 header 是讀到陣列外面。
def parse_packet(data):
    """bytes → (type, seq, frame_id, piece_idx, piece_cnt, ssrc, payload)；不合法回傳 None。"""
    if len(data) < HDR_LEN or data[0] != MAGIC:
        return None
    _, ptype, seq, frame_id, piece_idx, piece_cnt, ssrc = struct.unpack(">BBHIHHI", data[:HDR_LEN])
    payload = data[HDR_LEN:]
    if ptype == T_PIECE:
        if piece_idx >= piece_cnt or not 0 < len(payload) <= PIECE_MAX:
            return None
        return ptype, seq, frame_id, piece_idx, piece_cnt, ssrc, payload
    if ptype in (T_BYE, T_HELLO):
        return ptype, seq, frame_id, piece_idx, piece_cnt, ssrc, b""
    return None


# 16-bit 的 seq 會繞回：65535 的下一個是 0。比大小、算遺失都不能直接用它，要先換成「延伸序號」（RTP 也是這樣做）。
# 做法：和目前看過最大的延伸序號比，差距取 16-bit 的有號差（-32768 … +32767）再加回去。
#   例：highest = 65535，收到 seq = 0 → (0 - 65535) & 0xFFFF = 1 → 65536（不是 0）
# ★ 對應 C：同樣的兩行，記得用 uint16_t 相減再轉成 int16_t。
def extend_seq(seq, highest):
    if highest is None:
        return seq
    diff = (seq - highest) & 0xFFFF
    if diff >= 0x8000:
        diff -= 0x10000
    return highest + diff


# pieces_lost 的算法：所有 type 的封包 seq 共用、每包 +1，所以「跳號」就是遺失。
#   seq 往前跳 k 個 → 先把中間的 k 個記成 missing、lost += k；之後它們亂序到了再從 missing 拿掉、lost −= 1。
#   已經看過的 seq 再來 → 重複（udp_impair --dup），不算遺失、也不算在 lost 裡。
#   missing 只留最近 4096 個 seq，表不會無限長（--loop 可以跑很久）。
# ★ 對應 C：一個 uint16_t highest、一個 int lost，加一個小的環狀 bitmap 記「最近哪些 seq 沒收到」就夠了。
class SeqTracker:
    def __init__(self):
        self.highest, self.missing, self.lost = None, set(), 0

    def note(self, seq):
        """回傳 (延伸序號, 是不是重複)。"""
        ext = extend_seq(seq, self.highest)
        if self.highest is None or ext > self.highest:
            if self.highest is not None:
                gap = range(self.highest + 1, ext)
                self.missing.update(gap)
                self.lost += len(gap)
                if len(gap):
                    probe(f"seq 從 {self.highest} 跳到 {ext}：中間 {len(gap)} 包沒收到（pieces_lost 累計 {self.lost}）")
            self.highest = ext
            if len(self.missing) > 4096:
                self.missing = {s for s in self.missing if s > ext - 4096}
            return ext, False
        if ext in self.missing:
            self.missing.discard(ext)
            self.lost -= 1
            return ext, False                                        # 亂序補到了：不是遺失
        return ext, True


# =============================================================== 3. video-send：3 次 HELLO → 每 200 ms 一張 → 3 次 BYE
# 「照真實時間送」的寫法：記下開始時刻 t0，第 k 張的截止時刻是 t0 + k / fps，睡到那一刻才送；這張的片一口氣送完（≤ 1216 bytes × 片數）。
# 不要寫 sleep(0.2)：sendto 本身也花時間、sleep 會多睡一點，誤差一張一張累積。
# ★ 對應 C：clock.h 的單調時鐘與 sleep_until；切片就是一個 for 迴圈：第 i 片 = jpeg + i × 1200，長度 min(1200, 剩下的)。
#
# 這支程式的約定（規格沒規定 HELLO／BYE 的 frame_id 等欄位）：HELLO 的 frame_id = 0；BYE 的 frame_id = 這次串流送了幾張；
# 兩者 piece_idx = piece_cnt = 0、payload 空。接收端對 HELLO／BYE 不檢查 piece_idx < piece_cnt（那只對 PIECE 有意義）。
def cmd_video_send(args):
    if args.jpegdir == "camera":
        print("video-send camera：攝影機（live 模式）不在 Python 版裡，請用 C 版；mock 模式請給 JPEG 序列的資料夾", file=sys.stderr)
        return 2
    if args.fps <= 0:
        raise ValueError("--fps 要是正數")
    frames = read_sequence(args.jpegdir)
    ssrc = random.getrandbits(32)                                    # 本次串流的識別碼；接收端看到不同的 ssrc 就重置
    period = 1.0 / args.fps
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (args.ip, args.port)
    total_bytes = sum(len(d) for _, d in frames)
    print(f"video-send：{args.jpegdir}（{len(frames)} 張，{total_bytes:,} bytes，{frames[0][0]}…{frames[-1][0]}）→ {args.ip}:{args.port}，"
          f"ssrc=0x{ssrc:08X}，{'一口氣送（--burst）' if args.burst else f'每 {period * 1000:.0f} ms 一張'}{'，--loop 循環' if args.loop else ''}", flush=True)
    seq = 0

    def send(packet):
        nonlocal seq
        try:
            sock.sendto(packet, dest)
        except (ConnectionResetError, OSError):
            pass                                                     # Windows：對方的 port 還沒開（ICMP）；UDP 沒有連線，照送
        seq += 1

    for _ in range(HELLO_REPEAT):                                    # 3 次 HELLO；seq 從 0 開始、照樣 +1
        send(pack_header(T_HELLO, seq, 0, 0, 0, ssrc))
    probe(f"HELLO × {HELLO_REPEAT}：seq=0…{seq - 1}，ssrc=0x{ssrc:08X}")
    t0 = time.perf_counter()
    n_frames = n_pieces = sent_bytes = 0
    try:
        while True:
            for name, jpeg in frames:
                if not args.burst:
                    # 第 k 張的截止時刻；sleep 的參數是「還要等多久」，已經晚了就不等（max 0）
                    time.sleep(max(0.0, t0 + n_frames * period - time.perf_counter()))
                cnt = (len(jpeg) + PIECE_MAX - 1) // PIECE_MAX       # ⌈大小／1200⌉ 片；最後一片不補 0
                first_seq = seq
                for idx, off in enumerate(range(0, len(jpeg), PIECE_MAX)):
                    packet = pack_header(T_PIECE, seq, n_frames, idx, cnt, ssrc) + jpeg[off:off + PIECE_MAX]
                    send(packet)
                    sent_bytes += len(packet)
                n_pieces += cnt
                if PROBE:
                    probe(f"送出 frame={n_frames}（{name}，{len(jpeg)} bytes）→ {cnt} 片 seq={first_seq}…{seq - 1}  延遲 {(time.perf_counter() - t0 - n_frames * period) * 1000:+.1f} ms")
                n_frames += 1
            if not args.loop:
                break
        if not args.burst:                                           # 最後一張的 200 ms 也要等完，BYE 才在下一個拍子送（duration 才是 60 × 200 ms）
            time.sleep(max(0.0, t0 + n_frames * period - time.perf_counter()))
    except KeyboardInterrupt:
        print("Ctrl-C：送 BYE 結束", flush=True)
    for _ in range(BYE_REPEAT):                                      # 3 次 BYE；seq 照樣 +1，frame_id = 送了幾張（見上面的約定）
        send(pack_header(T_BYE, seq, n_frames, 0, 0, ssrc))
    duration = time.perf_counter() - t0
    sock.close()
    probe(f"BYE × {BYE_REPEAT}：seq={seq - BYE_REPEAT}…{seq - 1}，frame_id={n_frames}")
    fps_actual = n_frames / duration if duration > 0 else 0.0
    print(f"STATS role=video-send frames={n_frames} pieces={n_pieces} bytes={sent_bytes} fps_target={args.fps:g} "
          f"fps_actual={fps_actual:.1f} duration_ms={duration * 1000:.0f}", file=sys.stderr, flush=True)
    return 0


# =============================================================== 4. video-recv：重組表、bounded queue、顯示時鐘
# 4a. 重組表：{frame_id: Frame}。Frame 記這張共幾片、已經收到哪些片（{piece_idx: bytes}）、第一片到達的時刻。
#   put(frame_id, idx, cnt, payload, now)   放一片；收齊就回傳整張的 bytes
#   expire(now, timeout)                    把逾時的（第一片 + timeout）與落後太多的（max_seen ≥ frame_id + 2）丟掉，回傳丟了幾張
# 規則（規格）：重複的片忽略；同一張收齊後（或已經丟掉後）再來的片忽略；表最多 8 張，滿了丟最舊的（也算 timeout）。
# C 版建議：固定 8 格的陣列，每格一個 struct { frame_id, cnt, got_mask 或 got[]，first_t, buf[cnt × 1200] }。
class Frame:
    __slots__ = ("cnt", "pieces", "t_first")

    def __init__(self, cnt, now):
        self.cnt, self.pieces, self.t_first = cnt, {}, now


class Reassembler:
    def __init__(self):
        self.table, self.finished, self.max_seen = {}, set(), None
        self.frames_complete = self.frames_dropped_timeout = 0
        self.assemble = []                                           # 每張收齊的：第一片 → 收齊的秒數

    def reset(self):
        """HELLO 或 ssrc 變了：重組表清空、frame_id 歸零（finished 也清掉，第二段的 frame 0 才收得進來）。計數器不清。"""
        self.table, self.finished, self.max_seen = {}, set(), None

    def put(self, frame_id, idx, cnt, payload, now):
        if frame_id in self.finished or (self.max_seen is not None and frame_id + 2 <= self.max_seen and frame_id not in self.table):
            probe(f"frame={frame_id} piece={idx}/{cnt}：這張已經收齊或丟掉了，忽略")
            return None
        fr = self.table.get(frame_id)
        if fr is None:
            if len(self.table) >= TABLE_CAP:                         # 表滿了：丟最舊的一張（算 timeout）
                oldest = min(self.table)
                del self.table[oldest]
                self.finished.add(oldest)
                self.frames_dropped_timeout += 1
                probe(f"重組表滿了（{TABLE_CAP} 張）：丟掉最舊的 frame={oldest}（算 timeout）")
            fr = self.table[frame_id] = Frame(cnt, now)
        if fr.cnt != cnt:
            probe(f"frame={frame_id} piece={idx}：piece_cnt {cnt} 和第一片說的 {fr.cnt} 不一樣，忽略")
            return None
        if idx in fr.pieces:
            probe(f"frame={frame_id} piece={idx}/{cnt}：重複，忽略")
            return None
        fr.pieces[idx] = payload
        if self.max_seen is None or frame_id > self.max_seen:
            self.max_seen = frame_id
        if len(fr.pieces) < cnt:
            probe(f"frame={frame_id} piece={idx}/{cnt}：已收 {len(fr.pieces)}/{cnt}  表={sorted(self.table)}")
            return None
        del self.table[frame_id]                                     # 收齊：接起來、記時間、之後再來的片都忽略
        self.finished.add(frame_id)
        self.frames_complete += 1
        self.assemble.append(now - fr.t_first)
        self._prune()
        probe(f"frame={frame_id} 收齊 {cnt} 片，{(now - fr.t_first) * 1000:.1f} ms")
        return b"".join(fr.pieces[i] for i in range(cnt))

    def expire(self, now, timeout):
        """逾時規則：第一片到達 + timeout 還沒收齊、或已經看到 frame_id ≥ 它 + 2 的片 → 丟。回傳丟掉的 frame_id list。"""
        gone = [fid for fid, fr in self.table.items()
                if now - fr.t_first >= timeout or (self.max_seen is not None and self.max_seen >= fid + 2)]
        for fid in gone:
            fr = self.table.pop(fid)
            self.finished.add(fid)
            self.frames_dropped_timeout += 1
            why = f"{(now - fr.t_first) * 1000:.0f} ms 還沒收齊" if now - fr.t_first >= timeout else f"已經看到 frame={self.max_seen}"
            probe(f"frame={fid} 丟掉（timeout）：{why}，只收到 {len(fr.pieces)}/{fr.cnt} 片")
        self._prune()
        return gone

    def drop_all(self, now):
        """結束時（idle 逾時、或 BYE 之後等不到）：表裡還沒收齊的都算 timeout。"""
        for fid in list(self.table):
            fr = self.table.pop(fid)
            self.frames_dropped_timeout += 1
            probe(f"frame={fid} 丟掉（timeout，結束了）：只收到 {len(fr.pieces)}/{fr.cnt} 片")

    def next_deadline(self, timeout):
        return min((fr.t_first + timeout for fr in self.table.values()), default=None)

    def _prune(self):
        if len(self.finished) > 256:                                 # 「已經處理過的 frame_id」只留最近的，--loop 跑很久也不會長大
            self.finished = {f for f in self.finished if f > self.max_seen - 64}


# 4b. 顯示：bounded queue 與顯示時鐘。
#   push(frame_id, jpeg, now)   收齊的一張進 queue；滿了（--queue）丟最舊的，計 frames_dropped_queue
#   tick(now)                   顯示時鐘的一拍：queue 有東西就取最舊的一張寫成 outdir/NNNN.jpg、記進 shown.txt；空的計 display_idle
# 時鐘從「第一張收齊」那一刻開始，之後每 200 ms 一拍，不等網路（規格：顯示端有自己的時鐘）。HELLO／換 ssrc 時時鐘也重來。
# fps_actual = frames_shown ÷ 顯示的總秒數；「顯示的總秒數」= 拍數 × 0.2（每一段串流從第一拍到最後一拍，含 idle 的拍）。
# C 版建議：queue 用 n 格的環狀陣列；live 模式顯示是另一條執行緒（display_show），queue 要加 mutex。
class Display:
    def __init__(self, outdir, cap):
        self.outdir, self.cap = outdir, cap
        self.queue = []                                              # [(frame_id, bytes, 第一片到達時刻), …]，最舊的在前面
        self.t0 = self.ticks = None
        self.total_ticks = 0
        self.frames_shown = self.frames_dropped_queue = self.display_idle = 0
        self.t_start = time.perf_counter()
        self.shown_path = os.path.join(outdir, "shown.txt")
        open(self.shown_path, "w").close()                           # 每次執行從空的開始；之後逐行 append

    def reset(self):
        """HELLO 或 ssrc 變了：queue 清空、時鐘重來。計數器不清。"""
        if self.ticks:
            self.total_ticks += self.ticks
        self.queue, self.t0, self.ticks = [], None, None

    def push(self, frame_id, jpeg, t_first, now):
        if len(self.queue) >= self.cap:
            old = self.queue.pop(0)
            self.frames_dropped_queue += 1
            probe(f"queue 滿了（{self.cap}）：丟掉最舊的 frame={old[0]}（frames_dropped_queue={self.frames_dropped_queue}）")
        self.queue.append((frame_id, jpeg, t_first))
        if self.t0 is None:
            self.t0, self.ticks = now, 0                             # 第一張收齊：顯示時鐘從現在開始
            probe(f"顯示時鐘開始：第 i 拍在 +{FRAME_SEC * 1000:.0f}·i ms")

    def next_tick(self):
        return None if self.t0 is None else self.t0 + self.ticks * FRAME_SEC

    def tick(self, now):
        """一拍。回傳有沒有顯示東西。"""
        self.ticks += 1
        ids = [q[0] for q in self.queue]
        if not self.queue:
            self.display_idle += 1
            probe(f"tick {self.ticks - 1:4d}  queue=[]  空的（display_idle={self.display_idle}）")
            return False
        frame_id, jpeg, t_first = self.queue.pop(0)
        with open(os.path.join(self.outdir, f"{frame_id:04d}.jpg"), "wb") as f:
            f.write(jpeg)
        t_ms = (now - self.t_start) * 1000
        with open(self.shown_path, "a") as f:
            f.write(f"{frame_id},{t_ms:.0f}\n")
        self.frames_shown += 1
        probe(f"tick {self.ticks - 1:4d}  queue={ids}  顯示 frame={frame_id}（{len(jpeg)} bytes，第一片到顯示 {(now - t_first) * 1000:.0f} ms）")
        return True

    def fps_actual(self):
        ticks = self.total_ticks + (self.ticks or 0)
        return self.frames_shown / (ticks * FRAME_SEC) if ticks else 0.0


# 4c. 主迴圈。一條執行緒、一個 select：每一輪算出「下一件事」是什麼時候（下一拍、重組表最早的逾時、idle 逾時、BYE 的寬限），
# 有封包就收、到了就做。結束條件（mock）：
#   BYE    等 queue 顯示完（表裡還沒收齊的給它 --timeout 的機會），再等 BYE_GRACE_SEC 看有沒有新的 HELLO（掛斷再連）→ 沒有就結束
#   idle   --idle ms 沒有任何合法封包：表裡的算 timeout、queue 顯示完就結束（送端被殺掉的情況）
#   第一包來之前的等待不受 --idle 限制（人手動開兩個視窗會超過 3 秒），但也不會永久等：最多 FIRST_WAIT 秒
def cmd_video_recv(args):
    if args.queue <= 0 or args.timeout <= 0 or args.idle <= 0:
        raise ValueError("--queue、--timeout、--idle 都要是正整數")
    if args.display == "window":
        print("--display window：Python 版沒有視窗，只寫檔（等同 --display none）；要開視窗請用 C 版（wrapper 的 display.h）", flush=True)
    os.makedirs(args.outdir, exist_ok=True)
    asm, disp, seqs = Reassembler(), Display(args.outdir, args.queue), SeqTracker()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)   # --burst 時一次來上千包，別讓作業系統先丟
    if hasattr(socket, "SIO_UDP_CONNRESET"):                         # Windows：對方 port 沒開時 ICMP 會讓 recvfrom 丟 ConnectionResetError，關掉它
        sock.ioctl(socket.SIO_UDP_CONNRESET, False)
    sock.bind((args.bind, args.port))
    sock.setblocking(False)
    print(f"video-recv：聽 {args.bind}:{args.port}，queue={args.queue} timeout={args.timeout} ms idle={args.idle} ms → {args.outdir}/NNNN.jpg、shown.txt", flush=True)

    FIRST_WAIT = max(args.idle / 1000.0 * 10, 30.0)
    timeout, idle = args.timeout / 1000.0, args.idle / 1000.0
    pieces_received = pieces_bad = lost_before = 0                   # lost_before：前幾段串流（換 ssrc 之前）累計的 pieces_lost
    ssrc = last_rx = bye_at = idle_at = None
    t_start = time.perf_counter()

    def reset(why):
        nonlocal seqs, lost_before, bye_at
        lost_before += seqs.lost
        seqs = SeqTracker()
        asm.reset()
        disp.reset()
        bye_at = None
        probe(f"{why}：重組表與 queue 清空、frame_id 歸零、顯示時鐘重來")

    try:
        while True:
            now = time.perf_counter()
            # 1. 算「下一件事」是什麼時候
            events = [disp.next_tick(), asm.next_deadline(timeout) if asm.table else None]
            if bye_at is not None:
                events.append(bye_at + BYE_GRACE_SEC)
            elif idle_at is None:
                events.append(last_rx + idle if last_rx is not None else t_start + FIRST_WAIT)
            deadline = min(x for x in events if x is not None)
            # 2. 結束條件
            if idle_at is not None and not disp.queue:
                break
            if bye_at is not None and not disp.queue and not asm.table and now - bye_at >= BYE_GRACE_SEC:
                break
            # 3. 等封包，最多等到 deadline。Windows 的 select 逾時解析度只有 ~15 ms，顯示時鐘會抖；
            #    所以用 select(0) 看一下有沒有封包，沒有就 sleep 最多 1 ms 再看
            readable, _, _ = select.select([sock], [], [], 0)
            if not readable and deadline - now > 0:
                time.sleep(min(deadline - now, 0.001))
                continue
            now = time.perf_counter()
            # 4. 把現在可以收的封包全部收進來（可能不只一個）
            while readable:
                try:
                    data, _ = sock.recvfrom(65535)
                except BlockingIOError:
                    break
                except OSError:
                    continue                                         # Windows 的 ConnectionResetError 之類：和我們無關，繼續
                pkt = parse_packet(data)
                if pkt is None:
                    pieces_bad += 1
                    probe(f"壞封包 {len(data)} bytes（magic／長度／piece_idx 不對），丟掉；累計 {pieces_bad}")
                    continue
                ptype, seq, frame_id, piece_idx, piece_cnt, pkt_ssrc, payload = pkt
                last_rx = now
                if idle_at is not None:
                    continue                                         # 已經在收尾了（idle）：不再收新東西
                if ssrc is None or pkt_ssrc != ssrc:                 # 第一包、或對方重連（新 ssrc）
                    if ssrc is not None:
                        reset(f"ssrc 從 0x{ssrc:08X} 變成 0x{pkt_ssrc:08X}")
                    else:
                        probe(f"第一包到了（type={ptype} seq={seq} ssrc=0x{pkt_ssrc:08X}）")
                    ssrc = pkt_ssrc
                prev_highest = seqs.highest
                ext, dup = seqs.note(seq)
                if ptype == T_HELLO:
                    # 同一個 ssrc 的 HELLO 也當作重新開始（對方 /call 重連但 ssrc 沒換）——但只有「比目前看過的都新」的 HELLO 才算：
                    # 開頭 3 次 HELLO 被亂序扣住、晚於第一片才到的，seq 比片小，是遲到的舊包，忽略（不然會把剛收的 frame 0 清掉）
                    if prev_highest is not None and ext <= prev_highest:
                        probe(f"HELLO（seq={seq}）：比已經收到的封包舊（亂序遲到），忽略")
                    elif asm.table or disp.queue or disp.t0 is not None or bye_at is not None:
                        reset(f"HELLO（seq={seq}）")
                    else:
                        probe(f"HELLO（seq={seq}）")
                elif ptype == T_PIECE:
                    pieces_received += 1
                    jpeg = asm.put(frame_id, piece_idx, piece_cnt, payload, now)
                    if jpeg is not None:
                        disp.push(frame_id, jpeg, now - asm.assemble[-1], now)
                elif bye_at is None:
                    bye_at = now
                    probe(f"BYE（seq={seq} frame_id={frame_id}）：queue（{[q[0] for q in disp.queue]}）顯示完、表（{sorted(asm.table)}）收齊或逾時後，再等 {BYE_GRACE_SEC:.0f} s 看有沒有新的 HELLO")
            # 5. 逾時規則：第一片 + timeout、或 frame_id 落後 2
            asm.expire(now, timeout)
            # 6. idle：這麼久沒有任何封包 → 送端不見了，收尾
            if idle_at is None and bye_at is None and last_rx is not None and now - last_rx >= idle:
                idle_at = now
                print(f"{args.idle} ms 沒有封包，當作結束", flush=True)
                asm.drop_all(now)
            if last_rx is None and now - t_start >= FIRST_WAIT:
                print(f"等了 {FIRST_WAIT:.0f} 秒都沒有封包，結束", flush=True)
                break
            # 7. 顯示時鐘：到了拍子就顯示（程式停頓太久的話一次補齊好幾拍）。
            #    收尾（BYE 或 idle）而且 queue 已經空了就停下時鐘：之後的拍子不算 display_idle、也不算進 fps_actual 的秒數
            while disp.t0 is not None and now >= disp.next_tick() and not ((bye_at is not None or idle_at is not None) and not disp.queue):
                disp.tick(now)
    except KeyboardInterrupt:
        print("Ctrl-C：結束", flush=True)
    if bye_at is not None and asm.table:
        asm.drop_all(time.perf_counter())
    sock.close()

    if pieces_bad:
        print(f"丟掉了 {pieces_bad} 個壞封包（magic、長度或 piece_idx 不對）", flush=True)
    avg = sum(asm.assemble) / len(asm.assemble) * 1000 if asm.assemble else 0.0
    mx = max(asm.assemble) * 1000 if asm.assemble else 0.0
    print(f"已存檔：{args.outdir}（{disp.frames_shown} 張，shown.txt）", flush=True)
    print(f"STATS role=video-recv queue={args.queue} timeout_ms={args.timeout} frames_shown={disp.frames_shown} frames_complete={asm.frames_complete} "
          f"frames_dropped_timeout={asm.frames_dropped_timeout} frames_dropped_queue={disp.frames_dropped_queue} "
          f"pieces_received={pieces_received} pieces_lost={lost_before + seqs.lost} pieces_bad={pieces_bad} "
          f"fps_actual={disp.fps_actual():.1f} avg_assemble_ms={avg:.1f} max_assemble_ms={mx:.1f} display_idle={disp.display_idle}",
          file=sys.stderr, flush=True)
    return 0


# =============================================================== 5. main
# argparse 的 subcommand：第一個字決定後面接受哪些參數。call／devices 有登記，但只印一句話、結束碼 2（live 模式是 C 的工作）。
# 結束碼：0 正常；1 網路錯誤（OSError）；2 參數或檔案錯誤（ValueError、argparse 的錯誤）。
def main():
    global PROBE
    ap = argparse.ArgumentParser(description="MiniLINE 純 Python 完整版（mock 模式，對照 C 版用）")
    ap.add_argument("--probe", action="store_true", help="印出每一片、每一張、每一拍的中間過程")
    sub = ap.add_subparsers(dest="cmd", required=True)
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--probe", action="store_true", default=argparse.SUPPRESS)
    p = sub.add_parser("video-send", parents=[common]); p.add_argument("ip"); p.add_argument("port", type=int); p.add_argument("jpegdir")
    p.add_argument("--fps", type=float, default=5); p.add_argument("--quality", type=int, default=75, help="只有 camera（C 版）用得到；mock 不重新壓縮")
    p.add_argument("--burst", action="store_true", help="不等 200 ms，一口氣全送"); p.add_argument("--loop", action="store_true", help="送完從頭再送，Ctrl-C 才停")
    p = sub.add_parser("video-recv", parents=[common]); p.add_argument("port", type=int); p.add_argument("outdir")
    p.add_argument("--bind", default="0.0.0.0"); p.add_argument("--queue", type=int, default=3); p.add_argument("--timeout", type=int, default=400)
    p.add_argument("--idle", type=int, default=3000); p.add_argument("--display", choices=["none", "window"], default="none")
    sub.add_parser("call", parents=[common], add_help=False)
    sub.add_parser("devices", parents=[common], add_help=False)
    args, _ = ap.parse_known_args() if sys.argv[1:2] in (["call"], ["devices"]) else (ap.parse_args(), None)
    PROBE = args.probe
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    if args.cmd in ("call", "devices"):
        print(f"{args.cmd}：live 模式（攝影機、麥克風、三條管線）不在 Python 版裡，請用 C 版", file=sys.stderr)
        return 2
    if args.port not in range(1, 65536):
        print("錯誤: port 要是 1–65535 的整數", file=sys.stderr)
        return 2
    try:
        return {"video-send": cmd_video_send, "video-recv": cmd_video_recv}[args.cmd](args)
    except ValueError as why:
        print(f"錯誤: {why}", file=sys.stderr)
        return 2
    except OSError as why:
        print(f"錯誤: {why}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
