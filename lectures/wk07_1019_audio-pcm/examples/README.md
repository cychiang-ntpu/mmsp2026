# 第 7 週範例程式（只用 Python 標準函式庫，不需要 numpy）

| 檔案 | 看什麼 | 講義 |
|---|---|---|
| `frame_demo.py` | 一個 32 ms 的語音 frame：ASCII 波形、自相關估 F0、接縫處加 Hann 窗前後的差別 | 1.x STFT |
| `stft_demo.py` | framing → 加窗 → zero padding → 自己寫的 radix-2 FFT；寫出 MP5 格式的 `.spec.txt`，`--ascii` 印 spectrogram | 1.x STFT |
| `window_demo.py` | rectangular／Hann／Hamming 的主瓣寬度、旁瓣高度；bin 對正與沒對正的 leakage | 1.x STFT |
| `jitter_sim.py` | 離線模擬 VoiceLink 接收端：buffer 從 0 掃到 200 ms 的 late／concealed／delay 表 | 2.x Team 2 |
| `Makefile` | `make check` 全部跑一次並檢查 spectrogram 的列數；`make clean` | — |

## 怎麼跑

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 frame_demo.py ../data/aeueo-16kHz.wav 1.0` | `python frame_demo.py ..\data\aeueo-16kHz.wav 1.0` |
| `python3 stft_demo.py ../data/aeueo-16kHz.wav --ascii` | `python stft_demo.py ..\data\aeueo-16kHz.wav --ascii` |
| `python3 stft_demo.py --selftest` | `python stft_demo.py --selftest` |
| `python3 window_demo.py` | `python window_demo.py` |
| `python3 jitter_sim.py --jitter 30 --loss 0.05` | `python jitter_sim.py --jitter 30 --loss 0.05` |
| `make check` | `mingw32-make check` |

`stft_demo.py` 的選項：`--win hamming|rectangular --w_ms 32 --dft_ms 32 --hop_ms 10`。N 不是 2 的次方時退回 O(N²) 的直接 DFT（很慢）。
