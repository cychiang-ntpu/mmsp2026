# 第 9 週（2026/11/2）｜期中考、語音編碼的方法（配合 Team 2）

對應：期中考、Team 2（11/8 18:00 截止、11/9 評測）
時間：13:10–16:00，三節。

> **課前準備（同學）**：`git pull`；帶耳機。用電腦教室公用電腦的同學：一開機先做 [../wk03_0921_team1-kickoff/setup_first.md](../wk03_0921_team1-kickoff/setup_first.md)。
> **跟著打指令請開 [commands.md](commands.md)**。
> 期中考範例：[midterm_sample.md](midterm_sample.md)／[詳解](midterm_sample_solutions.md)。

## 本週目標

上完課你應該能：

1. 算出未壓縮語音的位元率（16 kHz × 16 bits = 256 kb/s），並說出 Team 2 每 20 ms 一包的封包開銷。
2. 解釋 μ-law／A-law 為什麼用 8 bits 就能打電話：對數量化讓小聲時的 SQNR 不掉。
3. 解釋預測編碼（DPCM／ADPCM）：語音相鄰 sample 很像，只送殘差；prediction gain 與省下的 bits。
4. 說出 source–filter 模型與 LPC／CELP 的概念：為什麼語音 codec 都以 20 ms 為 frame（和 Team 2 的 framing 一樣）。
5. 說出 MP3／AAC 這類感知編碼的三個要素：轉換（MDCT）、心理聲學（遮蔽）、位元分配。
6. 把 Team 2 的 PLC（zero／repeat）放到真實 codec 的做法（pitch 重複、衰減、FEC）裡定位，知道加分項怎麼做。

## 時程

| 節次 | 時間 | 內容 | 教材／範例 |
|---|---|---|---|
| 1 | 13:10–14:00 | **期中考**：50 分鐘閉卷，範圍 MP1–MP5 與 Team 1 | 範例：[midterm_sample.md](midterm_sample.md) |
| 2 | 14:10–15:00 | **語音編碼（上）**：位元率與封包開銷、μ-law 壓伸、預測編碼 DPCM／ADPCM | [examples/mulaw_demo.py](examples/mulaw_demo.py)、[examples/dpcm_demo.py](examples/dpcm_demo.py) |
| 3 | 15:10–16:00 | **語音編碼（下）**：source–filter 與 LPC／CELP（為什麼 20 ms）、感知編碼（MP3／AAC）概觀、真實 codec 的 PLC 與 Team 2 的加分項；Team 2 問答 | [Team 2 規格](../../team_projects/team2_voicelink/README.md)、[第 7 週 2.3–2.4](../wk07_1019_audio-pcm/README.md#23-抖動jitter-buffer-與播放時鐘12-分鐘) |

Team 2 的實作工作坊（聽掉封包、對手端驗格式、自動驗收、buffer 掃描、live 通話）放在 [lab.md](lab.md)，由助教帶、各組自己跑；今天第三節最後 10 分鐘講怎麼用。

---

## 第一節｜期中考

13:10 準時發卷，14:00 收卷。閉卷、可帶不能上網的計算機。題型與配分同[範例考題](midterm_sample.md)，題數較少。

---

## 第二節｜語音編碼（上）：從 256 kb/s 往下壓

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

**和 Team 2 的關係**：規格固定 16-bit PCM，不要求做 μ-law；但如果你們想把位元率減半（640 → 320 bytes 一包），μ-law 是最簡單的加分項：編碼一個查表、解碼一個查表，PLC 完全不用改。

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

## 第三節｜語音編碼（下）：送參數、送感知得到的，與 Team 2 的 PLC

### 3.1 source–filter 模型與 LPC／CELP：為什麼語音 codec 都是 20 ms（15 分鐘）

第 7 週 1.3 講過：語音 = **聲源**（聲帶振動的脈衝串，週期 1/F₀；或氣音的雜訊）經過**聲道**（口腔、鼻腔這根會變形的管子）濾波。數學上聲道就是一個 p 階的全極點濾波器，係數 a_k 由 20–30 ms 的 frame 用最小平方法算出來（**LPC**，linear predictive coding，p ≈ 10–16）——和 2.3 的 p 階預測器是同一件事。

所以可以**不送波形，送參數**：每 20 ms 送一組 a_k（10 個數）＋ F₀ ＋ 增益 ＋ 有聲／無聲，解碼端自己合成。1980 年代的 LPC-10 只要 2.4 kb/s，但聽起來像機器人；**CELP**（GSM、AMR、Opus 的 SILK 模式）改成「聲源用一本 codebook 裡的雜訊向量，挑一個合成出來最像原音的」——codebook 搜尋就是 Team 1 的 codebook 概念反過來用：兩邊事先約定同一本，只送索引。

**為什麼 20 ms**：LPC 假設 frame 內聲道形狀不變（準週期，1.3 節）；太短（< 10 ms）係數估不準、開銷大，太長（> 40 ms）聲道已經在變。Team 2 的 320 samples = 20 ms 不是隨便挑的：**它就是語音 codec 的自然單位**，Opus、AMR、G.729 都用 20 ms（或它的倍數／分數）。掉一包＝掉一個 LPC frame，接收端有上一組參數可以「延續」——這是真實 PLC 比 `repeat` 好的原因。

### 3.2 感知編碼：MP3／AAC 概觀（12 分鐘）

音樂不能用 LPC（沒有單一聲道模型），走另一條路：**不送人耳聽不到的部分**。

| 要素 | 做什麼 | 課程裡對應的東西 |
|---|---|---|
| 轉換 | 每 frame（MP3：1152 sample，約 26 ms）做 MDCT，得到幾百個頻帶的係數 | 第 7 週的 STFT；MDCT 是重疊 50%、可以完美重建的版本 |
| 心理聲學模型 | 算每個頻帶的**遮蔽門檻**：大聲的成分旁邊、之後幾十 ms 內，比門檻小的聲音人耳聽不到（第 7 週 1.2 的等響曲線是最簡單的門檻） | — |
| 位元分配與量化 | 每個頻帶依「門檻以上有多少」決定量化階距：聽不到的粗、聽得到的細；量化雜訊被藏在遮蔽門檻之下 | 第 3 週的量化，但階距每個頻帶、每個 frame 不同 |
| 熵編碼 | 量化後的係數用 Huffman（MP3 有 32 張表） | MP4、Team 1 |
| 位元率 | MP3 128 kb/s ≈ 0.09；AAC 同品質更低；Opus 音樂 64 kb/s | — |

這是**有失真**壓縮：失真發生在量化（第 3 週 1.1 說的），但量化雜訊的「形狀」被心理聲學模型控制，讓它聽不見。

### 3.3 真實 codec 的 PLC、FEC、DTX：Team 2 的加分項從哪裡來（13 分鐘）

| 技術 | 做法 | Team 2 對應 |
|---|---|---|
| **PLC：pitch 重複**（G.711 Appendix I） | 掉包時不是重複整個 20 ms，而是重複**上一個 pitch 週期**（用 1.3 的自相關找 T₀），波形連續；連續掉包時每包**衰減**（×0.5–0.8），4 包後靜音 | `repeat` 的升級版；規格加分項「衰減」就是這裡 |
| **接縫平滑** | 補上去的 frame 和前後真實 frame 之間做 2–4 ms 的淡入淡出（cross-fade），避免 click | 加分項「2 ms 淡入淡出」 |
| **FEC（前向錯誤更正）**（Opus in-band FEC） | 每包順便帶**上一包的低品質版本**：掉一包，下一包到了就能補回大致的內容 | 要多 20 ms 延遲（等下一包）；位元率多 20–30%；可以當進階加分 |
| **DTX／CNG**（不連續傳輸、舒適雜訊） | 沒人講話時不送包（只送很少的背景雜訊參數），接收端自己合成輕微的雜訊，否則對方以為斷線 | 你們的 recv 遇到沒有封包 = 逾時結束；真實系統要分「靜音」和「斷線」 |
| **jitter buffer 自適應**（WebRTC NetEQ） | buffer 大小隨量到的抖動自動調，太多 late 就放大、一直沒 late 就縮小；調整時用時間伸縮（加速／放慢播放）而不是跳 | 規格的 `--buffer` 是固定的；報告第 2 題「建議多少 ms」就是在手動做這件事 |

**把今天的方法放回 Team 2 的管線**：

```
麥克風 → [μ-law／ADPCM／CELP：source coding，壓到 8–64 kb/s] → 20 ms 封包（seq、ts）→ UDP
      ← [PLC：補；FEC：用冗餘救] ← jitter buffer ← 收
```

你們做的是中間沒有 codec 的版本（PCM 直傳）；codec 放進去之後 header、seq／ts、buffer、PLC 的位置都不變——這就是第 3 週 1.1 說的 source coding 和 channel／transport 可以分開設計。

### 3.4 Team 2 問答與接下來兩週的安排（10 分鐘）

- 規格的問題現場問；評測流程見 [lab.md](lab.md) 的簽核表（mock 用 `run_mock_tests.py`、live 用熱點）。
- **11/9、11/16 老師出國**：安排見課堂公告與 Slack（評測與 Team 3 開題的方式）。

---

## 範例程式（examples/）

| 檔案 | 用途 | 執行 |
|---|---|---|
| [examples/mulaw_demo.py](examples/mulaw_demo.py) | μ-law 8-bit 與線性 8-bit 在不同音量下的 SQNR（2.2） | `python3 mulaw_demo.py [檔案.wav]` |
| [examples/dpcm_demo.py](examples/dpcm_demo.py) | 1、2 階預測的 prediction gain 與熵；4-bit DPCM 的 SQNR（2.3） | `python3 dpcm_demo.py 檔案.wav` |
| [lab.md](lab.md) | Team 2 實作工作坊（助教帶） | — |

只用標準函式庫。

## 回家作業／下週前要做的事

1. **Team 2**：照 [lab.md](lab.md) 的簽核表把驗收 0–7 跑過；11/8（日）18:00 登錄 SHA。
2. 想一想（口試會問）：如果把 μ-law 加進你們的管線，封包格式哪裡要改？PLC 要不要改？位元率變多少？
3. 11/9 的評測方式見 Slack 公告。

## 參考

- ITU-T G.711（1988）：<https://www.itu.int/rec/T-REC-G.711>——μ-law／A-law；Appendix I（1999）：PLC。
- ITU-T G.726（1990），"40, 32, 24, 16 kbit/s Adaptive Differential Pulse Code Modulation (ADPCM)"：<https://www.itu.int/rec/T-REC-G.726>。
- J.-M. Valin, K. Vos, T. Terriberry, "Definition of the Opus Audio Codec," RFC 6716, Sept. 2012：<https://www.rfc-editor.org/rfc/rfc6716>（SILK＝LPC／CELP 路線、CELT＝MDCT 路線、in-band FEC、PLC）。
- M. R. Schroeder and B. S. Atal, "Code-excited linear prediction (CELP): High-quality speech at very low bit rates," *Proc. ICASSP*, 1985, pp. 937–940. [doi:10.1109/ICASSP.1985.1168147](https://doi.org/10.1109/ICASSP.1985.1168147)。
- K. Brandenburg, "MP3 and AAC Explained," *AES 17th International Conference on High-Quality Audio Coding*, 1999：<https://www.aes.org/e-lib/browse.cfm?elib=8079>——感知編碼的三要素。
- X. Huang, A. Acero, H.-W. Hon, *Spoken Language Processing*, Prentice Hall, 2001, ch. 7（speech coding：PCM、ADPCM、LPC、CELP）。
- 第 3 週 2.3–2.4（量化、SQNR）、第 7 週 1.3（F₀、source–filter）、2.3–2.4（jitter buffer、PLC）。
