#!/usr/bin/env python3
"""make_samples_js.py — 把 data/aeueo-16kHz.wav 轉成 slides/samples.js，讓 stft_steps.html 不需要伺服器就能讀到它的每一個 sample。

  python3 examples/make_samples_js.py        （從本週資料夾執行；Windows 用 python）

為什麼需要這一步：直接用瀏覽器開 .html（file://）時 JavaScript 不能讀旁邊的檔案（瀏覽器的安全限制），
所以要在網頁裡畫波形、切 frame、算 FFT，就得把檔案內容放進一支 .js。做法和第 3 週一樣：base64，每 3 bytes 變 4 個可列印字元。

素材：aeueo-16kHz.wav——老師自己錄的五個母音「a、e、u、e、o」，16 kHz／16-bit／單聲道，約 7 秒。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：第 7 週 STFT 的互動網頁 slides/stft_steps.html；它內建的母音錄音就是這支程式產生的 slides/samples.js。
# 這是「老師準備教材用」的工具：上課不需要跑它，repo 裡已經有產生好的 samples.js。注意：它會覆寫 slides/samples.js。
# 和第 3 週的版本相比，這裡不截短檔案（7 秒、225 KB，base64 之後約 300 KB，網頁還吃得下），
# 但一樣「照 chunk 走」把 fmt 與 data 找出來、重組成標準 44-byte 檔頭的 WAV，順便把取樣率、sample 數寫進 samples.js，網頁就不必再解析檔頭。
# ──────────────────────────────────────────────────────────────────────────────────────
import base64
import json
import os
import struct

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA, OUT = os.path.join(HERE, "data"), os.path.join(HERE, "slides", "samples.js")


def wav_canonical(path, max_seconds=None):
    """照 chunk 走，取出 fmt 與 data，重新組一個標準的 44-byte 檔頭（只處理 PCM）。回傳 (bytes, 取樣率, 聲道數, bits, sample 數)。"""
    d = open(path, "rb").read()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise SystemExit(f"{path} 不是 WAV")
    p, fmt, body = 12, None, b""
    while p + 8 <= len(d):
        cid, size = d[p:p + 4], struct.unpack("<I", d[p + 4:p + 8])[0]
        if cid == b"fmt ":
            fmt = d[p + 8:p + 24]
        elif cid == b"data":
            body = d[p + 8:p + 8 + size]
        p += 8 + size + (size & 1)
    audio_fmt, ch, rate, _, align, bits = struct.unpack("<HHIIHH", fmt)
    if audio_fmt != 1:
        raise SystemExit("只處理 PCM")
    if max_seconds is not None:
        body = body[:int(max_seconds * rate) * align]
    body = body[:len(body) // align * align]
    out = b"RIFF" + struct.pack("<I", 36 + len(body)) + b"WAVEfmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(body)) + body
    return out, rate, ch, bits, len(body) // align


items = {}
src = os.path.join(DATA, "aeueo-16kHz.wav")
if os.path.exists(src):
    raw, rate, ch, bits, n = wav_canonical(src)
    items["vowel"] = {"name": "aeueo-16kHz.wav", "kind": "wav", "bytes": raw, "rate": rate, "channels": ch, "bits": bits, "samples": n,
                      "credit": "母音 a、e、u、e、o；16 kHz、16-bit、單聲道，約 %.1f 秒" % (n / rate)}
else:
    raise SystemExit(f"找不到 {src}")

lines = ["// 由 examples/make_samples_js.py 產生，請勿手改。內容是 data/ 裡真實檔案的 base64。", "window.SAMPLES = {"]
for key, it in items.items():
    b64 = base64.b64encode(it["bytes"]).decode("ascii")
    lines.append(f"  {key}: {{ name: {json.dumps(it['name'], ensure_ascii=False)}, kind: {json.dumps(it['kind'])}, "
                 f"credit: {json.dumps(it['credit'], ensure_ascii=False)}, rate: {it['rate']}, channels: {it['channels']}, bits: {it['bits']}, "
                 f"samples: {it['samples']}, size: {len(it['bytes'])}, b64: \"{b64}\" }},")
    print(f"{key:7s} {len(it['bytes']):>9,d} bytes → base64 {len(b64):>9,d} 字元（{len(b64) / len(it['bytes']):.3f} 倍）")
lines.append("};")
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
print(f"寫出 {os.path.relpath(OUT, HERE)}（{os.path.getsize(OUT):,} bytes）")
