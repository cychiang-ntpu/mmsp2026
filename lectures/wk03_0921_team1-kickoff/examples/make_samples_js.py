#!/usr/bin/env python3
"""make_samples_js.py — 把 data/ 裡的真實 WAV 與 BMP 轉成 slides/samples.js，讓網頁不需要伺服器就能讀到它們的「每一個 byte」。

  python3 examples/make_samples_js.py        （從本週資料夾執行；Windows 用 python）

為什麼需要這一步：直接用瀏覽器開 .html（file://）時，<audio> 與 <img> 可以讀旁邊的檔案，但 JavaScript 不能（瀏覽器的安全限制），
所以要顯示 hex、解析檔頭、改取樣率與 bit depth 重新播放，就得把檔案內容放進一支 .js。做法是 base64：每 3 bytes 變成 4 個可列印字元。
檔案因此變大 4/3 倍——這本身就是一個「表示法不同，大小就不同」的例子。

素材與授權：
  speech  Open Speech Repository 的 Harvard sentences（OSR_us_000_0010，8 kHz／16-bit／mono），取前 4.5 秒。可自由用於測試與研究，須註明來源。
  music   John Philip Sousa〈Comrades of the Legion〉，"The President's Own" United States Marine Band 演奏，公有領域（美國聯邦政府作品）。
          取第 20–26 秒，轉成 44.1 kHz／16-bit／mono。來源：Wikimedia Commons。
  image   The Blue Marble（NASA／Apollo 17，1972），公有領域。縮成 256×256、24-bit BMP。來源：Wikimedia Commons。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第二節 2.6 的互動網頁 slides/wav_bmp_viewer.html；它內建的語音、音樂與照片就是這支程式產生的 slides/samples.js。
# 這是「老師準備教材用」的工具：上課不需要跑它，repo 裡已經有產生好的 samples.js。
#           想讀它的理由是 wav_clip()：它示範了「走 chunk 讀 WAV」加上「自己組一個 44-byte 檔頭寫 WAV」，
#           和 MP2（寫 WAV 檔頭）、Team 1（解析別人的 WAV）要做的事相同，只是這裡用 Python。
# 怎麼跑：  見上面（從哪個資料夾執行都可以，路徑是由這個檔案自己的位置算出來的）。注意：它會覆寫 slides/samples.js。
# 會看到：  每個素材一行「原始幾 bytes → base64 幾個字元（約 1.333 倍）」，最後一行是寫出的檔名與大小。
# ──────────────────────────────────────────────────────────────────────────────────────
import base64
import json
import os
import struct

# 由「這個檔案自己的位置」推出本週資料夾，這樣不管使用者站在哪個資料夾執行，都找得到 data/ 與 slides/。
#   __file__                 這支程式的路徑；os.path.abspath() 把它變成絕對路徑；
#   os.path.dirname(路徑)    去掉最後一段。做一次得到 …/examples，再做一次得到本週資料夾。
#   os.path.join(a, b, …)    用這台電腦的路徑分隔符號（/ 或 \）把各段接起來，不要自己用字串相加。
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA, OUT = os.path.join(HERE, "data"), os.path.join(HERE, "slides", "samples.js")


def wav_clip(path, seconds):
    """取 WAV 的前 seconds 秒，重新組一個標準的 44-byte 檔頭（只處理 PCM）。"""
    # 語音檔原本有 33.6 秒、五十幾萬 bytes，整個塞進網頁太大，所以只留前幾秒。
    # WAV 不能直接「砍掉後半段」：檔頭裡有兩個大小欄位（講義 2.6 的 ChunkSize 與 Subchunk2Size），
    # 資料變短了，這兩個欄位要跟著改，否則播放器會讀到檔案外面去。所以做法是：拆開 → 截短 → 重新組裝。

    # "rb" = 以 binary 模式讀，得到 bytes（一串 0–255 的位元組）。
    d = open(path, "rb").read()

    # ── 拆開：和 wav_info.py 同樣的走法，從第 12 byte 起一個 chunk 一個 chunk 往後跳 ──
    # p = 目前這個 chunk 的開頭位置；b"" 是空的 bytes。
    p, fmt, body = 12, None, b""
    while p + 8 <= len(d):
        # 每個 chunk 的開頭：4 bytes 的 id ＋ 4 bytes 的大小。d[p:p + 4] 是 slicing（不含結尾的 p + 4）。
        # struct.unpack("<I", 4 個 bytes)："<" = little-endian（低位 byte 在前）、"I" = 無號 32-bit 整數；
        # 回傳的是 tuple，所以用 [0] 取出裡面唯一的那個數字。
        cid, size = d[p:p + 4], struct.unpack("<I", d[p + 4:p + 8])[0]
        if cid == b"fmt ":
            # fmt chunk 的 16 bytes「原封不動」留下來：取樣率、bit depth 都沒變，待會直接貼回新的檔頭。
            fmt = d[p + 8:p + 24]
        elif cid == b"data":
            body = d[p + 8:p + 8 + size]
        # 跳到下一個 chunk：8 bytes 的開頭＋內容；內容長度是奇數時檔案裡多補了 1 byte（size & 1：奇數得 1、偶數得 0）。
        # LIST 之類用不到的 chunk 就這樣跳過、丟掉。
        p += 8 + size + (size & 1)

    # ── 截短 ──
    # "<HHIIHH" 依序解出 fmt 的六個欄位：H = 無號 16-bit（2 bytes）、I = 無號 32-bit（4 bytes），共 16 bytes：
    #   AudioFormat、NumChannels、SampleRate、ByteRate、BlockAlign、BitsPerSample。
    # 用不到的欄位照慣例用底線 _ 接住；這裡真正用到的只有 rate（取樣率）與 align（BlockAlign）。
    _, ch, rate, _, align, _ = struct.unpack("<HHIIHH", fmt)
    # seconds 秒 = seconds × rate 個時間點；每個時間點佔 align bytes（所有聲道合計）。
    # 先 int() 取整數個時間點、再乘 align，切的位置才會落在 sample 的邊界上，不會把一個 16-bit sample 切成兩半。
    # body[:n] 是取前 n 個 bytes；n 比實際長度大也沒關係，slicing 會自動停在結尾。
    body = body[:int(seconds * rate) * align]

    # ── 重新組裝：照講義 2.6 的表，一欄一欄接起來，剛好 44 bytes 的檔頭＋資料 ──
    #   b"RIFF"                         位移 0
    #   struct.pack("<I", 36 + 長度)    位移 4：ChunkSize = 檔案大小 − 8。檔頭 44 bytes，扣掉前 8 bytes 剩 36，再加資料長度
    #   b"WAVEfmt "                     位移 8、12（"fmt " 結尾有一個空白）
    #   struct.pack("<I", 16)           位移 16：fmt chunk 的內容有 16 bytes
    #   fmt                             位移 20–35：原檔的 16 bytes
    #   b"data" + struct.pack(…)        位移 36、40：Subchunk2Size = PCM 資料的 bytes 數
    #   body                            位移 44 起：PCM sample
    # struct.pack 是 unpack 的反向：整數 → bytes。bytes 之間用 + 接起來。
    return b"RIFF" + struct.pack("<I", 36 + len(body)) + b"WAVEfmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(body)) + body


# ── 收集三個素材 ──
# items 是一個 dict：key 是素材的代號，value 又是一個 dict（名稱、種類、原始 bytes、出處說明）。
# 每個檔案都先用 os.path.exists 檢查：少了哪個檔就略過哪個，程式不會因此當掉。
items = {}
src = os.path.join(DATA, "speech_osr_8k.wav")
if os.path.exists(src):
    # 語音只取前 4.5 秒；音樂與照片本來就不大，整個檔案原樣讀進來。
    items["speech"] = {"name": "speech_osr_8k.wav（前 4.5 秒）", "kind": "wav", "bytes": wav_clip(src, 4.5),
                       "credit": "Open Speech Repository, Harvard sentences；8 kHz、16-bit、單聲道"}
src = os.path.join(DATA, "music_sousa_44k.wav")
if os.path.exists(src):
    items["music"] = {"name": "music_sousa_44k.wav", "kind": "wav", "bytes": open(src, "rb").read(),
                      "credit": "Sousa〈Comrades of the Legion〉，United States Marine Band，公有領域；44.1 kHz、16-bit、單聲道"}
src = os.path.join(DATA, "earth_256.bmp")
if os.path.exists(src):
    items["image"] = {"name": "earth_256.bmp", "kind": "bmp", "bytes": open(src, "rb").read(),
                      "credit": "The Blue Marble，NASA／Apollo 17（1972），公有領域；256×256、24-bit"}

# ── 產生 JavaScript 原始碼：先把每一行收進 list，最後一次寫出 ──
# 產生的檔案長這樣： window.SAMPLES = { speech: { name: "…", kind: "wav", …, b64: "UklGR…" }, … };
# 網頁用 <script src="samples.js"> 載入後，就能從 window.SAMPLES 拿到資料，再把 base64 解回原本的 bytes。
lines = ["// 由 examples/make_samples_js.py 產生，請勿手改。內容是 data/ 裡真實檔案的 base64。", "window.SAMPLES = {"]
# items.items() 走訪 dict 的每一對（key, value）。
for key, it in items.items():
    # base64：原始的 bytes 裡什麼值都有（包括引號、換行、不是文字的 byte），不能直接寫進 .js 的字串裡；
    # base64 每次取 3 bytes（24 bits），切成 4 個 6-bit 的數字，每個對應到 A–Z a–z 0–9 + / 這 64 個安全字元之一。
    # 代價是 3 bytes 變 4 個字元，大了 1/3。b64encode 回傳的是 bytes，.decode("ascii") 把它轉成 str 才能放進 f-string。
    b64 = base64.b64encode(it["bytes"]).decode("ascii")
    # json.dumps(字串) 會產生「加好雙引號、特殊字元也跳脫好」的寫法，剛好也是合法的 JavaScript 字串；
    #   ensure_ascii=False 讓中文照原樣輸出，不要變成 \uXXXX。
    # f-string 裡要印出「大括號本身」得寫兩個：{{ 印成 {、}} 印成 }。
    # 兩個相鄰的字串常值會自動接成一個，所以這個長字串可以拆成兩行寫。
    lines.append(f"  {key}: {{ name: {json.dumps(it['name'], ensure_ascii=False)}, kind: {json.dumps(it['kind'])}, "
                 f"credit: {json.dumps(it['credit'], ensure_ascii=False)}, size: {len(it['bytes'])}, b64: \"{b64}\" }},")
    # 格式規格：{key:7s} 寬 7 的字串；{…:>9,d} 寬 9、靠右、加千分位的整數；{…:.3f} 小數 3 位。
    # 倍數會是 1.333 或再多一點點：長度不是 3 的倍數時，base64 會在結尾用 = 補滿 4 個字元。
    print(f"{key:7s} {len(it['bytes']):>9,d} bytes → base64 {len(b64):>9,d} 字元（{len(b64) / len(it['bytes']):.3f} 倍）")
lines.append("};")
# with 區塊結束時自動關檔。這次寫的是文字檔（"w"），兩個參數是為了讓產生的檔案在每台電腦上都一模一樣：
#   encoding="utf-8"  明確指定編碼，不用作業系統的預設值（Windows 的預設常常不是 UTF-8）；
#   newline="\n"      換行一律寫成 LF，不讓 Windows 自動改成 CRLF，否則 git 會以為檔案被改過。
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    # "\n".join(lines)：用換行把每一行接起來，最後再補一個換行。
    f.write("\n".join(lines) + "\n")
# os.path.relpath(OUT, HERE)：把絕對路徑改寫成相對於本週資料夾的寫法（slides/samples.js；Windows 上分隔符號會是反斜線），印出來比較短。
print(f"寫出 {os.path.relpath(OUT, HERE)}（{os.path.getsize(OUT):,} bytes）")
