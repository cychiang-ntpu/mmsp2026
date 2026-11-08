# 第 9 週補充：Chapter 2／3 的整理版（文字摘要、公式、範例指令；課後參考）

> 上課用的是 2023 原講義 [ch2_2023.md](ch2_2023.md)、[ch3_2023.md](ch3_2023.md)；這份是同樣內容的文字整理，附本課程的範例程式指令與實測數字。

## 第二節｜Chapter 2：聲音的數位化（Digital Data Representation）

### 2.1 ★ A/D 與 D/A：聲音進電腦、再出來（10 分鐘；照 2023 Ch2 2.1 的 SpeechA2D 投影片）

投影片：[SpeechA2D.pdf](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023/blob/master/Chapter-2/SpeechA2D.pdf)（2023 repo）；互動版 [a2d_steps.html](../wk03_0921_team1-kickoff/slides/a2d_steps.html)。

**A/D（ADC，analog-to-digital）**：

```
說話 → 空氣壓力變化 ──麥克風（振膜→電壓）──▶ x_c(t) 類比電壓（連續時間、連續值）
   ① 取樣 sampling      每 T 秒記一次 → x[n] = x_c(nT)            離散時間、連續值
   ② 量化 quantization   每個值四捨五入到 2^m 階之一 → Q(x[n])      離散時間、離散值
   ③ 編碼 coding         每階用 m 個 bit 的二進位表示 → 0101…      PCM
   → 存成 WAV、送上網路（Team 2 的一包 = 320 個 sample 的 PCM）
```

**D/A（DAC，digital-to-analog）**：反過來，而且每一步都有對應：

```
0101… ──① 解碼──▶ 整數 ──② 零階保持（zero-order hold）──▶ 階梯波（每個 sample 撐 T 秒）
      ──③ 重建濾波器（低通，截止 f_s/2）──▶ 平滑的 x̂_c(t) ──放大──▶ 喇叭（電壓→振膜→空氣壓力）
```

- 階梯波的「角」是高頻（f_s 的倍數附近的複製頻譜，2.3 會算出來），低通濾掉它們就剩原訊號；理想情況下（Nyquist 條件成立、不量化）還原是**完美的**。
- 整條路徑裡**只有量化（②）不可逆**：取樣可以還原，編碼只是改寫法。所以 A/D → D/A 的損失 = 量化雜訊（2.4）＋ aliasing（如果取樣太慢，2.3）。
- 兩個參數就決定一切：**f_s**（多久記一次）與 **m**（每次記多細）。

| macOS | Linux / WSL | Windows PowerShell |
|---|---|---|
| `open ../wk03_0921_team1-kickoff/slides/a2d_steps.html` | `xdg-open …/a2d_steps.html` | `start ..\wk03_0921_team1-kickoff\slides2d_steps.html` |

第 3 週 2.1 有麥克風的構造（振膜、動圈／電容）與 DSP 2026 講義的四步驟；今天從數學往下走。

### 2.2 ★ 為什麼要 digital？（5 分鐘）

只是要「把訊號放大」，一顆電晶體的類比放大電路就夠；為什麼要繞一圈 C/D → 離散時間系統 → D/C？

```
x_c(t) ──▶ [C/D (ADC)] ──▶ x[n] ──▶ [離散時間系統（程式）] ──▶ y[n] ──▶ [D/C (DAC)] ──▶ y_c(t)
```

| Digital 的優點 | Digital 的缺點 |
|---|---|
| 可程式：同一顆晶片改軟體就變成濾波器、壓縮器、合成器 | 要多 ADC／DAC 兩個轉換，各有延遲與誤差 |
| 可複製、可儲存、可傳輸而**不累積失真**（數字不會變舊） | 取樣與量化本身就是失真（aliasing、量化雜訊） |
| 可壓縮、可加密、可糾錯（第 3 週的 source／channel coding） | 高取樣率、高位元深度的 ADC 貴、耗電 |
| 精度由 bit 數決定，不受溫度、元件老化影響 | 即時處理有運算量上限（第 7 週 FFT 的 N² 對 N log N） |

DSP 的 D 是 Discrete 還是 Digital？離散時間（discrete-time）只說「時間是整數 n」，值仍可是實數；數位（digital）再加上「值也是有限 bit」。數學推導用 discrete-time，電腦裡跑的是 digital。

### 2.3 ★ 理想週期取樣（Ideal Periodic Sampling）（15 分鐘）

**定義**：取樣是把連續訊號變成數列——聲音在時間上等距取點，影像在空間上等距取點。

```
x[n] = x_c(nT)                          T：取樣週期（秒）；f_s = 1/T：取樣率（Hz）
```

**用數學看它**：把取樣想成「連續訊號乘上一串 impulse」：

```
s(t) = Σ_k δ(t − kT)                                      impulse train（Dirac delta）
x_s(t) = x_c(t)·s(t) = Σ_n x_c(nT)·δ(t − nT)              仍是連續時間的訊號，只在 t = nT 有值
```

訊號與系統學過：時間上的 impulse train，頻率上也是 impulse train：

```
s(t)  ⟷  S(jΩ) = (2π/T)·Σ_k δ(Ω − kΩ_s)                   Ω_s = 2π/T（rad/s）
x_s(t) = x_c(t)·s(t)  ⟷  X_s(jΩ) = (1/2π)·X_c(jΩ) * S(jΩ) = (1/T)·Σ_k X_c(j(Ω − kΩ_s))
```

**結論一句話**：取樣後的頻譜 = 原頻譜**每隔 Ω_s 複製一份**再相加。

```
|X_c(jΩ)|                       |X_s(jΩ)|
   ▲   /\                          ▲   /\      /\      /\      /\
   │  /  \                         │  /  \    /  \    /  \    /  \
───┴─/────\──▶ Ω        ────────/────\──/────\──/────\──/────\──▶ Ω
  −Ω_N    Ω_N                  −Ω_s      0      Ω_s     2Ω_s
                                  ◄──── 複製不重疊的條件：Ω_s − Ω_N > Ω_N ────►
```

- **C/D**（連續到離散）：乘 impulse train，再把 impulse 的面積拿出來當 x[n]。
- **D/C**（離散到連續）：把 x[n] 放回 impulse 的位置，過一個截止在 Ω_s/2 的理想低通濾波器，複製的頻譜被濾掉，剩下原來那一份 → 完美還原。（真實的 DAC 是「零階保持」的階梯波加上類比低通。）

**Nyquist 定理**：取樣率 ≥ 訊號最高頻率成分的兩倍（Ω_s ≥ 2Ω_N），複製的頻譜才不會重疊，才有機會用低通濾波器還原。
**Aliasing**：Ω_s < 2Ω_N 時，複製的頻譜疊到原來的頻譜上，高頻成分「假扮」成低頻，**還原不回來**。

- **Nyquist rate vs. Nyquist frequency**（不要搞混）：Nyquist rate = 2·f_max，是**訊號**要求的最低取樣率；Nyquist frequency = f_s/2，是**系統**能表示的最高頻率。（2023 講義把 Nyquist rate 寫成 f_s，本講義採一般定義。）
- 例（第 3 週 `alias_demo.py`）：f_s = 8 000 Hz。
  - Case 1，f = 3 000：x₁[n] = sin(2π·3000·n/8000)，沒有 aliasing。
  - Case 2，f = 5 000：x₂[n] = sin(2π·5000·n/8000) = sin(2π·(8000−3000)·n/8000) = sin(2πn − 2π·3000·n/8000) = **−sin(2π·3000·n/8000) = −x₁[n]**。5 kHz 取樣後和 3 kHz 的波形一模一樣（只差正負號），聽起來就是 3 kHz。（2023 講義少了負號；程式印出 x₁ + x₂ = 0 可以驗證。）
- Undersampling 在影像上：取樣跟不上花紋的變化，出現摩爾紋（moiré）、假的圖案；聲音上就是假的低頻。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../wk03_0921_team1-kickoff/examples/alias_demo.py` | `python ..\wk03_0921_team1-kickoff\examples\alias_demo.py` |

課後讀：第 3 週 2.2 的「頻譜複製」小節、Oppenheim & Schafer ch. 4（取樣定理的完整推導、D/C 的零階保持）。

### 2.4 ★ 量化（Quantization）（15 分鐘）

每個 sample 用固定的 m 個 bit 表示，m 叫 **sample size／bit depth**；能表示的值有 **L = 2^m 個等級**（quantization levels）。影像每個像素的 R、G、B 各 8 bits → 256 階；聲音常用 16 bits（65 536 階；2023 講義寫 65,532 是筆誤）或 32 bits。

**均勻（線性）量化**，階距 Δ：

```
Q(x[n]) = Δ · ⌊ x[n]/Δ + 1/2 ⌋              四捨五入到最近的階
e[n] = x[n] − Q(x[n])                       量化誤差，|e[n]| ≤ Δ/2
```

教科書的例子（Oppenheim & Schafer）：x[n] = 0.99·cos(n/10)，3-bit 量化後波形變成階梯、誤差 e[n] 是一串看得出規律的鋸齒；8-bit 時誤差小到像雜訊。這就是為什麼量化誤差常被**當成均勻分布的雜訊**來分析（Bennett 1948）。

**SNR**（signal-to-noise ratio）= 訊號功率 ÷ 雜訊功率；用 dB 表示：

```
SNR = P_signal / P_noise = A_signal² / A_noise²        （P ∝ V²）
SNR(dB) = 10·log₁₀(P_signal / P_noise) = 20·log₁₀(A_signal / A_noise)
```

**SQNR**（signal-to-quantization-noise ratio）：雜訊就是量化誤差。用整段訊號算：

```
P  = E[x²[n]] = (1/N)·Σ x²[n]          P₀ = E[e²[n]] = (1/N)·Σ e²[n]          SQNR(dB) = 10·log₁₀(P / P₀)
```

**動態範圍（dynamic range）**：量化後的訊號「能分得多細」，不是值域的大小。用最大振幅對最大量化誤差的比來估：

```
SQNR(dB) ≈ 10·log₁₀( (2^{m−1}·Δ)² / (0.5·Δ)² ) = 20·log₁₀(2^m) ≈ 6.02·m dB
```

每多 1 bit，動態範圍多 6.02 dB：8 bits 約 48 dB、16 bits 約 96 dB、24 bits 約 144 dB。
這是「峰值比」的版本；MP2 量的是功率比，滿刻度弦波會多 1.76 dB（6.02m + 1.76），第 3 週 2.4 兩個都講過，考試要說清楚用哪一個。

**非線性量化**：A-law、μ-law（G.711）——小訊號階距細、大訊號粗，讓 SQNR 不隨音量掉，電話用 8 bits 就能聽；原理與實測在 [audio_coding.md](audio_coding.md) 2.2，也是 Team 2 功能 4。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../wk03_0921_team1-kickoff/examples/quantize_demo.py` | `python ..\wk03_0921_team1-kickoff\examples\quantize_demo.py` |

實測（440 Hz 滿刻度弦波、f_s = 8 kHz）：4 bits 23.2 dB、8 bits 49.6 dB、12 bits 74.6 dB、16 bits 98.5 dB；振幅減半每列少 6 dB。

### 2.5 PCM 與 WAV：把數字存起來（5 分鐘；第 3 週 2.6 講過）

量化後的整數用二進位存，就是 **PCM**（pulse code modulation）；WAV 是把 PCM 裝進 RIFF 容器（`fmt ` chunk 記 f_s、聲道、bits；`data` chunk 是 sample，little-endian，`data` 不一定在第 44 byte）。
MP2 的 function generator 就是在模擬一台 A/D：產生 x_c(t) → 取樣 → 量化（`rint`）→ 寫 WAV → 算 SQNR。2023 的 MP3「Generating Waves」是同一題（`sinegen fs m wavetype f A T`）。

打開 [wav_bmp_viewer.html](../wk03_0921_team1-kickoff/slides/wav_bmp_viewer.html)：換取樣率、bit depth 再聽；用 hex 逐欄讀檔頭。

### 2.6 小結：Chapter 2 的三個數字（2 分鐘）

| 參數 | 決定什麼 | 太小會怎樣 | 常見值 |
|---|---|---|---|
| f_s | 能表示的最高頻率 f_s/2 | aliasing | 8k（電話）、16k（寬頻語音）、44.1k（CD）、48k（影片）、96k／192k（DVD-Audio） |
| m（bit depth） | 動態範圍 ≈ 6.02·m dB | 量化雜訊、小聲時粗糙 | 8（電話，配 μ-law）、16（CD）、24（錄音室） |
| 聲道 | 空間感 | — | 1、2、5.1 |

資料率 = f_s × m × 聲道（第 3 週 1.1 的計算機）。

---

## 第三節｜Chapter 3：數位音訊的表示（Digital Audio Representation）

課程目標（2023）：1. 了解音訊非常粗淺的物理特性；2. 回顧音訊的來源與儲存方法；3. 音訊的量測與人類的感知；4. 時頻譜（spectrogram）。

### 3.1 ★ 聲音從哪裡來（8 分鐘）

**聲波**是聲源振動產生的機械波。在空氣與液體裡是**縱波**（疏密波：質點振動方向與傳播方向平行；空氣擠在一起氣壓升高、散開氣壓降低），在固體裡可能混有**橫波**（振動方向與傳播方向垂直，像繩波）。人耳大約聽 20 Hz–20 kHz；狗聽得到 50 kHz，但 15 Hz 以下聽不到。

**語音**：發音器官的結構（肺 → 聲帶 → 咽、口腔、鼻腔 → 嘴唇）可以簡化成 **source–filter 模型**：
- **聲源**：聲帶振動（有聲音，週期 1/F₀）或氣流通過狹縫的湍流（無聲音，像雜訊）。
- **濾波器**：聲道是一根會變形的管子（tube resonance model），不同形狀有不同的共振頻率（**formant**），決定是哪個母音。
- 數位模型：脈衝串或雜訊 → 聲道濾波器 → 輻射 → 語音；頻譜 = 聲源頻譜（harmonics）× 聲道頻率響應（formant 包絡）。這就是第 7 週 1.3 與 Team 2 PLC（packet loss concealment，封包遺失補償）「pitch 重複」的根據，也是 [audio_coding.md](audio_coding.md) LPC／CELP 的出發點。
- 影片（2023 連結）：聲帶振動的內視鏡影像（Sean Parker Institute for the Voice）、tube resonance（voicescienceworks.org）、Vibration of the Vocal Folds 動畫。

**樂器**：
- 吉他弦的振動影片——注意影片裡弦看起來慢慢擺動，那是**攝影機的 frame rate 取樣了弦的高頻振動**造成的 aliasing（弦 637 Hz、攝影 500 fps → 看起來 137 Hz），正是 2.3 的現象。
- 鋼琴的擊弦機制；銅鈸與小鼓的慢動作（膜與板的振動模態）。

### 3.2 ★ 拾取與數位化：waveform 與 PCM（7 分鐘）

機械波 → 麥克風振膜 → **電壓**：記下振幅隨時間的變化就是 **waveform（波形）**。電壓 → ADC → 0／1 的序列：最常用的表示法叫 **PCM**。

- PCM 由 Alec Reeves 於 1937 年在 International Telephone and Telegraph 發明；當時的重點是「調變」：每個 sample 量化、用二進位編碼、把 bit 當成脈衝傳出去。今天 PCM 這個詞在數位音訊裡就是指「取樣＋量化、未壓縮」的資料；raw 的 PCM 檔沒有檔頭。
- 新建一個音訊檔時軟體問你的兩個問題就是 **sampling rate** 與 **bit depth**。
- 取樣率的常見選擇：8 kHz 單聲道（電話）、44.1 kHz 雙聲道 16 bits（CD）、48 kHz（DAT、影片）、96／192 kHz 24 bits（DVD-Audio）。
- Bit depth 限制每個 sample 的**精度**，決定 SQNR 與動態範圍；16 bits 最常用（值域 [−2¹⁵, 2¹⁵−1]，C 的 `short`）；處理中常用 32-bit（浮點）以免中間運算累積誤差，最後再降回 16 bits。
- 選擇的考量：要和其他要混音的素材一致；錄音的對象（語音 16 kHz 就夠，音樂要 44.1k 以上）；儲存媒體容量；播放設備的靈敏度；要加特效時先用高一點的規格。

2023 講義的 C 範例：產生 1 秒 440 Hz 弦波、振幅 10 000、16 kHz，`x[n] = (short)floor(A·sin(2π·f·n·T) + 0.5)` 寫成 `.pcm`——就是 MP2 去掉 WAV 檔頭的版本。

### 3.3 ★ 音訊的基礎量測：頻率與振幅（12 分鐘）

**頻率（frequency）**：介質質點每秒來回振動的次數，單位 Hz（cycle/second）。弦波的一個 cycle 是一個完整的來回；弦波的聲音叫 **pure tone**。

**用波形量週期**（2023 用 WaveSurfer 看「voice」這個詞；今天用 [frame_demo.py](../wk07_1019_audio-pcm/examples/frame_demo.py)）：母音穩定的一段裡可以看到波形重複；數 5 個週期佔的時間除以 5 就是**基本週期**，倒數是 **F₀**。
2023 的例子：0.1112–0.1352 s 有 5 個週期 → T₀ = 0.0048 s → F₀ ≈ 208 Hz。今天的檔案：t = 1.0 s 處 τ = 102 sample = 6.4 ms → F₀ ≈ 157 Hz。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../wk07_1019_audio-pcm/examples/frame_demo.py ../wk07_1019_audio-pcm/data/aeueo-16kHz.wav 1.0` | `python ..\wk07_1019_audio-pcm\examples\frame_demo.py ..\wk07_1019_audio-pcm\data\aeueo-16kHz.wav 1.0` |

三種訊號：
- **週期訊號**：存在最小的 N 使 x[n] = x[n+N]。
- **準週期（quasi-periodic）**：相鄰週期很像但不完全一樣（四季變化也是）。大部分語音是這一種。
- **非週期**：氣音、擦音，像雜訊。

**分析語音的基本假設**：整段不是週期的，但 20–40 ms 的 **frame** 內近似週期（16 kHz × 20 ms = 320 個 sample——Team 2 的一包）。週期訊號可以拆成 **F₀ 的弦波＋整數倍的 harmonics**（傅立葉級數）；用 DFT 分析一個 frame 就看得到。MP3 的 frame 是 576 或 1152 個 sample（44.1 kHz 下 13／26 ms）。

**振幅（amplitude）** 有好幾種單位，都在用，要分清楚：

| 單位 | 意義 | 數字 |
|---|---|---|
| **Pa**（N/m²，氣壓的變化量） | 聲音造成的氣壓對平均大氣壓（約 10⁵ Pa）的偏差 | 聽覺門檻（1 kHz）：2×10⁻⁵ Pa |
| **dB SPL**（sound pressure level） | 20·log₁₀(p / p₀)，p₀ = 20 μPa（聽覺門檻），所以門檻 = 0 dB SPL | 耳語 30、交談 60、捷運 90、痛覺 120–130 dB SPL；**+10 dB 聽起來約兩倍大聲；3 dB 是大多數人分得出的最小差別** |
| **sample 值** | 電腦裡的整數：16 bits 是 [−32768, 32767] | — |
| **dBFS**（full scale） | 20·log₁₀(\|x\| / 32768)：滿刻度 = 0 dBFS，只能是負的 | 10 000 → −10.3 dBFS；1 → −90.3 dBFS |
| **百分比** | 最大 = 100%、最小 = −100% | — |
| **normalized** | 振幅正規化到 [−1, 1]（MP2 的 A、`quantize_demo.py` 的 x） | — |

dB 永遠是「相對某個參考」：SPL 的參考是聽覺門檻，FS 的參考是滿刻度，SNR 的參考是雜訊。用 log 是因為人耳對大小聲的感覺接近對數（線上計算機：sengpielaudio.com）。

### 3.4 ★ 人的感知：客觀量 vs 主觀量（8 分鐘）

| 客觀（儀器量） | 主觀（人聽到的） | 關係 |
|---|---|---|
| frequency（Hz） | **pitch（音高）** | 大致對數：高八度 = 頻率 ×2；250 vs 260 Hz 分得出，1000 vs 1010 Hz 幾乎分不出。「音準」是感知的說法 |
| intensity／energy（dB） | **loudness（響度）** | 大致對數；而且**人耳對不同頻率的靈敏度不同** |

- **絕對聽覺門檻（ATH）**：最小能聽到的聲壓，隨頻率變，2–5 kHz 最靈敏（門檻最低），低頻與高頻要大聲很多才聽得到。
- **等響曲線（equal-loudness contour）**：同樣「聽起來一樣大聲」的各頻率需要的 dB SPL；低音量時低頻要補很多（音響的 loudness 鈕）。
- **Mel scale**、**Bark scale**：把頻率軸換成「聽起來等距」的尺度：1 kHz 以下近似線性、以上近似對數。語音辨識的 MFCC、MP3 的頻帶切法都用它。
- 線上工具：Online Tone Generator（szynalski.com）——課堂放 250／260 Hz 與 1000／1010 Hz 比較；放 50 Hz 與 4 kHz 同樣 dBFS 比較響度。

這一節是有失真壓縮（MP3／AAC）的根據：**聽不到的不用存**（[audio_coding.md](audio_coding.md) 感知編碼）。

### 3.5 ★ DFT 複習與 spectrogram（15 分鐘；第 7 週 1.4–1.6 的濃縮）

**FT、DTFT、DFT 的關係**：

| 轉換 | 輸入 | 輸出 | 用在 |
|---|---|---|---|
| FT（Fourier transform） | 連續時間 x_c(t) | 連續頻率 X(jΩ) | 2.3 的推導 |
| DTFT | 離散時間 x[n]（無限長） | 連續、週期 2π 的 X(e^{jω}) | 理論 |
| **DFT** | 有限長 N 點 x[n] | N 個離散的 X[k] | **電腦算的就是它**；FFT 是它的快速算法 |

DFT 是 DTFT 在 N 個等距頻率上的取樣；隱含假設 x[n] 是週期為 N 的週期訊號（所以要用窗函數，第 7 週 1.5）。

**DFT 裡的頻率**：

```
X[k] = Σ_{n=0}^{N−1} x[n]·e^{−j2πkn/N}         f_k = k·f_s / N（Hz）      k = 0 … N/2 有意義，最高 = f_s/2
```

把 x[n] 寫成 N/2+1 個弦波的和：x[n] = Σ_k A[k]·cos(2π(k/N)f_s·t + φ[k])|_{t=n/f_s}；第 k 根就是頻率 k·f_s/N 的成分，振幅 A[k]，畫頻譜時縱軸用 20·log₁₀A[k]（dB）。例：f_s = 16 000、N = 512 → 每根 31.25 Hz；k = 32 → 1 000 Hz。

**Spectrogram 四步驟**（[stft_steps.html](../wk07_1019_audio-pcm/slides/stft_steps.html)）：framing（P、hop M）→ windowing（Hamming）→ zero padding（到 N）→ DFT；一列一個 frame、橫軸時間、縱軸頻率、亮度 dB。

- **P 決定頻率解析度**（主瓣寬 ∝ f_s/P）：窄頻（32 ms）看到 harmonics，寬頻（4 ms）看到 pitch pulse 與 formant。
- **N 只是畫幾根**（zero padding 是內插）。
- **M 決定時間解析度**。
- 窗函數：rectangular 的接縫不連續 → leakage（side lobe −13 dB）；Hamming 主瓣寬一倍、side lobe −41 dB。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../wk07_1019_audio-pcm/examples/window_demo.py` | `python ..\wk07_1019_audio-pcm\examples\window_demo.py` |
| `python3 ../wk07_1019_audio-pcm/examples/stft_demo.py ../wk07_1019_audio-pcm/data/aeueo-16kHz.wav --ascii` | `python ..\wk07_1019_audio-pcm\examples\stft_demo.py ..\wk07_1019_audio-pcm\data\aeueo-16kHz.wav --ascii` |

2023 的 Chapter 3 以「Mini Project 2 — 音訊的時頻分析」收尾，就是今年的 MP5（已截止；[第 7 週 1.7](../wk07_1019_audio-pcm/README.md) 有今年的介面與常見錯誤）。

### 3.6 收尾（5 分鐘）

- Team 2：11/9 工作坊前讀 [team2_core.md](team2_core.md)；11/9 自習（m1–m9 11/11 前 push）、11/16 工作坊跟著 [tutorial](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 做（每步 push）；11/22 截止、11/23 評測；**11/9 規格定版**：tutorial 六步（m1–m9、step1–6 全部 push ＝ 40 分）做出能通話的 raw PCM voicelink，作業是在 `mulaw_system/`、`adpcm_huffman_system/` 加 μ-law、ADPCM、Huffman（demo 30、程式碼審查 30）。
- 語音壓縮（μ-law、DPCM，Team 2 功能 4）：[audio_coding.md](audio_coding.md)，11/16 前看完。

---

