# 語音編碼的方法（配合 Team 2 功能 4；課後讀，11/16 討論會前看完）

> 原本排在 11/2 第二、三節，改為課後讀：第 2.1–2.3 節是 μ-law 與 DPCM 的原理（Team 2 的 `--codec mulaw` 就是 2.2），後面是延伸。範例：[examples/mulaw_demo.py](examples/mulaw_demo.py)、[examples/dpcm_demo.py](examples/dpcm_demo.py)。

## 從 256 kb/s 往下壓

### 2.1 不壓縮要多少？Team 2 的封包開銷（8 分鐘）

| 項目 | 數字 |
|---|---|
| Team 2 的 PCM | 16 000 sample/s × 16 bits = **256 kb/s** |
| 每 20 ms 一包 | 640 bytes PCM ＋ 12 bytes header ＋ UDP 8 ＋ IP 20 ＋ 乙太網路 14 = 694 bytes → 50 包/s → **277.6 kb/s**；開銷 8.4% |
| 電話（G.711） | 8 kHz × 8 bits = **64 kb/s**；同樣 20 ms 一包只有 160 bytes，開銷 25% |
| Opus（LINE、Discord、WebRTC） | 語音 **6–40 kb/s**；20 ms 一包只有 15–100 bytes，header 比資料還大 |

兩個觀察：(1) 封包的固定開銷（54 bytes）不會隨壓縮變小，所以**壓得越兇，framing 的設計越重要**（這是為什麼 RTP header 只有 12 bytes，而且 Opus 不會把 frame 切更短）。(2) 壓縮是為了**位元率**（頻寬、行動網路的費用、同時通話的人數），不是為了「存起來」。

把 256 壓到 64、再壓到 8 kb/s，歷史上用了三種路線：**壓伸（量化階距隨大小變）→ 預測（只送差值）→ 參數／感知（送「怎麼合成」而不是波形）**。今天照這個順序講。

### 2.2 壓伸（companding）：μ-law／A-law，為什麼 8 bits 能打電話（15 分鐘）

第 3 週 2.4 的結論：均勻量化的 SQNR ≈ 6.02n + 1.76 dB，但那是**滿刻度**的訊號。語音大部分時間是小聲的（母音之間、句子尾巴），小聲 30 dB 時 SQNR 也掉 30 dB——8-bit 線性量化在 −30 dBFS 只剩 8 dB，根本不能聽。

解法：**量化前先把訊號壓成對數曲線**，小訊號被放大、大訊號被壓縮，再做均勻量化；解碼端反過來展開。小訊號得到細的階距、大訊號粗的階距，**相對誤差差不多，SQNR 就幾乎不隨音量變**。

```
μ-law（北美、日本）  y = sign(x) · ln(1 + μ|x|) ／ ln(1 + μ)        μ = 255，x ∈ [−1, 1]
A-law（歐洲、台灣）   y = sign(x) · A|x| ／ (1 + ln A)                |x| < 1／A
                      y = sign(x) · (1 + ln(A|x|)) ／ (1 + ln A)     其餘，A = 87.6
```

G.711 把這條曲線用 8 段折線近似（每段 16 階），所以可以用查表做，一個 sample 幾個 cycle；13–14 bits 的線性 PCM 壓成 8 bits，**壓縮率 0.57–0.62，而且沒有延遲、沒有 frame**。這是第 3 週 2.3 講過的「非均勻量化」，也是期中考範例選擇題那個「G.711 是把 13–14 bits 壓成 8 bits」的出處。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 examples/mulaw_demo.py` | `python examples\mulaw_demo.py` |
| `python3 examples/mulaw_demo.py ../wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `python examples\mulaw_demo.py ..\wk07_1019_audio-pcm\data\speech_osr_16k.wav` |

實測（真實語音）：

| 音量 | 線性 8-bit | μ-law 8-bit |
|---|---|---|
| 0 dBFS | 35.0 dB | 37.9 dB |
| −20 dBFS | 15.6 dB | 36.5 dB |
| −40 dBFS | 1.4 dB | 26.5 dB |

線性 8-bit 每小 10 dB 就掉 10 dB；μ-law 在 40 dB 的音量範圍內只掉 11 dB。代價：滿刻度弦波時 μ-law（36 dB）比線性 8-bit（48 dB）差——它把「大聲時的精準」換成「小聲時還能聽」。

**和 Team 2 的關係**：11/1 起規格新增**功能 4：`--codec mulaw` 必做**（封包 type 0x04、一包 320 bytes、位元率減半，驗收第 10 項看 SQNR ≥ 30 dB）；ADPCM 是加分。做法就是今天這一節：編碼一個查表（或這條公式）、解碼反過來；PLC 在解碼後的 16-bit 上做，完全不用改。

### 2.3 預測編碼：DPCM／ADPCM，只送「和上一個差多少」（22 分鐘）

語音是低頻為主的訊號（第 7 週 1.3：F₀ 100–250 Hz、能量集中在 4 kHz 以下），**相鄰的 sample 很像**。那就不要送 x[n]，送 **殘差 e[n] = x[n] − x̂[n]**，x̂[n] 是用前面的 sample 預測的值：

```
1 階：x̂[n] = x[n−1]                       （「跟上一個一樣」）
2 階：x̂[n] = 2x[n−1] − x[n−2]             （「照上一段的斜率直線外插」）
p 階 LPC：x̂[n] = Σ_{k=1}^{p} a_k·x[n−k]   （係數 a_k 每 20 ms 由訊號算一次——這就是下一節的 LPC）
```

殘差的功率比原訊號小多少（dB），叫 **prediction gain**；每小 6 dB 就能少用 1 bit 而 SQNR 不變。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 examples/dpcm_demo.py ../wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `python examples\dpcm_demo.py ..\wk07_1019_audio-pcm\data\speech_osr_16k.wav` |

實測（OSR 語音 16 kHz）：

| 送什麼 | 功率 | 熵（以 16-bit 值為符號） | prediction gain |
|---|---|---|---|
| sample 本身 | 63.0 dB | 11.78 bits/sample | — |
| 1 階殘差 | 48.0 dB | 8.72 bits/sample | 15.0 dB |
| 2 階殘差 | 43.4 dB | 7.84 bits/sample | 19.6 dB |

兩個讀法：
- **Huffman 的角度**（Team 1 的延伸）：熵從 11.8 降到 7.8 bits——同一支 Huffman 編碼器，先做預測再編，壓縮率從 0.74 變 0.49。「符號怎麼定」在聲音上就是「先不先預測」。這就是 FLAC 的原理（LPC 預測＋Rice／Huffman 編殘差，無失真，約 0.5–0.6）。
- **量化的角度**：殘差直接量化成 4 bits（**DPCM**），位元率 256 → 64 kb/s。程式試了四種階距：階距 256 時 SQNR 23.8 dB，太小（64）跟不上大的跳動（slope overload，9.4 dB），太大（512）小聲處粗糙（granular noise）。
  **ADPCM**（G.726，32 kb/s）讓階距跟著訊號自動調：上一個殘差大就把階距放大，小就縮小；預測器係數也邊跑邊調。

DPCM 最重要的一個細節（程式裡有註解）：**編碼端的預測器要用「解碼端重建出來的 x̂」而不是原始的 x**，否則兩邊的預測不同，誤差會一直累積。這和 Team 2 的 PLC `repeat` 是同一個問題：接收端用來「補」的，必須是它自己手上有的東西。

---

## 延伸閱讀：送參數、送感知得到的（課後讀）

### source–filter 模型與 LPC／CELP：為什麼語音 codec 都是 20 ms

第 7 週 1.3 講過：語音 = **聲源**（聲帶振動的脈衝串，週期 1/F₀；或氣音的雜訊）經過**聲道**（會變形的管子）濾波。聲道是一個 p 階的全極點濾波器，係數 a_k 由 20–30 ms 的 frame 算出（**LPC**，p ≈ 10–16）——和 2.3 的 p 階預測器是同一件事。
所以可以**不送波形，送參數**：每 20 ms 送一組 a_k＋F₀＋增益＋有聲／無聲，解碼端自己合成（LPC-10：2.4 kb/s，像機器人）。**CELP**（GSM、AMR、Opus 的 SILK）改成「聲源從一本 codebook 挑一個合成出來最像原音的向量」，只送索引。
**為什麼 20 ms**：LPC 假設 frame 內聲道不變；太短係數估不準、開銷大，太長聲道已經在變。Team 2 的 320 samples 就是語音 codec 的自然單位。

### 感知編碼：MP3／AAC 概觀

音樂沒有單一聲道模型，走另一條路：**不送人耳聽不到的部分**。每 frame（MP3：1152 sample）做 MDCT → 心理聲學模型算每個頻帶的**遮蔽門檻** → 門檻以下的粗量化、以上的細量化（雜訊藏在門檻下）→ Huffman。MP3 128 kb/s ≈ 壓縮率 0.09；是有失真壓縮，失真在量化但被遮蔽。

### 真實 codec 的 PLC、FEC、DTX

| 技術 | 做法 | Team 2 對應 |
|---|---|---|
| PLC：pitch 重複（G.711 App. I） | 重複上一個 pitch 週期、每包衰減、4 包後靜音 | `repeat` 的升級版；加分項「衰減」 |
| 接縫平滑 | 補的 frame 與前後做 2–4 ms cross-fade | 加分項「淡入淡出」 |
| FEC（Opus in-band） | 每包帶上一包的低品質版 | 多 20 ms 延遲、位元率多 20–30% |
| DTX／CNG | 沒人講話不送，收端合成輕微雜訊 | 「靜音」與「斷線」要分開 |
| 自適應 jitter buffer（WebRTC NetEQ） | buffer 隨量到的抖動自動調 | 報告第 2 題是手動版 |

