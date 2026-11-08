# 第 9 週（2026/11/2）｜期中考；聲音的數位表示（A/D、PCM）與數位音訊的量測、感知、頻譜

對應：期中考、MP2／MP5 複習、Team 2
時間：13:10–16:00，三節。

> **課前準備（同學）**：`git pull`；帶耳機。用電腦教室公用電腦的同學：一開機先做 [../wk03_0921_team1-kickoff/setup_first.md](../wk03_0921_team1-kickoff/setup_first.md)。
> **跟著打指令請開 [commands.md](commands.md)**。
> 期中考範例：[midterm_sample.md](midterm_sample.md)／[詳解](midterm_sample_solutions.md)。
>
> **Team 2 的同學注意**：10/19 沒講到的 Team 2 核心（為什麼 UDP、封包、jitter buffer、PLC（packet loss concealment，封包遺失補償））整理在 [team2_core.md](team2_core.md)，11/9 工作坊前讀完；11/9、11/16 全班跟著 [tutorial](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 一步一步做。語音壓縮（μ-law、DPCM）在 [audio_coding.md](audio_coding.md)。

第二、三節用的是授課教師 2023 年的 Chapter 2、Chapter 3 講義**原文**（[ch2_2023.md](ch2_2023.md)、[ch3_2023.md](ch3_2023.md)，含圖與上課影片連結），內容比兩節課能講的多：課堂挑重點，其餘課後讀。本課程的範例程式、公式整理與實測數字另放在 [supplement.md](supplement.md)。

## 本週目標

上完課你應該能：

1. 畫出「麥克風 → 取樣 → 量化 → 編碼 → 儲存／傳輸 → 解碼 → DAC → 喇叭」的完整路徑，說出每一步在做什麼、哪一步會失真。
2. 寫出理想取樣的數學（x[n] = x_c(nT)、頻譜以 Ω_s 週期複製），說明 Nyquist 條件與 aliasing，並用 8 kHz 取樣 3 kHz／5 kHz 的例子驗證。
3. 寫出均勻量化的式子，算出量化誤差、SQNR 與動態範圍（6.02·m dB），解釋 bit depth 與 dynamic range 的關係。
4. 說出聲音怎麼產生（語音：source–filter；樂器：弦、膜、管）、怎麼拾取（振膜 → 電壓 → PCM），以及 PCM 這個詞的來歷。
5. 用三種單位量振幅（Pa／dB SPL、dBFS、normalized），用波形量週期與 F₀，並區分客觀量（frequency、intensity）與主觀感知（pitch、loudness）。
6. 說出 FT、DTFT、DFT 的關係、DFT 第 k 根對應幾 Hz，以及 spectrogram 的四個步驟與時間／頻率解析度的取捨。

## 時程

| 節次 | 時間 | 內容 | 教材／範例 |
|---|---|---|---|
| 1 | 13:10–14:00 | **期中考**（50 分鐘閉卷） | [midterm_sample.md](midterm_sample.md) |
| 2 | 14:10–15:00 | **Chapter 2（2023 原講義 [ch2_2023.md](ch2_2023.md)）**：A/D 總覽（SpeechA2D）、為什麼 digital、理想週期取樣、Nyquist 與 aliasing、量化與 SQNR／動態範圍、產生波形 | [a2d_steps.html](../wk03_0921_team1-kickoff/slides/a2d_steps.html)、[alias_demo.py](../wk03_0921_team1-kickoff/examples/alias_demo.py)、[quantize_demo.py](../wk03_0921_team1-kickoff/examples/quantize_demo.py)、[wav_bmp_viewer.html](../wk03_0921_team1-kickoff/slides/wav_bmp_viewer.html) |
| 3 | 15:10–16:00 | **Chapter 3（2023 原講義 [ch3_2023.md](ch3_2023.md)）**：聲音的來源、拾取與 PCM、量測（頻率、振幅、dB）、人類感知、DFT 複習、時頻分析 | [frame_demo.py](../wk07_1019_audio-pcm/examples/frame_demo.py)、[window_demo.py](../wk07_1019_audio-pcm/examples/window_demo.py)、[stft_steps.html](../wk07_1019_audio-pcm/slides/stft_steps.html)、[stft_demo.py](../wk07_1019_audio-pcm/examples/stft_demo.py) |

---

## 第二節｜Chapter 2：Digital Data (Signal) Representation（2023 原講義）

上課投影 **[ch2_2023.md](ch2_2023.md)**（2023 Chapter 2 原文，含圖與上課影片連結），順序：
2.1 Analog to Digital Conversion in Brief（[SpeechA2D.pdf](slides/SpeechA2D.pdf)）→ 2.2 Why Digital? → 2.3 Ideal Periodic Sampling（C/D、D/C、Nyquist、aliasing 的 3 kHz／5 kHz 例子）→ 2.4 Quantization（SNR、SQNR、dynamic range、A-law／μ-law）→ 2.5 Generating Waves（2023 的作業，就是今年的 MP2）。

課堂示範（指令在 [commands.md](commands.md)）：[a2d_steps.html](../wk03_0921_team1-kickoff/slides/a2d_steps.html)（2.1）、`alias_demo.py`（2.3）、`quantize_demo.py`（2.4）、[wav_bmp_viewer.html](../wk03_0921_team1-kickoff/slides/wav_bmp_viewer.html)（2.5）。

兩個原講義的小出入（上課時留意）：2.3 的 Case 2 等式少了負號，sin(2π·5000·n/8000) = **−**sin(2π·3000·n/8000)；2.4 的 16 bits 是 65,536 階。文字整理版（公式、表格、實測數字）在 [supplement.md](supplement.md)。

## 第三節｜Chapter 3：Digital Audio Representation（2023 原講義）

上課投影 **[ch3_2023.md](ch3_2023.md)**，順序：
3.1 音訊／聲音的來源（聲波、語音發音器官與 source–filter、樂器）→ 3.2 拾取與數位化（waveform、PCM、取樣率與 bit depth 的選擇、C 範例）→ 3.3 基礎量測（frequency、用 [voice.wav](data/voice.wav) 量週期、periodic／quasi-periodic、frame；amplitude：Pa、dB SPL、dBFS、百分比、normalized）→ 3.4 人類感知（pitch、loudness、ATH 與等響曲線、Mel／Bark）→ 3.5 Reviews on DFT（FT／DTFT／DFT、DFT 的頻率）→ 3.6 Mini Project 2 時頻分析（就是今年的 MP5）。

課堂示範：`frame_demo.py`（3.3 量 F₀）、Online Tone Generator（3.4）、`window_demo.py`、[stft_steps.html](../wk07_1019_audio-pcm/slides/stft_steps.html)、`stft_demo.py --ascii`（3.5）。文字整理版在 [supplement.md](supplement.md)。

### 收尾（5 分鐘）

- Team 2：11/9 工作坊前讀 [team2_core.md](team2_core.md)；11/9、11/16 全班跟著 [tutorial](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 做（每步 push）；11/22 截止、11/23 評測；規格已簡化為三層（基本層必過）。
- 語音壓縮（μ-law、DPCM，Team 2 功能 4）：[audio_coding.md](audio_coding.md)，11/16 前看完。

---

## 範例程式（examples/）與本週用到的其他材料

| 檔案 | 用途 | 執行 |
|---|---|---|
| [ch2_2023.md](ch2_2023.md)、[ch3_2023.md](ch3_2023.md) | 2023 原講義（上課投影） | — |
| [slides/SpeechA2D.pdf](slides/SpeechA2D.pdf)、[data/voice.wav](data/voice.wav) | 原講義的投影片與音檔 | — |
| [supplement.md](supplement.md) | 文字整理版 | — |
| [commands.md](commands.md) | 上課跟著打的指令 | 上課時開著 |
| [a2d_steps.html](../wk03_0921_team1-kickoff/slides/a2d_steps.html) | A/D 四步驟動畫（2.1） | 瀏覽器 |
| [alias_demo.py](../wk03_0921_team1-kickoff/examples/alias_demo.py) | 3 kHz 與 5 kHz 在 8 kHz 取樣（2.3） | `python3 alias_demo.py` |
| [quantize_demo.py](../wk03_0921_team1-kickoff/examples/quantize_demo.py) | 量化、SQNR（2.4） | `python3 quantize_demo.py [振幅]` |
| [wav_bmp_viewer.html](../wk03_0921_team1-kickoff/slides/wav_bmp_viewer.html) | 播放、檔頭、換取樣率與 bit depth（2.5） | 瀏覽器 |
| [frame_demo.py](../wk07_1019_audio-pcm/examples/frame_demo.py) | 波形量週期、F₀（3.3） | `python3 frame_demo.py 檔案.wav 秒` |
| [window_demo.py](../wk07_1019_audio-pcm/examples/window_demo.py)、[stft_demo.py](../wk07_1019_audio-pcm/examples/stft_demo.py)、[stft_steps.html](../wk07_1019_audio-pcm/slides/stft_steps.html) | DFT、窗、spectrogram（3.5） | 見表格內指令 |
| [examples/mulaw_demo.py](examples/mulaw_demo.py)、[examples/dpcm_demo.py](examples/dpcm_demo.py) | 語音壓縮（[audio_coding.md](audio_coding.md)） | `python3 mulaw_demo.py`、`python3 dpcm_demo.py 檔案.wav` |
| [team2_core.md](team2_core.md)、[lab.md](lab.md) | Team 2 核心與實作工作坊 | — |

## 回家作業／下週前要做的事

1. 讀 [team2_core.md](team2_core.md)；11/9 工作坊全班跟著 tutorial 做第 1–3 步，帶筆電、熱點、耳機。
2. 用 `alias_demo.py` 改 f_s = 16 000、f = 11 000，預測會變成幾 Hz 再跑（答案：5 000）。
3. 用 `quantize_demo.py 0.1`：振幅 0.1（−20 dBFS）時 8 bits 的 SQNR 剩幾 dB？這就是 μ-law 要解決的問題。
4. 11/22 18:00 Team 2 截止；11/23 評測。

## 素材來源與授權

ch2_2023.md、ch3_2023.md、slides/SpeechA2D.pdf、data/voice.wav 是授課教師 2023 年課程 repo（[ntpu-ce-mmsp-2023](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023)）的原檔；原講義內的圖片各自標了來源（Wikimedia Commons 公有領域、beis.de、Springer Encyclopedia of Biometrics、Oppenheim & Schafer 的教科書圖等）。範例程式與其他音檔的授權見第 3、7 週的 data/README。

## 參考

- 2023 講義：[Chapter 2 Digital Data (Signal) Representation](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023/tree/master/Chapter-2)（含 SpeechA2D 投影片與上課影片）、[Chapter 3 Digital Audio Representation](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023/tree/master/Chapter-3)（含上課影片、fft_example）。
- A. V. Oppenheim and R. W. Schafer, *Discrete-Time Signal Processing*, 3rd ed., Pearson, 2010, ch. 4（取樣、C/D、D/C、量化的例子）、ch. 8（DFT）、ch. 10（spectral analysis）。
- J. Burg, *The Science of Digital Media*, Pearson, 2009, ch. 1（A/D、bit depth、dynamic range）、ch. 4（digital audio representation：PCM、dB、dBFS、感知）。
- X. Huang, A. Acero, H.-W. Hon, *Spoken Language Processing*, Prentice Hall, 2001, ch. 2（語音產生、聽覺感知：ATH、等響曲線、Mel／Bark）、ch. 6（spectrogram）。
- L. Docio-Fernandez and C. García Mateo, "Speech Production," in *Encyclopedia of Biometrics*, Springer, 2015. [doi:10.1007/978-1-4899-7488-4_199](https://doi.org/10.1007/978-1-4899-7488-4_199)（發音器官與 tube resonance 的圖）。
- W. R. Bennett, "Spectra of Quantized Signals," *Bell System Technical Journal*, vol. 27, no. 3, pp. 446–472, July 1948. [doi:10.1002/j.1538-7305.1948.tb01340.x](https://doi.org/10.1002/j.1538-7305.1948.tb01340.x)（量化誤差當雜訊的模型）。
- H. Nyquist (1928)、C. E. Shannon (1949)：取樣定理，見[第 7 週參考](../wk07_1019_audio-pcm/README.md#參考)。
- ITU-T G.711（μ-law／A-law）：<https://www.itu.int/rec/T-REC-G.711>。
- 第 3 週講義 2.1–2.6（麥克風、ADC 四步驟、WAV）、第 7 週講義 1.2–1.6（感知、frame、DFT、窗、STFT）。
