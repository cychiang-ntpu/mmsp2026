#!/usr/bin/env python3
"""MP2 參考實作（Python）：產生波形寫成 WAV，並計算量化 SQNR。

用法：python3 wavegen.py fs bits channels {sine,square,sawtooth,triangle} freq amp seconds out.wav
SQNR（dB）印在螢幕上。需要 numpy。
"""
import argparse
import wave

import numpy as np


def waveform(kind: str, t: np.ndarray, freq: float, amp: float) -> np.ndarray:
    phase = t * freq - np.floor(t * freq)          # 相位（0~1）
    if kind == "sine":
        return amp * np.sin(2 * np.pi * freq * t)
    if kind == "square":
        return amp * np.where(np.sin(2 * np.pi * freq * t) >= 0, 1.0, -1.0)
    if kind == "sawtooth":
        return amp * (2 * phase - 1)
    if kind == "triangle":
        return amp * (4 * np.abs(phase - 0.5) - 1)
    raise ValueError(kind)


def main():
    ap = argparse.ArgumentParser(description="waveform -> WAV + SQNR")
    ap.add_argument("fs", type=int)
    ap.add_argument("bits", type=int, choices=[8, 16, 32])
    ap.add_argument("channels", type=int, choices=[1, 2])
    ap.add_argument("kind", choices=["sine", "square", "sawtooth", "triangle"])
    ap.add_argument("freq", type=float)
    ap.add_argument("amp", type=float)
    ap.add_argument("seconds", type=float)
    ap.add_argument("out")
    args = ap.parse_args()
    if not 0.0 <= args.amp <= 1.0:
        ap.error("amp must be 0.0-1.0")
    if 2 * args.freq >= args.fs:
        ap.error("freq must be below Nyquist (fs/2)")

    n = round(args.seconds * args.fs)
    t = np.arange(n) / args.fs
    x = np.clip(waveform(args.kind, t, args.freq, args.amp), -1.0, 1.0)

    # 量化到整數格線，再還原回 [-1,1] 得到量化後訊號
    full_scale = {8: 127, 16: 32767, 32: 2147483647}[args.bits]
    q = np.rint(x * full_scale)
    xq = q / full_scale

    # SQNR = 10 log10(訊號功率 / 量化誤差功率)
    err = xq - x
    sqnr = 10 * np.log10(np.mean(x ** 2) / np.mean(err ** 2))
    print(f"SQNR = {sqnr:.6f} dB")

    dtype = {8: np.uint8, 16: np.int16, 32: np.int32}[args.bits]
    pcm = (q + 128 if args.bits == 8 else q).astype(dtype)
    frames = np.repeat(pcm, args.channels)         # 雙聲道 = 同值複製
    with wave.open(args.out, "wb") as w:
        w.setnchannels(args.channels)
        w.setsampwidth(args.bits // 8)
        w.setframerate(args.fs)
        w.writeframes(frames.tobytes())


if __name__ == "__main__":
    main()
