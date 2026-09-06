#!/usr/bin/env python3
import matplotlib
matplotlib.use("Agg")   # ←←← 關鍵中的關鍵（強制無頭 backend）

import sys
import numpy as np
import wave
import matplotlib.pyplot as plt



def read_wav_mono(path: str):
    """Read 16-bit PCM wav. Return (signal_float, fs). If stereo, take left channel."""
    with wave.open(path, "rb") as wf:
        nch = wf.getnchannels()
        fs = wf.getframerate()
        sampwidth = wf.getsampwidth()
        nframes = wf.getnframes()
        raw = wf.readframes(nframes)

    if sampwidth != 2:
        raise ValueError(f"Only supports 16-bit PCM wav. Got sampwidth={sampwidth}")

    x = np.frombuffer(raw, dtype=np.int16)

    if nch == 2:
        x = x[0::2]  # left channel
    elif nch != 1:
        raise ValueError(f"Only supports mono/stereo. Got nchannels={nch}")

    # normalize to [-1,1] float for plotting
    x = x.astype(np.float64) / 32768.0
    return x, fs


def read_spec_txt(path: str):
    """Read spectrogram matrix from txt.
    - supports pure numeric rows
    - also skips comment lines starting with '#'
    """
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                continue
            parts = line.split()
            try:
                rows.append([float(p) for p in parts])
            except ValueError:
                # if any weird line exists, skip it
                continue

    if not rows:
        raise ValueError(f"No numeric spectrogram data found in {path}")

    spec = np.array(rows, dtype=np.float64)
    return spec  # shape: (frames, bins)


def main():
    if len(sys.argv) != 4:
        print("Usage: spectshow.py wav_in spec_txt pdf_out", file=sys.stderr)
        sys.exit(1)

    wav_in, spec_txt, pdf_out = sys.argv[1], sys.argv[2], sys.argv[3]

    x, fs = read_wav_mono(wav_in)
    spec = read_spec_txt(spec_txt)  # (T, K)
    T, K = spec.shape

    # time axis for waveform
    t_wav = np.arange(len(x)) / fs

    # time axis for spectrogram (estimate by distributing frames across wav duration)
    # If frames = 1, just show 0..duration.
    duration = len(x) / fs if len(x) > 0 else 1.0
    if T > 1:
        t_spec = np.linspace(0, duration, T)
    else:
        t_spec = np.array([0.0])

    # frequency axis
    # Default: full spectrum bins k=0..N-1 mapped to 0..fs
    # (If your spectrogram is one-sided, you'll see only 0..fs/2; adjust mapping accordingly.)
    f = np.linspace(0, fs, K)

    fig = plt.figure(figsize=(10, 7))

    # 1) waveform
    ax1 = fig.add_subplot(2, 1, 1)
    ax1.plot(t_wav, x, linewidth=0.8)
    ax1.set_title("Waveform")
    ax1.set_xlabel("Time (s)")
    ax1.set_ylabel("Amplitude")
    ax1.grid(True, linewidth=0.3)

    # 2) spectrogram
    ax2 = fig.add_subplot(2, 1, 2)
    # imshow expects (rows, cols) = (T, K)
    # Use origin='lower' so low freq at bottom
    im = ax2.imshow(
        spec.T,
        aspect="auto",
        origin="lower",
        extent=[t_spec[0], t_spec[-1] if T > 1 else duration, f[0], f[-1]],
    )
    ax2.set_title("Spectrogram (dB)")
    ax2.set_xlabel("Time (s)")
    ax2.set_ylabel("Frequency (Hz)")
    fig.colorbar(im, ax=ax2, pad=0.02)

    fig.tight_layout()
    fig.savefig(pdf_out)
    plt.close(fig)


if __name__ == "__main__":
    main()
