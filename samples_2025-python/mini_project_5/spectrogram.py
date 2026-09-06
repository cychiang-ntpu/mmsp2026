#!/usr/bin/env python3
"""MP5 參考實作（Python）：測試訊號產生與 STFT spectrogram（使用 numpy）。

用法：
  python3 spectrogram.py gen fs out.wav
  python3 spectrogram.py spec w_ms {hamming,rectangular} dft_ms hop_ms in.wav out.txt
"""
import argparse
import wave

import numpy as np

# 測試訊號：四段波形 × 每段 10 個 0.1 秒的音，各自的振幅與頻率
AMPS = [100, 2000, 1000, 500, 250, 100, 2000, 1000, 500, 250]
FREQS = [0, 31.25, 500, 2000, 4000, 44, 220, 440, 1760, 3960]


def tone(kind: int, t: np.ndarray, f: float) -> np.ndarray:
    if kind == 0:
        return np.sin(2 * np.pi * f * t)
    if kind == 1:
        return f * t - np.floor(f * t)
    if kind == 2:
        return np.sign(np.sin(2 * np.pi * f * t) + 1e-300)
    return 2 * np.abs(2 * (f * t - np.floor(f * t + 0.5))) - 1


def gen(args):
    fs = args.fs
    t = np.arange(4 * fs) / fs
    x = np.zeros_like(t)
    for kind in range(4):
        for i, (a, f) in enumerate(zip(AMPS, FREQS)):
            seg = (t >= kind + 0.1 * i) & (t < kind + 0.1 * (i + 1))
            x[seg] += a * tone(kind, t[seg] - (kind + 0.1 * i), f)
    pcm = np.clip(x, -32768, 32767).astype(np.int16)
    with wave.open(args.out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(fs)
        w.writeframes(pcm.tobytes())


def spec(args):
    with wave.open(args.wav, "rb") as w:
        fs = w.getframerate()
        pcm = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float)

    win_len = fs * args.w_ms // 1000
    dft_len = fs * args.dft_ms // 1000
    hop = fs * args.hop_ms // 1000
    window = np.hamming(win_len) if args.window == "hamming" else np.ones(win_len)

    rows = []
    for start in range(0, len(pcm) - win_len + 1, hop):
        frame = pcm[start:start + win_len] * window
        spectrum = np.fft.rfft(frame, n=dft_len)          # zero padding 由 n 參數處理
        rows.append(np.abs(spectrum)[: dft_len // 2])
    np.savetxt(args.out, np.array(rows), fmt="%f", delimiter=" ")


def main():
    ap = argparse.ArgumentParser(description="signal generator / STFT spectrogram")
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("gen"); g.add_argument("fs", type=int); g.add_argument("out"); g.set_defaults(fn=gen)
    s = sub.add_parser("spec")
    s.add_argument("w_ms", type=int); s.add_argument("window", choices=["hamming", "rectangular"])
    s.add_argument("dft_ms", type=int); s.add_argument("hop_ms", type=int)
    s.add_argument("wav"); s.add_argument("out"); s.set_defaults(fn=spec)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
