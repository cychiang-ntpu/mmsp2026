#!/usr/bin/env python3
"""產生試玩 TextLink 用的檔案，放在 samples/（不進版控）。只用標準函式庫。

  python3 tests/make_samples.py        （Windows：python tests\\make_samples.py）

產生：
  samples/zh_text.txt   中文為主的 UTF-8 文字，含 1–4 bytes 字元、BOM、CRLF，約 1.2 MB
  samples/en_text.txt   英文為主的文字，約 1.2 MB
  samples/tone.wav      16 kHz、16-bit、單聲道的合成音 40 秒，約 1.2 MB

這些只是讓你馬上有東西可以傳。正式報告的測試檔請自己準備：真的文章、真的語音或音樂，
合成資料的統計特性和真實資料差很多，壓縮率不具代表性。
"""
import math
import os
import random
import struct
import wave

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "samples")
os.makedirs(OUT, exist_ok=True)
rng = random.Random(2026)

ZH = ("多媒體訊號處理把系統做出來文字聲音影像表示壓縮傳輸整合熵編碼霍夫曼長度前綴封包半包黏包"
      "位元組串流取樣量化頻譜離散餘弦轉換即時通訊網路延遲抖動緩衝遺失補償學為所用")
EXTRA = ["é", "Ω", "я", "😀", "👨‍👩‍👧", "𠮷", "：", "，", "。", "\t", '"']
with open(os.path.join(OUT, "zh_text.txt"), "wb") as f:
    f.write(b"\xef\xbb\xbf")                                   # BOM：還原後也要在
    while f.tell() < 1_200_000:
        n = rng.randint(8, 40)
        line = "".join(rng.choice(ZH) if rng.random() < 0.93 else rng.choice(EXTRA) for _ in range(n))
        f.write((line + "\r\n").encode("utf-8"))               # CRLF：一個 byte 都不能改
    f.write("檔尾沒有換行".encode("utf-8"))

WORDS = ("the of and to in is that for it as with signal system entropy huffman code frame packet "
         "length prefix stream byte audio image video sample quantize spectrum transform network "
         "latency jitter buffer loss sender receiver protocol compress decode encode").split()
with open(os.path.join(OUT, "en_text.txt"), "w", encoding="utf-8", newline="\n") as f:
    size = 0
    while size < 1_200_000:
        s = " ".join(rng.choice(WORDS) for _ in range(rng.randint(6, 18))).capitalize() + ".\n"
        f.write(s)
        size += len(s)

with wave.open(os.path.join(OUT, "tone.wav"), "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(16000)
    frames = bytearray()
    for t in range(16000 * 40):
        env = 0.5 + 0.5 * math.sin(2 * math.pi * 0.25 * t / 16000)      # 緩慢起伏的音量
        v = env * (9000 * math.sin(2 * math.pi * 440 * t / 16000) + 4000 * math.sin(2 * math.pi * 1237 * t / 16000))
        frames += struct.pack("<h", int(v + rng.gauss(0, 150)))
    w.writeframes(bytes(frames))

for name in sorted(os.listdir(OUT)):
    print(f"samples/{name:14s} {os.path.getsize(os.path.join(OUT, name)):>9,d} bytes")
