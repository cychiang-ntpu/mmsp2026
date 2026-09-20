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
import base64
import json
import os
import struct

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA, OUT = os.path.join(HERE, "data"), os.path.join(HERE, "slides", "samples.js")


def wav_clip(path, seconds):
    """取 WAV 的前 seconds 秒，重新組一個標準的 44-byte 檔頭（只處理 PCM）。"""
    d = open(path, "rb").read()
    p, fmt, body = 12, None, b""
    while p + 8 <= len(d):
        cid, size = d[p:p + 4], struct.unpack("<I", d[p + 4:p + 8])[0]
        if cid == b"fmt ":
            fmt = d[p + 8:p + 24]
        elif cid == b"data":
            body = d[p + 8:p + 8 + size]
        p += 8 + size + (size & 1)
    _, ch, rate, _, align, _ = struct.unpack("<HHIIHH", fmt)
    body = body[:int(seconds * rate) * align]
    return b"RIFF" + struct.pack("<I", 36 + len(body)) + b"WAVEfmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(body)) + body


items = {}
src = os.path.join(DATA, "speech_osr_8k.wav")
if os.path.exists(src):
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

lines = ["// 由 examples/make_samples_js.py 產生，請勿手改。內容是 data/ 裡真實檔案的 base64。", "window.SAMPLES = {"]
for key, it in items.items():
    b64 = base64.b64encode(it["bytes"]).decode("ascii")
    lines.append(f"  {key}: {{ name: {json.dumps(it['name'], ensure_ascii=False)}, kind: {json.dumps(it['kind'])}, "
                 f"credit: {json.dumps(it['credit'], ensure_ascii=False)}, size: {len(it['bytes'])}, b64: \"{b64}\" }},")
    print(f"{key:7s} {len(it['bytes']):>9,d} bytes → base64 {len(b64):>9,d} 字元（{len(b64) / len(it['bytes']):.3f} 倍）")
lines.append("};")
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
print(f"寫出 {os.path.relpath(OUT, HERE)}（{os.path.getsize(OUT):,} bytes）")
