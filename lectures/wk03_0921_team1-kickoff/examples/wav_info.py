#!/usr/bin/env python3
"""wav_info.py — 把 WAV 檔拆開來看：RIFF 的每個 chunk、fmt 的每個欄位、sample 值的 histogram 與熵。
只用標準函式庫（刻意不用 wave 模組，自己走 chunk，和你們在 C 裡要做的事一樣）。

用法：python3 wav_info.py <檔案.wav>

WAV = RIFF 容器：12 bytes 的 "RIFF" <大小> "WAVE"，後面接一串 chunk；
每個 chunk = 4 bytes 的 id + 4 bytes 的大小（little-endian）+ 內容，內容長度是奇數時補 1 byte。
"fmt " 描述格式、"data" 放 PCM sample。data 不一定緊接在第 44 byte：中間可能有 "LIST" 等 chunk。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第二節 2.6「WAV 檔：把 PCM 包起來」（檔頭逐欄解讀）與 2.7（sample 的 histogram 與 Huffman）。
# 怎麼跑：  python3 wav_info.py ../data/speech_osr_8k.wav     （Windows 打 python，路徑用 ..\data\…）
# 會看到：  (1) 檔案大小與 RIFF 大小欄位；(2) 每個 chunk 的位移、id、大小；(3) fmt chunk 的六個欄位、
#           data 區的位置與秒數、資料率；(4) 只有 16-bit PCM 才有：sample 的最小／最大值、
#           以 sample 為符號與以 byte 為符號的熵；(5) 16 格的 histogram（語音會集中在 0 附近的兩格）。
#           對本週的語音檔，數字應該和講義 2.6、2.7 裡列的一樣（538,014 bytes、H = 11.763 與 6.653）。
# 和 Team 1 的關係：功能 3（WAV 檔）要在 C 裡做同樣的事——走 chunk 找到 data 區、把 sample 讀出來統計。
# ──────────────────────────────────────────────────────────────────────────────────────
import collections
import math
import struct
import sys


def main():
    # 參數個數不對就印用法並結束。sys.exit(字串) 會把字串印到 stderr，並讓程式的結束碼不是 0（表示失敗）。
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} <file.wav>")

    # 一次把整個檔案讀進記憶體。"rb" = 以 binary 模式讀：得到的 d 是 bytes（一串 0–255 的位元組），不是文字 str。
    # 用文字模式開的話，Python 會想把內容當文字解碼，WAV 的內容不是文字，會出錯。
    d = open(sys.argv[1], "rb").read()

    # 先確認這真的是 WAV：第 0–3 byte 要是 "RIFF"、第 8–11 byte 要是 "WAVE"（講義 2.6 的表）。
    #   d[:4]    slicing（切片）：取索引 0、1、2、3，「不含」結尾的 4；d[8:12] 取索引 8 到 11。
    #   b"RIFF"  前面的 b 表示這是 bytes 常值，才能和 bytes 比較；bytes 和 str 比永遠不相等。
    # len(d) < 12 要先檢查：檔案太短的話，後面的比較沒有意義。
    if len(d) < 12 or d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        sys.exit("不是 RIFF/WAVE 檔")

    # struct.unpack(格式, bytes) 把一段 bytes「解讀」成數字，回傳 tuple（就算只有一個值也是），所以後面接 [0]。
    #   '<I'： "<" = little-endian（低位的 byte 在前，WAV 裡所有數字都是這樣）；"I" = 無號 32-bit 整數，佔 4 bytes。
    #   例：96 35 08 00 → 0x00083596 = 538,006。在 C 裡要用位移自己組：b0 | b1<<8 | b2<<16 | b3<<24。
    # {len(d):,} 的逗號是格式規格：印整數時加千分位。
    print(f"{sys.argv[1]}：{len(d):,} bytes；RIFF 大小欄位 = {struct.unpack('<I', d[4:8])[0]:,}（= 檔案大小 − 8）\n")

    # ── 走過每一個 chunk ──
    # 為什麼不能直接假設「data 從第 44 byte 開始」：fmt 與 data 之間可能夾著 LIST 等 chunk（講義 2.6 第 3 點）。
    # 正確的做法是從第 12 byte 起，一個 chunk 一個 chunk 往後跳，看到 id 是 "fmt " 或 "data" 才處理。
    # p 是「目前這個 chunk 的開頭在檔案的第幾個 byte」。一行指定四個變數：左邊四個名字依序對應右邊四個值。
    fmt, data_off, data_len, p = None, None, 0, 12
    print("  位移      chunk   大小")
    # 每個 chunk 的開頭固定是 8 bytes（4 bytes 的 id + 4 bytes 的大小）；剩下不到 8 bytes 就表示走完了。
    while p + 8 <= len(d):
        cid, size = d[p:p + 4], struct.unpack("<I", d[p + 4:p + 8])[0]
        # cid 是 bytes，要印出來得先變成 str。用 latin-1 解碼是因為它把 0–255 每個 byte 都對應到一個字元，
        # 不管 id 裡是什麼怪 byte 都不會解碼失敗。!r 表示印 repr()——帶引號的寫法，'fmt ' 結尾那個空白才看得見。
        # :8s 是寬度 8 的字串；{size:>10,d} 是寬 10、靠右、加千分位的整數。
        print(f"  {p:>6d}    {cid.decode('latin-1')!r:8s}{size:>10,d}")
        if cid == b"fmt ":
            # fmt chunk 的內容從 p + 8 開始，PCM 的標準欄位共 16 bytes（到 p + 24 之前）。
            # 格式字串 "<HHIIHH"：little-endian，H = 無號 16-bit（2 bytes）、I = 無號 32-bit（4 bytes），
            # 依序就是 AudioFormat(2) NumChannels(2) SampleRate(4) ByteRate(4) BlockAlign(2) BitsPerSample(2)，
            # 2+2+4+4+2+2 = 16 bytes，一次解成 6 個數字的 tuple。
            fmt = struct.unpack("<HHIIHH", d[p + 8:p + 24])
        elif cid == b"data":
            # 只記下 data 區的「位置」與「長度」，先不讀內容。
            # 大小欄位是檔案自己說的，不一定可信（檔案可能被截斷），所以和「檔案實際剩下的長度」取較小的。
            data_off, data_len = p + 8, min(size, len(d) - (p + 8))
        # 跳到下一個 chunk：8 bytes 的開頭 + 內容 size bytes；內容長度是奇數時檔案裡還多補了 1 byte。
        # size & 1 是取最低位的 bit：奇數得 1、偶數得 0，剛好就是要多跳的 byte 數。
        p += 8 + size + (size & 1)

    # 「is None」用來判斷變數是不是還停在初始值 None，也就是迴圈裡從來沒遇到那個 chunk。
    if fmt is None or data_off is None:
        sys.exit("找不到 fmt 或 data chunk")

    # tuple unpacking：把 6 個數字的 tuple 一次拆給 6 個變數。
    tag, ch, rate, byte_rate, align, bits = fmt
    # BlockAlign = 一個時間點所有聲道加起來幾 bytes，所以「data 的 bytes 數 ÷ BlockAlign」= 有幾個時間點。
    # // 是整數除法；後面的 if align else 0 是防止壞掉的檔頭裡 align = 0 造成除以零。
    n_frames = data_len // align if align else 0

    # 三個引號的字串可以跨很多行；前面加 f 一樣是 f-string，大括號裡可以放運算式。
    # 秒數 = 時間點個數 ÷ 取樣率；資料率 = 取樣率 × bit depth × 聲道數（講義 1.1、2.5 的公式）。
    print(f"""
  fmt chunk（16 bytes，全部 little-endian）
    AudioFormat    {tag}   （1 = PCM，未壓縮）
    NumChannels    {ch}
    SampleRate     {rate} Hz          ← 取樣率 fs
    ByteRate       {byte_rate} bytes/s    = SampleRate × BlockAlign
    BlockAlign     {align} bytes          = NumChannels × BitsPerSample ÷ 8（一個時間點的所有聲道）
    BitsPerSample  {bits}                ← bit depth
  data 從第 {data_off} byte 開始，共 {data_len:,} bytes = {n_frames:,} 個時間點 = {n_frames / rate if rate else 0:.3f} 秒
  資料率 = {rate} × {bits} × {ch} = {rate * bits * ch:,} bits/s = {rate * bits * ch / 1000:.1f} kb/s""")

    # ── 以下把 sample 讀出來做統計，只處理 16-bit PCM ──
    # 8-bit 的 WAV 是無號數、24／32-bit 要用別的格式字元，這支示範程式不處理（講義 2.6 第 2 點）。
    if tag != 1 or bits != 16:
        print("\n  不是 16-bit PCM，以下的 sample 統計略過。")
        return

    # 切出 data 區。data_len // 2 * 2 是把長度「向下取到偶數」：每個 sample 2 bytes，萬一多出 1 個落單的 byte 就丟掉，
    # 否則下一行的 unpack 會因為長度對不上而丟例外。
    body = d[data_off:data_off + data_len // 2 * 2]
    # 格式字串可以在字母前面寫「個數」：例如 "<3h" = 3 個 little-endian、有號的 16-bit 整數。
    # 這裡用 f-string 把個數（bytes 數 ÷ 2）填進去，一次把整段 bytes 解成一個很長的 tuple s。
    #   h（小寫）= 有號：16-bit PCM 的 sample 是二補數，範圍 −32768 到 32767，例如 69 FC → 0xFC69 → −919。
    #   如果誤用大寫 H（無號），−919 會被讀成 64617，波形就全錯了。
    # 雙聲道時 s 裡是 L R L R … 交錯，這裡不分聲道，一起統計。
    s = struct.unpack(f"<{len(body) // 2}h", body)
    # 空的 tuple 在 if 裡算 False：data 區是空的就沒有東西可以統計（也避免後面除以 0）。
    if not s:
        return

    # 函式裡面還可以再定義函式；只在這裡用得到，就寫在這裡。
    # 熵 H = Σ p·log2(1/p) = −Σ p·log2(p)（講義 1.3）。counter.values() 是每種符號出現的次數 c，p = c / n。
    # sum( … for c in … ) 是 generator expression：不另外建 list，一邊算一邊加。
    def entropy(counter, n):
        return -sum(c / n * math.log2(c / n) for c in counter.values())

    # collections.Counter(一串東西) 會數「每一種值各出現幾次」，結果像 dict：{值: 次數}。
    # 同一段資料用兩種「符號的定義」各算一次熵（講義 1.7、2.7 的重點）：
    #   Counter(s)     以 16-bit sample 為符號：走訪 tuple s，每個元素是一個 sample 值；
    #   Counter(body)  以 byte 為符號：走訪 bytes 時，每個元素是一個 0–255 的整數。
    hs, hb = entropy(collections.Counter(s), len(s)), entropy(collections.Counter(body), len(body))
    # set(s) 把重複的值去掉，len() 就是「用到了幾種不同的 sample 值」= 符號種類數 K = codebook 要記的項數。
    k = len(set(s))

    # 理論壓縮率 = 熵 ÷ 原本每個符號用的 bits 數（sample 是 16、byte 是 8）；越小越好。
    # {hs / 16:.1%} 的 % 格式會自動乘 100 並加上百分比符號，.1 是小數 1 位。
    # 這個數字「還沒算 codebook」：以 sample 為符號雖然熵比較低，但 codebook 大得多。
    print(f"""
  sample 值：最小 {min(s)}、最大 {max(s)}（16-bit 的範圍是 −32768 到 32767）；用掉了 {k:,} 種不同的值（最多 65,536 種）
  以 sample 為符號：H = {hs:.3f} bits／sample → 理論壓縮率 {hs / 16:.1%}；但 codebook 要記 {k:,} 種符號
  以 byte   為符號：H = {hb:.3f} bits／byte   → 理論壓縮率 {hb / 8:.1%}
""")

    # ── histogram：把 −32768..32767 平分成 16 格（每格寬 4096），數每一格有幾個 sample ──
    # v + 32768 先把範圍平移成 0..65535，再 // 4096 就得到 0..15 的格子編號；外面的 min(15, …) 只是保險。
    # Counter 直接吃一個 generator expression：數「每個格子編號」各出現幾次。
    bins = collections.Counter(min(15, (v + 32768) // 4096) for v in s)
    # 最多的那一格畫滿 50 個 #，其他格照比例縮短。
    top = max(bins.values())
    print("  sample 值的 histogram（分成 16 格，每格寬 4096）")
    for b in range(16):
        lo = b * 4096 - 32768
        # bins.get(b, 0)：那一格沒有任何 sample 時 Counter 裡沒有這個 key，get 的第二個參數是「找不到時的預設值」。
        # '#' * 個數：字串乘整數 = 重複那麼多次。:50s 把字串補空白到寬度 50，右邊的數字才會對齊。
        print(f"  {lo:>7d} ~ {lo + 4095:>6d} | {'#' * round(50 * bins.get(b, 0) / top):50s} {bins.get(b, 0):>9,d}")


# 直接執行這個檔案時 __name__ 會是 "__main__"，才呼叫 main()；
# 如果是被別的程式 import 進去，就不會自動執行。這是 Python 程式的慣用結尾。
if __name__ == "__main__":
    main()
