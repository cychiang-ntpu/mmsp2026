# 第 9 週（2026/11/2）｜期中考、語音編碼的方法（配合 Team 2）

對應：期中考、Team 2（**11/22 18:00 截止、11/23 評測**——因 11/9、11/16 停課各順延兩週）
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
6. 說出 Team 2 的四個核心：為什麼 UDP、封包格式（seq／ts／ssrc）、jitter buffer 與播放時鐘、PLC（zero／repeat）——這些 10/19 沒講到，今天第三節補上。

## 時程

| 節次 | 時間 | 內容 | 教材／範例 |
|---|---|---|---|
| 1 | 13:10–14:00 | **期中考**：50 分鐘閉卷，範圍 MP1–MP5 與 Team 1 | 範例：[midterm_sample.md](midterm_sample.md) |
| 2 | 14:10–15:00 | **語音編碼（上）**：位元率與封包開銷、μ-law 壓伸、預測編碼 DPCM／ADPCM | [examples/mulaw_demo.py](examples/mulaw_demo.py)、[examples/dpcm_demo.py](examples/dpcm_demo.py) |
| 3 | 15:10–16:00 | **Team 2 的核心（10/19 沒講到的部分）**：為什麼 UDP、封包格式、jitter buffer 與播放時鐘、PLC、新功能 4（μ-law）、兩週討論會與評測怎麼跑 | [Team 2 規格](../../team_projects/team2_voicelink/README.md)、[jitter_buffer.html](../wk07_1019_audio-pcm/slides/jitter_buffer.html)、[jitter_sim.py](../wk07_1019_audio-pcm/examples/jitter_sim.py) |

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

## 第三節｜Team 2 的核心：不等、會掉、要補（10/19 沒講到的部分）

> 第 7 週講義第二節（2.1–2.6）有完整版；今天把實作必須知道的四件事講完，剩下的課後讀。

### 3.1 為什麼不用 TCP（5 分鐘）

| | TCP（Team 1） | UDP（Team 2） |
|---|---|---|
| 掉包 | 重傳，後面的**全部等** | 掉了就掉，**從不等** |
| 邊界 | byte stream，要自己加長度前綴 | 一個 `sendto` 就是一個封包 |
| 順序 | 保證 | 不保證，可能亂序、重複 |

語音每 20 ms 一包：掉一包少 20 ms 的聲音，補一下幾乎聽不出；等重傳一次 RTT（50–200 ms）後面全部延後，而且延遲會累積。**即時媒體寧可掉，不要等**——這是整個 Team 2 的設計原則。

### 3.2 封包格式：seq、ts、ssrc 各回答一個問題（8 分鐘）

20 ms = 320 samples = 640 bytes（16-bit PCM），加 12 bytes header：

```
magic 'V' | type | seq(2) | ts(4) | ssrc(4) | 640 bytes PCM        多 byte 欄位 big-endian
```

| 欄位 | 回答的問題 | 用途 |
|---|---|---|
| `seq`（每包 +1，16-bit 會繞回） | 有沒有**掉**、**亂序**、**重複** | 偵測與計數 |
| `ts`（單位 sample，每包 +320） | 這包**什麼時候該播** | 對齊播放時鐘 |
| `ssrc`（隨機） | 這是**哪一次**的串流 | 對方重啟就重置 |

這是 RTP（RFC 3550）的精簡版。`type` 今天多了 0x04 μ-law（320 bytes）、0x05 ADPCM（160 bytes）——功能 4，見 3.5。

### 3.3 jitter buffer 與播放時鐘（15 分鐘）

送端每 20 ms 一包；網路送到的時間不規則：有快有慢（**jitter**）、不見（**loss**）、超車（**reorder**）、兩份（**dup**）。喇叭卻**必須**每 20 ms 拿到一個 frame，否則卡住。

```
到達（不規則）──▶ [ jitter buffer：依 seq 排序 ] ──▶ 每 20 ms 拿「下一個 seq」──▶ 喇叭
                                                       │ 沒有？→ PLC 補一個（concealed）
                                                       │ 來晚了？→ 丟（late）
```

規則（規格「jitter buffer 與播放時鐘」）：
1. 播放時鐘**自己走**，每 20 ms 一拍，不等網路。
2. 先存到 `--buffer` ms（預設 60 = 3 個 frame）才開始播。
3. 每一拍：buffer 裡有「下一個該播的 seq」→ 播；沒有 → **PLC 補一個**（計 `concealed`）；**seq 一定前進**。
4. 遲到的（它的拍子已經過了）→ 丟（`late`）；重複 → 丟（`dup`）；亂序但來得及 → 排回正確位置（`reordered`）。
5. buffer 有上限（建議 4 倍），滿了丟最舊（`overflow`）。

打開 [jitter_buffer.html](../wk07_1019_audio-pcm/slides/jitter_buffer.html)：拉 jitter 30 ms、loss 5%，看封包飛、buffer 排隊、時鐘一拍一拍走；把 buffer 從 0 拉到 200 看 `late`／`concealed` 下降、延遲上升；按「sweep」畫曲線。離線版：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../wk07_1019_audio-pcm/examples/jitter_sim.py --jitter 30 --loss 0.05` | `python ..\wk07_1019_audio-pcm\examples\jitter_sim.py --jitter 30 --loss 0.05` |

預設參數的結果：buffer 0 ms 時 late 288、concealed 513；40 ms 時 13／77；**60 ms 起 late = 0、concealed = 64 = 真正遺失的數目**，之後每多 20 ms 只是延遲多 20 ms。

**取捨**：buffer 越大越少 late／concealed，但延遲越高；G.114 說單向 150 ms 以內通話品質好。報告第 2 題「建議多少 ms」就是依量到的抖動選一個值。

### 3.4 PLC：封包掉了，喇叭不能停（7 分鐘）

**PLC = packet loss concealment（遺失補償）**：第 3 步「沒有就補一個」補的那個 frame 怎麼做。規格要求兩種都做：

| 方法 | 做法 | 聽起來 | 問題 |
|---|---|---|---|
| `zero` | 補 320 個 0 | 「喀」一聲斷音 | 波形突然跳到 0 → click |
| `repeat` | 重複上一個播出去的 frame | 單掉一包幾乎聽不出 | 連續掉好幾包 → 同一個 20 ms 一直重複，「嗡」的機械音 |

注意 repeat 重複的是「**收端手上已經有的**」東西（上一個播出去的 frame），這和 2.3 DPCM「預測器要用解碼端重建的值」是同一個道理。
真的 codec（G.711 Appendix I、Opus）是重複**上一個 pitch 週期**並逐包衰減、接縫做 2 ms 淡入淡出——第 7 週 1.3 的 F₀ 就用在這裡；規格的加分項「衰減」「淡入淡出」就是這兩個。更進階的 FEC（下一包帶上一包的低品質版）、DTX／CNG（沒人講話不送）、自適應 buffer（WebRTC NetEQ）見第 7 週 2.4 與本講義課後讀的 3.6。

### 3.5 新功能 4：μ-law 壓縮（必做）、ADPCM（加分）（10 分鐘）

今天第二節剛教完，直接用：`--codec mulaw` 把每個 sample 16-bit → 8-bit（2.2 的公式或查表），一包 320 bytes、位元率減半，封包 `type` = 0x04；收端解回 16-bit 再進 jitter buffer，**PLC 不用改**。驗收第 10 項：沒有搗蛋時輸出對原檔 SQNR ≥ 30 dB（今天 `mulaw_demo.py` 實測 36–38 dB）、`bytes` 減半。
ADPCM（2.3 的做法，4 bits、160 bytes、type 0x05）是加分：每個 frame 獨立編碼，掉一包預測器才不會失步——報告第 5 題要討論。
對手端 `python_ref/voicelink.py send --codec mulaw` 已支援，先拿它確認你們的解碼對不對。

### 3.6 接下來三週怎麼跑（5 分鐘）

- **11/9 進度討論會 ①**（助教主持）：各組 4 分鐘進度報告、組間**格式互測**（收端開 `--probe`）、問題診所。交進度報告 ①（11/15）。
- **11/16 進度討論會 ②**：自動驗收互測（`run_mock_tests.py` 四種組合）、live 預演、buffer 掃描當場畫圖。交進度報告 ②＋小成品（11/22 與 SHA 一起）。
- **11/22（日）18:00 截止；11/23 評測**：mock 用老師的 WAV 跑 `run_mock_tests.py` 兩個方向、live 用熱點；評測前老師先回答兩次討論會收集的問題。
- 工作坊的指令與簽核表在 [lab.md](lab.md)；今天回家先把 lab.md 的 2.1「聽掉封包」跑一次。

課後讀（原本第三節的內容，期末會用到）：第 7 週 2.5–2.6（baseline 導覽）、以及下面的「延伸：LPC／CELP 與感知編碼」。

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

## 範例程式（examples/）

| 檔案 | 用途 | 執行 |
|---|---|---|
| [examples/mulaw_demo.py](examples/mulaw_demo.py) | μ-law 8-bit 與線性 8-bit 在不同音量下的 SQNR（2.2） | `python3 mulaw_demo.py [檔案.wav]` |
| [examples/dpcm_demo.py](examples/dpcm_demo.py) | 1、2 階預測的 prediction gain 與熵；4-bit DPCM 的 SQNR（2.3） | `python3 dpcm_demo.py 檔案.wav` |
| [../wk07_1019_audio-pcm/slides/jitter_buffer.html](../wk07_1019_audio-pcm/slides/jitter_buffer.html)、[../wk07_1019_audio-pcm/examples/jitter_sim.py](../wk07_1019_audio-pcm/examples/jitter_sim.py) | jitter buffer 的動畫與離線模擬（3.3） | 瀏覽器開啟；`python3 jitter_sim.py --jitter 30 --loss 0.05` |
| [lab.md](lab.md) | Team 2 實作工作坊（11/9、11/16 討論會與自己互測用） | — |

只用標準函式庫。

## 回家作業／下週前要做的事

1. **Team 2**：照 [lab.md](lab.md) 的簽核表把驗收 0–7 跑過；**11/22（日）18:00** 登錄 SHA。
2. **Team 2 新增功能 4（μ-law，必做）**：封包 type 0x04、320 bytes；先用 `mulaw_demo.py` 的公式做出來，用 `python_ref/voicelink.py send --codec mulaw` 當對手端確認 SQNR ≥ 30 dB。
3. 11/9、11/16 Team 2 進度討論會 ①②（見各週講義）；11/23 評測（mock 用 `run_mock_tests.py` 加老師的 WAV、live 用熱點）。

## 參考

- ITU-T G.711（1988）：<https://www.itu.int/rec/T-REC-G.711>——μ-law／A-law；Appendix I（1999）：PLC。
- ITU-T G.726（1990），"40, 32, 24, 16 kbit/s Adaptive Differential Pulse Code Modulation (ADPCM)"：<https://www.itu.int/rec/T-REC-G.726>。
- J.-M. Valin, K. Vos, T. Terriberry, "Definition of the Opus Audio Codec," RFC 6716, Sept. 2012：<https://www.rfc-editor.org/rfc/rfc6716>（SILK＝LPC／CELP 路線、CELT＝MDCT 路線、in-band FEC、PLC）。
- M. R. Schroeder and B. S. Atal, "Code-excited linear prediction (CELP): High-quality speech at very low bit rates," *Proc. ICASSP*, 1985, pp. 937–940. [doi:10.1109/ICASSP.1985.1168147](https://doi.org/10.1109/ICASSP.1985.1168147)。
- K. Brandenburg, "MP3 and AAC Explained," *AES 17th International Conference on High Quality Audio Coding*, Florence, Sept. 1999, pp. 99–110。作者所屬單位公開的 PDF：<https://www.ee.columbia.edu/~dpwe/papers/Brand99-mp3.pdf>——感知編碼的三要素。
- X. Huang, A. Acero, H.-W. Hon, *Spoken Language Processing*, Prentice Hall, 2001, ch. 7（speech coding：PCM、ADPCM、LPC、CELP）。
- 第 3 週 2.3–2.4（量化、SQNR）、第 7 週 1.3（F₀、source–filter）、2.3–2.4（jitter buffer、PLC）。
