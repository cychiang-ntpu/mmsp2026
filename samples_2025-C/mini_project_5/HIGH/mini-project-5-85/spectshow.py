import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

if len(sys.argv) != 4:
    print("Usage: python spectshow.py input.wav input.txt output.pdf")
    sys.exit(1)

wav_path = sys.argv[1]
txt_path = sys.argv[2]
pdf_path = sys.argv[3]


fs, x = wavfile.read(wav_path)
if x.ndim == 2:
    x = x[:, 0] 
t = np.arange(len(x)) / fs
duration = t[-1] if len(t) > 1 else 0.0


data = np.loadtxt(txt_path)
if data.size == 0:
    raise RuntimeError("Spectrogram txt is empty")


S_db = data.T  
freq_bins, time_frames = S_db.shape


extent = [0.0, duration, 0.0, fs / 2.0]


vmax = np.nanpercentile(S_db, 99)
vmin = vmax - 80  


plt.rcParams.update({
    "font.size": 9,
    "axes.edgecolor": "#666666",
    "xtick.color": "#DDDDDD",
    "ytick.color": "#DDDDDD",
    "axes.labelcolor": "#DDDDDD",
    "text.color": "#DDDDDD",
})

fig = plt.figure(figsize=(12, 5), facecolor="#111111")
gs = fig.add_gridspec(2, 1, height_ratios=[1, 2], hspace=0.18)

# Waveform 
ax0 = fig.add_subplot(gs[0, 0], facecolor="#111111")
x_norm = x / (np.max(np.abs(x)) + 1e-12)  # normalize for consistent look
ax0.plot(t, x_norm, color="#00d084", linewidth=1.2)
ax0.set_title("Waveform")
ax0.set_xlim(0, duration)
ax0.set_ylabel("Amplitude")
ax0.grid(True, color="#2a2a2a", linewidth=0.6, alpha=0.9)

# Spectrogram
ax1 = fig.add_subplot(gs[1, 0], facecolor="#111111")
im = ax1.imshow(
    S_db,
    origin="lower",
    aspect="auto",
    cmap="gray",
    vmin=vmin,
    vmax=vmax,
    extent=extent
)
ax1.set_title("Spectrogram")
ax1.set_xlabel("Time [s]")
ax1.set_ylabel("Frequency [Hz]")
ax1.grid(True, color="#2a2a2a", linewidth=0.6, alpha=0.9)


cbar = fig.colorbar(im, ax=ax1, pad=0.01)
cbar.set_label("Magnitude (dB)")
cbar.ax.yaxis.set_tick_params(color="#DDDDDD")
plt.setp(cbar.ax.get_yticklabels(), color="#DDDDDD")

plt.tight_layout()
plt.savefig(pdf_path, dpi=300, facecolor=fig.get_facecolor())
plt.close()
