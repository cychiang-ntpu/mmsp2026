import numpy as np
import wave
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import sys

if len(sys.argv) != 4:
    print("Usage: python3 spectshow.py <input_wav> <input_txt> <output_pdf>")
    sys.exit(1)

in_wav = sys.argv[1]
in_txt = sys.argv[2]
out_pdf = sys.argv[3]

# 讀 WAV
with wave.open(in_wav, 'rb') as w:
    nframes = w.getnframes()
    framerate = w.getframerate()
    nchannels = w.getnchannels()
    sampwidth = w.getsampwidth()
    frames = w.readframes(nframes)

if sampwidth == 2:
    samples = np.frombuffer(frames, dtype=np.int16)
elif sampwidth == 1:
    samples = np.frombuffer(frames, dtype=np.uint8).astype(np.int16) - 128
elif sampwidth == 4:
    samples = np.frombuffer(frames, dtype=np.int32)
else:
    samples = np.frombuffer(frames, dtype=np.int16)

if nchannels > 1:
    samples = samples.reshape(-1, nchannels)[:, 0]

if sampwidth == 2:
    samples = samples.astype(np.float64) / 32768.0
elif sampwidth == 1:
    samples = samples.astype(np.float64) / 128.0
elif sampwidth == 4:
    samples = samples.astype(np.float64) / 2147483648.0

# 讀取 TXT
spect_data = np.loadtxt(in_txt)
if spect_data.ndim == 1:
    spect_data = spect_data[np.newaxis, :]
time_frames, freq_bins = spect_data.shape

# 畫波形
duration = nframes / float(framerate)
time_axis = np.linspace(0, duration, num=nframes, endpoint=False)
fig1, ax1 = plt.subplots()
ax1.plot(time_axis, samples, color='blue')
ax1.set_xlabel("Time (s)")
ax1.set_ylabel("Amplitude")
ax1.set_title("Waveform")

# 畫頻譜
fig2, ax2 = plt.subplots()
spect_T = spect_data.T
nyquist = framerate / 2.0
img = ax2.imshow(spect_T, aspect='auto', origin='lower',
                 extent=[0, duration, 0, nyquist], cmap='viridis')
ax2.set_xlabel("Time (s)")
ax2.set_ylabel("Frequency (Hz)")
ax2.set_title("Spectrogram")

# 存成 PDF
with PdfPages(out_pdf) as pdf:
    pdf.savefig(fig1)
    pdf.savefig(fig2)
