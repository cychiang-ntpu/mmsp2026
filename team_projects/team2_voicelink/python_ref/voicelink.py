#!/usr/bin/env python3
"""VoiceLink 的純 Python 完整版（mock 模式）：功能與 C 版的目標相同，用來「對照著看」，也是評測的對手端。只用標準函式庫。

  python3 voicelink.py send <ip> <port> <in.wav> [--burst]
      讀 16 kHz／16-bit／單聲道的 WAV，每 20 ms 送一包（320 samples＝640 bytes）；--burst 一口氣全送
  python3 voicelink.py recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]
      收封包 → jitter buffer → 照 20 ms 的節奏「播放」（寫進 WAV）→ 結束印 STATS
  任何指令都可以加 --probe：把每一拍 buffer 裡有哪些 seq、播了誰、補了誰、每個封包的命運印出來（stderr）
  call 與 devices（live 模式）不在 Python 版裡，要做的是 C。

這支程式刻意用「高階」的寫法，讓你一眼看到每一步在做什麼：
  封包    struct.pack(">BBHII", …) 一行打包 12 bytes 的 header；解析用 struct.unpack
  buffer  dict：{延伸過的 seq: (pcm, 到達時刻)}；「下一個該播的在不在」就是 seq in buf
  時鐘    time.perf_counter() 的絕對時刻：第 i 拍 = 開播時刻 + i × 0.02，睡到那一刻再動作（不是 sleep(0.02)，誤差不會累積）
  等待    select.select([sock], [], [], 到下一拍還有多久)：有封包就收、到拍子就播，一條執行緒就夠
C 沒有這些現成的東西：dict 要換成固定大小的環狀陣列（以 seq 取餘數當索引）、select 要換成 recvfrom 加逾時（wrapper 的 udp.h）、
perf_counter 換成 clock.h 的單調時鐘。把這裡的每一步「翻譯」成 C，就是 Team 2 的工作；觀念一樣，但程式不會長得一樣。
"""
# ---------------------------------------------------------------------------------------------------------------
# 怎麼讀這支程式：由下層往上層分成五個區塊。
#   1. WAV      逐 chunk 找 data 區，檢查格式（16 kHz、16-bit、單聲道）；寫出用 wave 模組
#   2. 封包     12 bytes header｜640 bytes PCM；pack／unpack；seq 的 16-bit 繞回 → 延伸序號
#   3. send     每 20 ms 一包，最後 3 次 BYE，印 STATS role=send
#   4. recv     jitter buffer 與播放時鐘：收到的封包怎麼分類（late／dup／reordered／overflow）、每一拍播誰或補誰
#   5. main     命令列
#
# 和規格的對應（★ 對應 C 的地方就是你們要寫的那一段）：
#   封包格式          pack_header／parse_packet
#   jitter buffer     JitterBuffer.put（分類與計數）、JitterBuffer.tick（播或補）
#   播放時鐘          cmd_recv 的主迴圈（select 的逾時 = 到下一拍的時間）
#   PLC               JitterBuffer.tick 裡 plc == "zero" 與 "repeat" 的兩行
#   STATS             cmd_send／cmd_recv 最後的 print(…, file=sys.stderr)
#
# 建議的讀法：先開兩個視窗跑 recv --probe 與 send，看每一拍印出來的 buffer；再夾上 udp_impair.py 看 late／dup／reordered 怎麼出現。
# ---------------------------------------------------------------------------------------------------------------
import argparse
import random
import select
import socket
import struct
import sys
import time
import wave

# 常數（數值是規格訂的；C 版請放在 header 檔）。
#   MAGIC／T_AUDIO／T_BYE   header 的第 0、1 byte
#   SAMPLE_RATE             只接受 16 kHz
#   FRAME_SAMPLES           20 ms = 320 samples；FRAME_BYTES = 640
#   HDR_LEN                 header 12 bytes；AUDIO 封包 652 bytes；BYE 封包只有 header 12 bytes
#   FRAME_SEC               播放時鐘一拍的長度（秒）
#   BYE_REPEAT              最後送幾次 BYE（UDP 可能掉）
PROBE = False
MAGIC, T_AUDIO, T_BYE = 0x56, 0x01, 0x02
SAMPLE_RATE, FRAME_SAMPLES = 16000, 320
FRAME_BYTES, HDR_LEN = FRAME_SAMPLES * 2, 12
AUDIO_LEN = HDR_LEN + FRAME_BYTES
FRAME_SEC = FRAME_SAMPLES / SAMPLE_RATE
BYE_REPEAT = 3
SILENCE = bytes(FRAME_BYTES)


# --probe 的輸出都經過這個函式，印到 stderr，不和正常輸出混在一起。
def probe(msg):
    if PROBE:
        print(f"[probe] {msg}", file=sys.stderr, flush=True)


# =============================================================== 1. WAV
# RIFF 的結構：12 bytes 的 "RIFF"｜大小｜"WAVE"，之後是一串 chunk（id 4 bytes｜size 4 bytes little-endian｜內容｜奇數補 1 byte）。
# data 不一定在第 44 byte（fmt 與 data 之間可能有 LIST 等 chunk），所以逐個 chunk 走（第 3 週 2.6）。
# 格式不對就丟 ValueError；main 接住後印錯誤、結束碼 2。
#   struct.unpack("<HHIIHH", 16 bytes)：fmt 的六個欄位——格式代碼（1 = PCM）、聲道數、取樣率、每秒 bytes、block align、每 sample 幾 bits
# ★ 對應 C：wrapper 的 wav.h 已經會讀；你們只要確認它拒絕不是 16 kHz／16-bit／單聲道的檔案（結束碼 2）。
def read_wav_pcm(path):
    """回傳 16 kHz／16-bit／單聲道 WAV 的 PCM bytes（little-endian，原樣）。格式不對就丟 ValueError。"""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("不是 RIFF/WAVE 檔")
    pos, fmt_ok = 12, False
    while pos + 8 <= len(data):
        chunk_id, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        if chunk_id == b"fmt ":
            if size < 16 or pos + 24 > len(data):
                raise ValueError("fmt chunk 太短")
            audio_format, channels, rate, _, _, bits = struct.unpack("<HHIIHH", data[pos + 8:pos + 24])
            if audio_format != 1 or bits != 16 or channels != 1 or rate != SAMPLE_RATE:
                raise ValueError(f"只接受 16 kHz／16-bit PCM／單聲道（這個檔是 format={audio_format} {rate} Hz {bits}-bit {channels} ch）")
            fmt_ok = True
        elif chunk_id == b"data":
            if not fmt_ok:
                raise ValueError("data 之前沒有 fmt")
            start = pos + 8
            end = start + min(size, len(data) - start) // 2 * 2       # size 可能大於實際剩下的（檔案被截斷）；奇數 byte 不算 sample
            return data[start:end]
        pos += 8 + size + (size & 1)
    raise ValueError("找不到 data chunk")


# wave 模組寫出標準的 44-byte header。PCM 是 bytes，原樣寫進去（它不會動 sample 的內容）。
def write_wav_pcm(path, pcm):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(pcm)


# =============================================================== 2. 封包：12 bytes header｜640 bytes PCM
# header 全部 big-endian（網路位元組序）；PCM 維持 WAV 的 little-endian，原樣搬。
#   ">BBHII"：B = 1 byte、H = 16-bit、I = 32-bit，共 1 + 1 + 2 + 4 + 4 = 12 bytes。
#   seq & 0xFFFF：送到 65535 之後繞回 0（規格）；ts 是 32-bit，320 × k 要 2^32 / 320 / 50 ≈ 3.7 天才會繞回。
# ★ 對應 C：不可以把 struct 直接 memcpy 到封包（對齊與 byte 順序都會錯）；一個 byte 一個 byte 用位移填：p[2] = seq >> 8; p[3] = seq;
def pack_header(ptype, seq, ts, ssrc):
    return struct.pack(">BBHII", MAGIC, ptype, seq & 0xFFFF, ts & 0xFFFFFFFF, ssrc)


# 解析收到的 datagram。合法的只有兩種長度：652（AUDIO）與 12（BYE）；magic 要是 0x56；type 要是 1 或 2。
# 不合法就回傳 None——呼叫端計數後丟掉，程式不可以當掉（規格：壞封包測試）。
# ★ 對應 C：這就是「先檢查長度、再讀欄位」：長度不到 12 就去讀 header 是讀到陣列外面。
def parse_packet(data):
    """bytes → (type, seq, ts, ssrc, pcm)；不合法回傳 None。"""
    if len(data) not in (HDR_LEN, AUDIO_LEN) or data[0] != MAGIC:
        return None
    magic, ptype, seq, ts, ssrc = struct.unpack(">BBHII", data[:HDR_LEN])
    if ptype == T_AUDIO and len(data) == AUDIO_LEN:
        return ptype, seq, ts, ssrc, data[HDR_LEN:]
    if ptype == T_BYE and len(data) == HDR_LEN:
        return ptype, seq, ts, ssrc, b""
    return None


# 16-bit 的 seq 會繞回：65535 的下一個是 0。比大小、算遺失都不能直接用它，要先換成「延伸序號」（RTP 也是這樣做）。
# 做法：和目前看過最大的延伸序號比，差距取 16-bit 的有號差（-32768 … +32767）再加回去。
#   例：highest = 65535，收到 seq = 0 → (0 - 65535) & 0xFFFF = 1 → 65536（不是 0）
#       highest = 65536，收到 seq = 65534 → 差 -2 → 65534（遲到的舊包，不是未來的）
# ★ 對應 C：同樣的兩行，記得用 uint16_t 相減再轉成 int16_t。
def extend_seq(seq, highest):
    if highest is None:
        return seq
    diff = (seq - highest) & 0xFFFF
    if diff >= 0x8000:
        diff -= 0x10000
    return highest + diff


# =============================================================== 3. send：每 20 ms 一包，最後 3 次 BYE
# 「照真實時間送」的寫法：記下開始時刻 t0，第 k 包的截止時刻是 t0 + k × 0.02，睡到那一刻才送。
# 不要寫 sleep(0.02)：sendto 本身也花時間、sleep 會多睡一點，誤差一包一包累積，30 秒下來會慢好幾百 ms。
# ★ 對應 C：clock.h 的單調時鐘與 sleep_until；Windows 的 Sleep 預設只有 ~15 ms 的解析度，wrapper 已經處理。
#
# 最後一包不足 640 bytes 時補 0（規格）。但 header 沒有「這包有幾個 sample」的欄位，接收端怎麼知道要去掉補的 0？
# 這支程式的約定：BYE 的 ts = 整個檔案真正的 sample 數（AUDIO 的 ts 是 320 × k，BYE 的 ts 規格沒有規定，所以拿來放這個）。
# 接收端收到 BYE 就把輸出截到這個長度；BYE 全部掉了（三次都掉）就留著補的 0，長度多 320 個 sample 以內，可以接受。
# 你們的 C 版可以採用同一個約定（和這支程式互測時長度就會逐 byte 相同），也可以不採用（評測的「逐 byte 相同」會容忍最後一包的補 0）。
def cmd_send(args):
    pcm = read_wav_pcm(args.wav)
    n_samples = len(pcm) // 2
    n_frames = (len(pcm) + FRAME_BYTES - 1) // FRAME_BYTES          # 無條件進位：最後不足一包也算一包
    ssrc = random.getrandbits(32)                                    # 本次串流的識別碼；接收端看到不同的 ssrc 就重置 buffer
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (args.ip, args.port)
    print(f"send：{args.wav}（{n_samples:,} samples，{n_frames} 包）→ {args.ip}:{args.port}，ssrc=0x{ssrc:08X}，{'一口氣送（--burst）' if args.burst else '每 20 ms 一包'}", flush=True)
    t0 = time.perf_counter()
    seq, sent_bytes = 0, 0
    for k in range(n_frames):
        if not args.burst:
            # 第 k 包的截止時刻；sleep 的參數是「還要等多久」，已經晚了就不等（max 0）
            time.sleep(max(0.0, t0 + k * FRAME_SEC - time.perf_counter()))
        frame = pcm[k * FRAME_BYTES:(k + 1) * FRAME_BYTES]
        frame = frame + bytes(FRAME_BYTES - len(frame))              # 最後一包補 0 到 640 bytes
        packet = pack_header(T_AUDIO, seq, FRAME_SAMPLES * k, ssrc) + frame
        try:
            sock.sendto(packet, dest)
        except ConnectionResetError:
            pass                                                     # Windows：對方的 port 還沒開（ICMP）；UDP 沒有連線，照送
        sent_bytes += len(packet)
        if PROBE and (k < 3 or k == n_frames - 1):
            probe(f"送出 seq={seq} ts={FRAME_SAMPLES * k} {len(packet)} bytes  header={packet[:HDR_LEN].hex(' ')}  延遲 {(time.perf_counter() - t0 - k * FRAME_SEC) * 1000:+.2f} ms")
        seq += 1
    for _ in range(BYE_REPEAT):                                      # 3 次 BYE；seq 照樣 +1，ts = 真正的 sample 數（見上面的說明）
        try:
            sock.sendto(pack_header(T_BYE, seq, n_samples, ssrc), dest)
        except ConnectionResetError:
            pass
        seq += 1
    duration_ms = (time.perf_counter() - t0) * 1000
    sock.close()
    probe(f"BYE × {BYE_REPEAT}：seq={n_frames}…{seq - 1}，ts={n_samples}（真正的 sample 數，接收端用來去掉最後一包補的 0）")
    print(f"STATS role=send frames={n_frames} bytes={sent_bytes} duration_ms={duration_ms:.0f} pace={0 if args.burst else 20}", file=sys.stderr, flush=True)
    return 0


# =============================================================== 4. recv：jitter buffer 與播放時鐘
# 狀態都放在一個 class 裡（C：一個 struct）。兩個入口：
#   put(seq, ts, ssrc, pcm, now)   收到一個 AUDIO：分類（late／dup／reordered／overflow）後放進 buf 或丟掉
#   tick(now)                      播放時鐘的一拍：buf 裡有「下一個該播的 seq」就播它，沒有就 PLC 補一個；回傳 640 bytes
# buf 是 dict {延伸序號: (pcm, 到達時刻)}。C 版建議用環狀陣列：index = seq % 容量，每格記「有沒有東西」與 seq。
#
# 計數器（STATS 的欄位）的定義，設計成彼此一致：
#   received   合法的 AUDIO 封包數（含重複、遲到、溢位的）
#   late       到的時候它的拍子已經過了（seq < expected），而且那個 seq 不是「已經用真的封包播過」
#   dup        同一個 seq 已經在 buf 裡、或已經用真的封包播過了
#   reordered  seq 比目前看過最大的小（亂序到達），但還來得及（放進 buf）
#   overflow   buf 滿了（上限 = 4 × buffer 的 frame 數）丟掉最舊的
#   concealed  播放時 buf 裡沒有那個 seq 的次數（PLC 補的 frame 數）
#   lost       結束時，從第一個播的 seq 到最後一個，完全沒收到過的 seq 數（亂序或遲到才到的不算）
#   frames_out = 用真的封包播的 + concealed；而「用真的封包播的」= received − late − dup − overflow（buffer 最後有排空）
#   沒有搗蛋時 lost = late = dup = reordered = overflow = concealed = 0，輸出 PCM 與輸入逐 byte 相同。
class JitterBuffer:
    def __init__(self, buffer_frames, plc):
        self.cap = 4 * buffer_frames                                 # 上限：--buffer 的 4 倍
        self.plc = plc
        self.reset()
        self.received = self.late = self.dup = self.reordered = self.overflow = self.concealed = 0
        self.frames_out = 0
        self.trailing_concealed = 0
        self.delays = []                                             # 每個真的播出去的封包：到達 → 播放的秒數
        self.last_frame = SILENCE                                    # repeat 用：上一個「收到的」frame

    def reset(self):
        """換了 ssrc（新的串流）時呼叫：buf、序號、播放進度全部從頭來。計數器不清。"""
        self.buf, self.highest, self.expected, self.ssrc = {}, None, None, None
        self.seen, self.played_real = set(), set()                   # 收到過的延伸序號／用真的封包播過的延伸序號
        self.first_ext, self.end_seq = None, None

    def put(self, seq, ts, ssrc, pcm, now):
        if self.ssrc is not None and ssrc != self.ssrc:
            probe(f"ssrc 從 0x{self.ssrc:08X} 變成 0x{ssrc:08X}：重置 buffer")
            self.reset()
        self.ssrc = ssrc
        self.received += 1
        ext = extend_seq(seq, self.highest)
        if ext in self.buf or ext in self.played_real:
            self.dup += 1
            probe(f"收到 seq={seq}（ext {ext}）：重複，丟掉")
            return
        if self.expected is not None and ext < self.expected:
            self.late += 1
            self.seen.add(ext)                                       # 到了，只是太晚：不算 lost
            probe(f"收到 seq={seq}（ext {ext}）：遲到（已經播到 {self.expected}），丟掉")
            return
        if self.highest is not None and ext < self.highest:
            self.reordered += 1
            probe(f"收到 seq={seq}（ext {ext}）：亂序（最大是 {self.highest}），還來得及，排回去")
        else:
            self.highest = ext
        self.seen.add(ext)
        if self.expected is None and (self.first_ext is None or ext < self.first_ext):
            self.first_ext = ext                                     # 開播前看過最小的 seq：開播就從它開始
        self.buf[ext] = (pcm, now)
        if self.end_seq is not None and ext >= self.end_seq:         # BYE 之後才到的更後面的包（BYE 被亂序超前了）：把結尾往後推
            self.end_seq = ext + 1
        if len(self.buf) > self.cap:
            oldest = min(self.buf)
            del self.buf[oldest]
            self.overflow += 1
            probe(f"buffer 滿了（{self.cap}）：丟掉最舊的 ext {oldest}")

    def start(self):
        """開播：下一個該播的 = 開播前看過最小的 seq（第一包可能是亂序到的；--burst 時它可能已經被 overflow 丟掉，那就補）。"""
        if self.first_ext is None:
            self.first_ext = 0
        self.expected = self.first_ext

    def tick(self, now):
        """播放時鐘的一拍。回傳要交給喇叭（寫進 WAV）的 640 bytes。"""
        seqs = sorted(self.buf)
        if self.expected in self.buf:
            pcm, arrived = self.buf.pop(self.expected)
            self.played_real.add(self.expected)
            self.delays.append(now - arrived)
            self.last_frame = pcm
            action, out = f"play  delay={(now - arrived) * 1000:.1f} ms", pcm
            self.trailing_concealed = 0
        else:
            self.concealed += 1
            self.trailing_concealed += 1                             # 連續補了幾個（逾時收尾時要把「送端死掉之後」補的去掉）
            out = SILENCE if self.plc == "zero" else self.last_frame     # ★ PLC：zero 補靜音；repeat 重複上一個收到的 frame
            action = f"conceal({self.plc})"
        probe(f"tick {self.frames_out:5d}  expected={self.expected:<6d} buf={seqs}  {action}")
        self.expected += 1
        self.frames_out += 1
        return out

    def finish_at(self, end_seq):
        """知道串流到哪裡結束了（BYE 算出來的、或逾時時看過的最大 seq + 1）。之後會一直 tick 到 expected == end_seq。"""
        self.end_seq = max(end_seq, (self.highest + 1) if self.highest is not None else 0)

    def trim_tail(self):
        """逾時收尾（送端死了）：播放時鐘在等待的這 --timeout 期間一直在補 frame，但那些是「送端死掉之後」的拍子，
        不是遺失補償——從輸出去掉，frames_out／concealed 也減回來。回傳要從輸出尾巴去掉的 frame 數。"""
        extra = self.expected - self.end_seq if self.expected is not None else 0
        extra = min(max(extra, 0), self.trailing_concealed)
        self.expected -= extra
        self.frames_out -= extra
        self.concealed -= extra
        self.trailing_concealed -= extra
        return extra

    def done(self):
        return self.end_seq is not None and self.expected is not None and self.expected >= self.end_seq

    def lost(self):
        if self.first_ext is None or self.end_seq is None:
            return 0
        return sum(1 for s in range(self.first_ext, self.end_seq) if s not in self.seen)


def cmd_recv(args):
    if args.buffer <= 0 or args.buffer % 20 != 0:
        raise ValueError("--buffer 要是 20 的正倍數")
    if args.timeout <= 0:
        raise ValueError("--timeout 要是正整數")
    buffer_frames = args.buffer // 20
    jb = JitterBuffer(buffer_frames, args.plc)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)   # --burst 時一次來幾千包，別讓作業系統先丟
    sock.bind((args.bind, args.port))
    sock.setblocking(False)
    print(f"recv：聽 {args.bind}:{args.port}，buffer={args.buffer} ms（{buffer_frames} 包，上限 {jb.cap}），plc={args.plc}，timeout={args.timeout} ms → {args.wav}", flush=True)

    # 時鐘與結束條件：
    #   play_t0    開播時刻 = 第一包到達 + buffer ms；第 i 拍在 play_t0 + i × 0.02
    #   last_rx    最後一次收到合法封包的時刻；超過 --timeout 沒有封包就當作送端死了 → 排空 buffer 後結束
    #   第一包來之前的等待不受 --timeout 限制（人手動開兩個視窗會超過 2 秒），但也不會永久等：最多等 FIRST_WAIT 秒
    #   bye_ts     第一個 BYE 的 ts（真正的 sample 數，用來去掉最後一包補的 0；見 cmd_send 的說明）
    FIRST_WAIT = max(args.timeout / 1000.0 * 10, 30.0)
    timeout = args.timeout / 1000.0
    out = bytearray()
    play_t0 = last_rx = None
    bye_ts, bad = None, 0
    t_start = time.perf_counter()
    try:
        while True:
            now = time.perf_counter()
            # 1. 算「下一件事」是什麼時候：下一拍、或逾時
            if play_t0 is not None:
                next_tick = play_t0 + jb.frames_out * FRAME_SEC
            else:
                next_tick = None
            deadline = min(x for x in (next_tick, (last_rx + timeout) if last_rx is not None else (t_start + FIRST_WAIT)) if x is not None)
            if jb.done():
                break
            # 2. 等封包，最多等到 deadline。Windows 的 select 逾時解析度只有 ~15 ms，播放時鐘會抖；
            #    所以用 select(0) 看一下有沒有封包，沒有就 sleep 最多 1 ms 再看（Python 3.11+ 的 sleep 在 Windows 也是 1 ms 準）
            readable, _, _ = select.select([sock], [], [], 0)
            if not readable and deadline - now > 0:
                time.sleep(min(deadline - now, 0.001))
                continue
            now = time.perf_counter()
            # 3. 把現在可以收的封包全部收進來（可能不只一個）
            while readable:
                try:
                    data, _ = sock.recvfrom(65535)
                except BlockingIOError:
                    break
                except OSError:
                    continue                                         # Windows 的 ConnectionResetError 之類：和我們無關，繼續
                pkt = parse_packet(data)
                if pkt is None:
                    bad += 1
                    probe(f"壞封包 {len(data)} bytes（magic/長度不對），丟掉；累計 {bad}")
                    continue
                ptype, seq, ts, ssrc, pcm = pkt
                last_rx = now
                if ptype == T_AUDIO:
                    if play_t0 is None:
                        play_t0 = now + args.buffer / 1000.0        # 第一包：buffer ms 之後開播
                        probe(f"第一包到了（seq={seq} ts={ts} ssrc=0x{ssrc:08X}），{args.buffer} ms 後開播")
                    jb.put(seq, ts, ssrc, pcm, now)
                elif jb.end_seq is None and (jb.ssrc is None or ssrc == jb.ssrc):
                    # BYE：第一個 BYE 的 seq 就是「最後一個 AUDIO 的 seq + 1」（每包 +1），所以可以知道尾巴有沒有掉包。
                    # 但 BYE 送 3 次、seq 各 +1：第一個 BYE 掉了（或被亂序扣住）就會多算 1–2 個 frame。
                    # 所以再用 ts 交叉比對：這支程式的約定是 BYE 的 ts = 真正的 sample 數（送端用 320·n 也行），
                    # ceil(ts / 320) 就是 frame 數；它和 seq 差 0、1、2 就表示「這是第 1／2／3 個 BYE」，採用 ts 算的。
                    # ts 和 seq 對不上（對方的 BYE 沒放 sample 數）就只能信 seq。
                    end = extend_seq(seq, jb.highest) if jb.highest is not None else 0
                    end_ts = (ts + FRAME_SAMPLES - 1) // FRAME_SAMPLES
                    if ts > 0 and 0 <= end - end_ts < BYE_REPEAT:
                        end = end_ts
                    if jb.highest is not None and end > jb.highest + 1 + jb.cap:
                        end = jb.highest + 1                         # seq 不合理（對方的 BYE 沒照規格給 seq）：用看過最大的 + 1
                    jb.finish_at(end)
                    bye_ts = ts
                    if play_t0 is None:                              # 還沒開播就收到 BYE（很短的檔）：馬上開播
                        play_t0 = now
                    probe(f"BYE：seq={seq} ts={ts}，結尾 = ext {jb.end_seq}，排空 buffer（{sorted(jb.buf)}）後結束")
            # 4. 逾時：送端不見了。知道的最大 seq + 1 就是結尾，排空後結束
            if last_rx is not None and jb.end_seq is None and now - last_rx >= timeout:
                print(f"{args.timeout} ms 沒有封包，當作結束", flush=True)
                if play_t0 is None:
                    play_t0 = now
                jb.finish_at(0)
                extra = jb.trim_tail()
                if extra:
                    del out[-extra * FRAME_BYTES:]
                    probe(f"逾時：最後 {extra} 個是送端不見之後補的 frame，從輸出去掉（結尾 = ext {jb.end_seq}）")
            if last_rx is None and now - t_start >= FIRST_WAIT:
                print(f"等了 {FIRST_WAIT:.0f} 秒都沒有封包，結束", flush=True)
                break
            # 5. 播放時鐘：到了拍子就播（程式停頓太久的話一次補齊好幾拍，frames_out 才會和時間一致）
            if play_t0 is not None:
                if jb.expected is None:
                    if now < play_t0:
                        continue
                    jb.start()
                    probe(f"開播：expected={jb.expected}，buf={sorted(jb.buf)}")
                while now >= play_t0 + jb.frames_out * FRAME_SEC and not jb.done():
                    out += jb.tick(now)
    except KeyboardInterrupt:
        print("Ctrl-C：把收到的寫出去", flush=True)
    sock.close()

    # 輸出。BYE 的 ts 是真正的 sample 數：只有當它落在最後一包裡（去掉的是補的 0、不是真的聲音）才採用
    total = len(out) // 2
    if bye_ts is not None and total - FRAME_SAMPLES < bye_ts <= total:
        probe(f"BYE 的 ts={bye_ts}：輸出從 {total} 截到 {bye_ts} samples（去掉最後一包補的 0）")
        out = out[:bye_ts * 2]
    write_wav_pcm(args.wav, bytes(out))
    if bad:
        print(f"丟掉了 {bad} 個壞封包（magic 或長度不對）", flush=True)
    avg = sum(jb.delays) / len(jb.delays) * 1000 if jb.delays else 0.0
    mx = max(jb.delays) * 1000 if jb.delays else 0.0
    print(f"已存檔：{args.wav}（{len(out) // 2:,} samples，{jb.frames_out} frames）", flush=True)
    print(f"STATS role=recv buffer_ms={args.buffer} plc={args.plc} frames_out={jb.frames_out} received={jb.received} lost={jb.lost()} "
          f"late={jb.late} dup={jb.dup} reordered={jb.reordered} overflow={jb.overflow} concealed={jb.concealed} "
          f"avg_delay_ms={avg:.1f} max_delay_ms={mx:.1f}", file=sys.stderr, flush=True)
    return 0


# =============================================================== 5. main
# argparse 的 subcommand：第一個字決定後面接受哪些參數。call／devices 有登記，但只印一句話、結束碼 2（live 模式是 C 的工作）。
# 結束碼：0 正常；1 網路錯誤（OSError）；2 參數或檔案格式錯誤（ValueError、argparse 的錯誤）。
def main():
    global PROBE
    ap = argparse.ArgumentParser(description="VoiceLink 純 Python 完整版（mock 模式，對照 C 版用）")
    ap.add_argument("--probe", action="store_true", help="印出每一拍與每個封包的中間過程")
    sub = ap.add_subparsers(dest="cmd", required=True)
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--probe", action="store_true", default=argparse.SUPPRESS)
    p = sub.add_parser("send", parents=[common]); p.add_argument("ip"); p.add_argument("port", type=int); p.add_argument("wav")
    p.add_argument("--burst", action="store_true", help="不等 20 ms，一口氣全送")
    p = sub.add_parser("recv", parents=[common]); p.add_argument("port", type=int); p.add_argument("wav")
    p.add_argument("--bind", default="0.0.0.0"); p.add_argument("--buffer", type=int, default=60)
    p.add_argument("--plc", choices=["zero", "repeat"], default="repeat"); p.add_argument("--timeout", type=int, default=2000)
    sub.add_parser("call", parents=[common], add_help=False)
    sub.add_parser("devices", parents=[common], add_help=False)
    args, _ = ap.parse_known_args() if sys.argv[1:2] in (["call"], ["devices"]) else (ap.parse_args(), None)
    PROBE = args.probe
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    if args.cmd in ("call", "devices"):
        print(f"{args.cmd}：live 模式（麥克風與喇叭）不在 Python 版裡，請用 C 版", file=sys.stderr)
        return 2
    if args.port not in range(1, 65536):
        print("錯誤: port 要是 1–65535 的整數", file=sys.stderr)
        return 2
    try:
        return {"send": cmd_send, "recv": cmd_recv}[args.cmd](args)
    except ValueError as why:
        print(f"錯誤: {why}", file=sys.stderr)
        return 2
    except OSError as why:
        print(f"錯誤: {why}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
