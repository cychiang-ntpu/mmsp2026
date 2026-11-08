# 第 7 週（2026/10/19）｜聲音的時頻分析（STFT、MP5）、Team 2 開題（即時語音）、補驗時段 1

對應：MP5、Team 2
時間：13:10–16:00，三節。

> **用電腦教室公用電腦的同學：一開機先做 [../wk03_0921_team1-kickoff/setup_first.md](../wk03_0921_team1-kickoff/setup_first.md)**（貼一行指令，約 5 分鐘，自動裝好 Git、gcc、make、Python 並抓下課程 repo）。
>
> **課前準備（同學）**：在 `mmsp2026` 資料夾打 `git pull`；確認 gcc、make、Python 3 還能用；**帶耳機**（第二節要聽掉封包的聲音）。
> 指令一樣分兩欄：左邊 macOS／Linux／WSL，右邊 Windows 的 VSCode PowerShell（`python3` 在 Windows 通常叫 `python`；`make` 在 Windows 叫 `mingw32-make`）。
> 本講義的指令都從本週資料夾出發：`cd lectures/wk07_1019_audio-pcm`。
>
> **上課跟著打指令，請開 [commands.md（一頁版指令表）](commands.md)**。

## 本週目標

上完課你應該能：

1. 說出語音為什麼要「切成 frame 再分析」：短時間內近似週期（F0 與 harmonics），長時間會變。
2. 寫出 DFT 的式子，說出第 k 根對應幾 Hz（k·f_s／N），並解釋 dB 為什麼用在縱軸。
3. 解釋窗函數的作用：rectangular 在 frame 兩端造成的不連續會「漏」到整個頻譜（leakage），Hamming／Hann 怎麼改善，代價是什麼。
4. 說出 STFT 的四個步驟與三個參數（P、N、M）各自控制什麼，以及「窄頻 vs 寬頻」spectrogram 的差別。
5. 照 MP5 的規格算出一個 frame 的乘法次數（DFT 對 FFT），並知道這次自動測試比對的是什麼。
6. 說出 Team 2 的三個功能、固定的封包格式、jitter buffer 的規則與 STATS 的每個欄位，以及你這一輪的角色。

## 時程

| 節次 | 時間 | 內容 | 教材／範例 |
|---|---|---|---|
| 1 | 13:10–14:00 | **聲音的時頻分析**：A/D 複習、量測與感知、語音的 frame 與週期性、DFT 複習、窗函數與 leakage、STFT 四步驟與 spectrogram、MP5 講解 | [slides/stft_steps.html](slides/stft_steps.html)、[examples/frame_demo.py](examples/frame_demo.py)、[examples/window_demo.py](examples/window_demo.py)、[examples/stft_demo.py](examples/stft_demo.py)、[data/aeueo-16kHz.wav](data/aeueo-16kHz.wav) |
| 2 | 14:10–15:00 | **Team 2 開題：即時語音**：為什麼 UDP、20 ms framing 與封包格式、jitter buffer 與播放時鐘、遺失補償、baseline 導覽（用耳朵聽掉封包）、量測與驗收 | [Team 2 規格](../../team_projects/team2_voicelink/README.md)、[baseline/](../../team_projects/team2_voicelink/baseline/)、[slides/jitter_buffer.html](slides/jitter_buffer.html)、[examples/jitter_sim.py](examples/jitter_sim.py) |
| 3 | 15:10–16:00 | **分組公告與補驗時段 1**：Team 2 分組與角色；Team 1 口試未達標者補驗；其他組當場建 repo、跑通 baseline、兩台電腦 UDP 互通 | [commands.md](commands.md) 第 3 節 |

---

## 第一節｜聲音的時頻分析：從一個 frame 到 spectrogram

### 1.1 複習：聲音怎麼變成數字（3 分鐘）

第 3 週講過：麥克風把氣壓變化變成電壓，A/D 轉換器**取樣**（f_s 次／秒）再**量化**（n bits），存成 PCM，包成 WAV（[第 3 週 2.1–2.6](../wk03_0921_team1-kickoff/README.md#第二節wav-與-pcm聲音怎麼變成數字)）。
本週所有的聲音都是 **16 kHz、16-bit、單聲道**：1 秒 16 000 個 sample、32 000 bytes；Team 2 的一個 20 ms frame 就是 320 個 sample、640 bytes。

今天要回答的問題是：**一串 sample 裡面「有什麼聲音」？** 這要看頻率，而語音的頻率內容隨時間一直在變，所以要「一小段一小段」看——這就是 STFT。

### 1.2 聲音的量測與感知（7 分鐘）

| 物理量（客觀，儀器量得到） | 感知量（主觀，人耳聽到的） | 關係 |
|---|---|---|
| 頻率 frequency（Hz） | 音高 pitch | 大致是**對數**：高八度＝頻率 2 倍。250 vs 260 Hz 聽得出差；1000 vs 1010 Hz 幾乎聽不出 |
| 強度／振幅（Pa、sample 值） | 響度 loudness | 也是對數：所以用 **dB**。+10 dB 聽起來「大約兩倍大聲」；3 dB 是一般人分得出的最小差別 |

- 人耳大約聽 20 Hz–20 kHz；電話只傳 300–3400 Hz（8 kHz 取樣）就聽得懂，但「聽得出是誰」要更寬（16 kHz 取樣的 0–8 kHz 常稱 wideband）。
- 數位音訊的振幅單位 **dBFS**（full scale）：16-bit 的 32768 是 0 dBFS；10 000 是 20·log10(10000/32768) ≈ −10.3 dBFS；1 是 −90.3 dBFS。
- 人耳對不同頻率的靈敏度不同（等響曲線：2–5 kHz 最靈敏），這是 MP3 心理聲學模型的根據，這學期只點到為止。

### 1.3 語音的 frame：短時間內近似週期（8 分鐘）

打開 [data/aeueo-16kHz.wav](data/aeueo-16kHz.wav)（五個母音 a-e-u-e-o，7 秒）。把其中 32 ms 放大看：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 examples/frame_demo.py data/aeueo-16kHz.wav 1.0` | `python examples/frame_demo.py data\aeueo-16kHz.wav 1.0` |

會看到波形**每隔固定的時間重複一次**：那個時間是**基本週期** T₀，倒數是**基本頻率 F₀**（男聲約 100–150 Hz、女聲約 200–250 Hz），是聲帶振動的頻率。
程式用自相關（把波形和「平移 τ 之後的自己」相乘相加，最像的 τ 就是週期）估出 F₀：t = 1.0 s 處 τ = 102 個 sample = 6.4 ms → F₀ ≈ 157 Hz；檔案中間（t ≈ 3.5 s）τ = 92 → 174 Hz。程式最後還會印出同一個 frame 接縫處的 8 個 sample：rectangular 的跳動是 4 430，Hann 是 0。

三種語音訊號：

| 類型 | 波形 | 聲帶 | 例子 |
|---|---|---|---|
| 週期 periodic | 一段時間後重複同樣的形狀 | 振動 | 母音、鼻音（[a]、[m]） |
| 非週期 aperiodic | 像雜訊，看不出規律 | 多半不振動 | 氣音、擦音（[s]、[f]、「秋」的 [tɕʰ]） |
| 準週期 quasi-periodic | 相鄰週期很像但不完全一樣 | 振動 | **大部分的語音**其實是這一種 |

重點：語音**整段**看不是週期的（一直在變），但**20–40 ms 內**近似週期。傅立葉理論說：週期訊號＝基本頻率 F₀ 的弦波＋它的整數倍（**harmonics**）相加。
所以只要把語音切成 20–40 ms 的 **frame**，每個 frame 當成週期訊號分析，就能看到 F₀ 與 harmonics——這就是 spectrogram 的出發點。
（MP3 也是切 frame：44.1 kHz 下 576 或 1152 個 sample，約 13 或 26 ms。）

### 1.4 DFT 複習：第 k 根是幾 Hz（8 分鐘）

N 點的離散傅立葉轉換（DFT）：

```
X[k] = Σ_{n=0}^{N−1} x[n] · e^{−j2πkn/N}        k = 0, 1, …, N−1
```

- 把 x[n] 拆成 N 個「複數弦波」的和；**X[k] 的大小 |X[k]| 是第 k 個頻率成分的振幅，角度是相位**。
- 第 k 根對應的實際頻率：**f_k = k · f_s ／ N**（Hz）。相鄰兩根差 f_s／N，叫**頻率解析度**。例：f_s = 16 000、N = 512 → 每根 31.25 Hz。
- 只有 k = 0 … N/2 有意義（實數訊號的頻譜左右對稱），最高就是 f_s／2——又回到 Nyquist。
- 縱軸畫 **20·log10|X[k]|**（dB）：人耳對大小聲是對數感覺，而且語音的 harmonics 振幅差好幾個數量級，用線性軸只看得到最大的一根。
- 直接照式子算：每個 k 要 N 次複數乘加，N 個 k 共 **N² 次**；**FFT** 用 N 是 2 的次方的對稱性把它變成 **(N／2)·log₂N 次乘法**。N = 512：262 144 對 2 304，差 100 倍以上。MP5 報告要你算這個。

### 1.5 窗函數：為什麼不能直接「剪」一段（8 分鐘）

DFT 其實是把這 N 點**當作一個週期為 N 的週期訊號**來分析（x[n] 的定義域延伸到全部整數時，變成 x[((n))_N]）。
所以用 rectangular window 直接剪一段弦波，接起來的**接縫處會不連續**（除非剛好剪到整數個週期）——這個「突然跳一下」不是原訊號有的，但 DFT 會老實地把它算進去，結果是能量**漏到所有頻率**（spectral leakage）：本來該是一根的頻譜變成一座山加很多小山（side lobes）。

```
w[n]（0 ≤ n < P，其餘為 0）
rectangular : w[n] = 1
Hamming     : w[n] = 0.54 − 0.46·cos(2πn／(P−1))
Hann        : w[n] = 0.50 − 0.50·cos(2πn／(P−1))
```

Hamming／Hann 把 frame 兩端**平滑地壓到接近 0**，接縫就不跳了。代價：**主瓣變寬**（頻率解析度變差，約 2 倍），換來 **side lobe 低很多**（rectangular 約 −13 dB，Hamming 約 −41 dB，Hann 約 −31 dB 但衰減得快）。語音分析慣用 Hamming。

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 examples/window_demo.py` | `python examples/window_demo.py` |

程式用 170 Hz 的弦波（8 kHz 取樣、256 點）印出三種窗的主瓣寬度與最高 side lobe：rectangular 主瓣 2 根、side lobe −12 dB；Hann 4 根、−31.5 dB；Hamming 4.5 根、−37.5 dB（170 Hz 不在整數根上，所以比教科書的 −41 dB 高一點；換成 2 000 Hz 就是 −42.6 dB）；並示範**頻率剛好落在整數根上**（187.5 Hz = 6 根 × 31.25 Hz）與**落在兩根之間**（170 Hz）的差別——後者連 rectangular 都會嚴重 leakage。
（為什麼只分析一個弦波？因為一般訊號是許多弦波的疊加，頻譜也是各弦波頻譜的疊加。）

### 1.6 STFT 四步驟與 spectrogram（10 分鐘）

打開 [slides/stft_steps.html](slides/stft_steps.html)（用 ← → 走），四個步驟：

| 步驟 | 做什麼 | 參數 | 數學 |
|---|---|---|---|
| 1 Framing | 每隔 M 個 sample 切一段長 P 的 frame | P（視窗長）、M（hop） | x_m[n] = x[n + mM]，0 ≤ n < P |
| 2 Windowing | 乘上窗函數 | 窗型 | x_m[n]·w[n] |
| 3 Zero padding | 後面補 0 到長度 N（N ≥ P，通常是 2 的次方） | N（DFT 長） | 多出來的根是**內插**，沒有新資訊，但曲線比較平滑 |
| 4 DFT | 每個 frame 做 N 點 DFT，取 \|X_m[k]\|，轉 dB | — | 一個 frame 一列（k = 0 … N/2−1）|

把每一列垂直排、依時間（m）橫著放，用亮度畫 dB，就是 **spectrogram**（聲紋圖）：橫軸時間、縱軸頻率、亮度能量。

三個參數各控制什麼：

- **P 決定頻率解析度**（主瓣寬 ∝ f_s／P，不是 N！）：
  - **窄頻 narrow-band**（P ≈ 32 ms，16 kHz 下 512 點）：解析度 31 Hz，看得到一條一條的 **harmonics**（F₀ 的整數倍），但時間糊掉。
  - **寬頻 wide-band**（P ≈ 4 ms，64 點）：解析度 250 Hz，harmonics 糊成一片，但看得到**一根一根的 pitch pulse**（每個聲帶週期一條直線）和清楚的 **formant**（共振峰）帶。
- **N 只決定「畫幾根」**：N > P 是內插，讓頻譜看起來平滑；不會提高解析度。
- **M 決定時間解析度**：越小越細，但 frame 數越多、算得越慢。慣例 M ≤ P，讓 frame 重疊。
- 慣用值：語音 P = 4–40 ms、M = 5–20 ms、N = 2^⌈log₂P⌉。

在頁面上把 P 從 32 ms 切到 4 ms，看 harmonics 消失、pitch pulse 出現——這是「時間與頻率解析度互相牴觸」最直接的體驗（測不準原理的訊號版）。

命令列版（不用 numpy，自己寫的 FFT）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 examples/stft_demo.py data/aeueo-16kHz.wav --ascii` | `python examples/stft_demo.py data\aeueo-16kHz.wav --ascii` |
| `python3 examples/stft_demo.py data/aeueo-16kHz.wav --w_ms 4 --dft_ms 4 --ascii` | `python examples/stft_demo.py data\aeueo-16kHz.wav --w_ms 4 --dft_ms 4 --ascii` |

它會印出 P、N、M 換算成 sample 的數字、頻率解析度、frame 數、DFT 與 FFT 的乘法次數，並把頻譜寫成與 MP5 相同格式的文字檔。

### 1.7 MP5 講解：spectrogram（6 分鐘）

題目（去年規格，今年的差異見下）：<https://hackmd.io/@ntpu-ce-mmsp/mmsp-2025-mini-project-5>；樣本與評語：[samples_2025-C/mini_project_5](../../samples_2025-C/mini_project_5/README.md)；
**正式參考答案**（Python）：[samples_2025-python/mini_project_5/spectrogram.py](../../samples_2025-python/mini_project_5/spectrogram.py)。

今年固定的介面（與參考答案相同，自動測試照這個跑）：

```
mp5 gen <fs> <out.wav>                                          產生 4 秒測試訊號（四種波形 × 10 段，振幅與頻率表在題目裡）
mp5 spec <w_ms> <hamming|rectangular> <dft_ms> <hop_ms> <in.wav> <out.txt>
```

| 項目 | 規定 |
|---|---|
| 參數換算 | P = f_s·w_ms／1000、N = f_s·dft_ms／1000、M = f_s·hop_ms／1000（整數除法） |
| frame 範圍 | start = 0, M, 2M, …，只要 start + P ≤ 總長（最後不足一個 frame 的不算） |
| 輸出 | 一個 frame 一列、空白分隔、**線性的 \|X[k]\|**（不是 dB；畫圖時再轉）、k = 0 … N/2−1、`%f` |
| 自動測試 | `gen` 的 WAV **逐 byte 相同**；`spec` 的數字在容差內（相對 1e-4、絕對 1e-2；見 [tools/ci/run_tests.sh](../../tools/ci/run_tests.sh) 的 MP5 段與 `tools/ci/tests/mp5_cases.txt`）|
| 報告（README） | 比較四種設定（32／30 ms × rectangular／Hamming；30 ms 時 N = 32 ms 就是 zero padding）；每個 frame 的乘法與加法次數（DFT 與 FFT）；分工；心得 |
| 截止 | **10/23（四）18:00**，MP1–MP5 統一截止 |

三個最常見的錯：
1. **WAV 的 `data` 不在第 44 byte**（LIST chunk）：照 chunk 走（第 3 週 2.6）。
2. **Hamming 的分母是 P−1**，不是 P；窗長是 P 不是 N。
3. 最後一個不完整的 frame：參考答案**不算**它；多算一列就比對失敗。

自己對答案：`bash <課程repo>/tools/ci/run_tests.sh mp5`（在你的作業 repo 執行；做法同第 2 週 2.4）。

---

## 第二節｜Team 2 開題：即時語音（VoiceLink）

規格全文：[team_projects/team2_voicelink/README.md](../../team_projects/team2_voicelink/README.md)。這一節講「為什麼這樣規定」。

### 2.1 為什麼不用 TCP（5 分鐘）

第 2 週的 `chat.c` 就有 UDP 模式（[baseline/chat.c](../../team_projects/team2_voicelink/baseline/chat.c) 第 5 區段 `udp_setup`）。回想兩者的差別：

| | TCP（Team 1） | UDP（Team 2） |
|---|---|---|
| 送出去會不會到 | 保證到、保證順序；掉了**重傳**，後面的**全部等** | 不保證；掉了就掉，亂序就亂序，**從不等** |
| 邊界 | byte stream，沒有訊息邊界（所以要 frame 的長度前綴） | **一個 sendto 就是一個封包**，對方一次 recvfrom 收到一整包 |
| 適合 | 檔案、文字、網頁 | 即時語音／視訊、遊戲 |

語音每 20 ms 一包。掉一包＝少 20 ms 的聲音，補一下幾乎聽不出來；但如果用 TCP 等重傳，一次 RTT（50–200 ms）後面所有聲音都延後，而且**延遲會累積**。所以：**即時媒體寧可掉，不要等**——這是整個 Team 2 的設計原則。

### 2.2 20 ms framing 與封包格式（8 分鐘）

為什麼 20 ms？
- 語音在 20 ms 內近似穩定（1.3 節），掉一包損失小；
- 一包 640 bytes，遠小於乙太網路的 1500 bytes（不會被切）；
- 每秒 50 包，header 的開銷可接受（12／652 ≈ 2%）；電話的 codec（G.711、Opus）也多用 20 ms。

封包＝12 bytes header＋640 bytes PCM（規格「封包格式」）。三個欄位各有用途，不要混：

| 欄位 | 回答的問題 | 用在 |
|---|---|---|
| `seq`（2 bytes，每包 +1） | 有沒有**掉**、有沒有**亂序**、有沒有**重複** | 偵測與計數 |
| `ts`（4 bytes，單位 sample，每包 +320） | 這包**什麼時候該播** | 播放時鐘對齊 |
| `ssrc`（4 bytes，隨機） | 這是**哪一次**的串流 | 對方重啟時重置 buffer |

這就是 RTP（RFC 3550）header 的精簡版——真的 VoIP 用的格式，你們做完就看得懂 Wireshark 裡的 RTP。
seq 是 16-bit 會**繞回**（65535 → 0）：比較「誰在前面」要用差值的正負，不能直接比大小（規格要求處理）。

### 2.3 抖動、jitter buffer 與播放時鐘（12 分鐘）

送端每 20 ms 一包，網路不會每 20 ms 一包地送到：有的快有的慢（**jitter**）、有的不見（**loss**）、有的超車（**reorder**）、有的收到兩次（**dup**）。
喇叭卻**必須**每 20 ms 拿到一個 frame，否則會「卡」（underrun，聽到喀一聲或停頓）。

解法：接收端放一個 **jitter buffer**，先存幾個 frame 再開始播，用**自己的時鐘**每 20 ms 拿一個：

```
到達（不規則）──▶ [ jitter buffer：依 seq 排序 ] ──▶ 每 20 ms 拿「下一個 seq」──▶ 喇叭
                                                       │ 沒有？→ PLC（packet loss concealment，封包遺失補償） 補一個（concealed）
                                                       │ 來晚了？→ 丟（late）
```

規則（規格「jitter buffer 與播放時鐘」）：播放時鐘**不等網路**；buffer 存到目標延遲（預設 60 ms = 3 frame）才開始；每一拍要嘛播要嘛補，**seq 一定前進**；遲到丟、重複丟、亂序排回去；有上限，滿了丟最舊。

打開 [slides/jitter_buffer.html](slides/jitter_buffer.html)：拉 jitter 到 30 ms、loss 到 5%，看封包飛、buffer 排隊、播放時鐘一拍一拍走；把 buffer 從 0 拉到 200 ms 看 `late`／`concealed` 下降、`avg_delay_ms` 上升。按「sweep」畫出曲線——這就是報告要你們自己量的圖。

離線先算一次（不用網路）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 examples/jitter_sim.py --jitter 30 --loss 0.05` | `python examples/jitter_sim.py --jitter 30 --loss 0.05` |

預設參數（1 500 個 frame、抖動 30 ms、遺失 5%、亂序 5%）的結果：buffer 0 ms 時 late 288、concealed 513；20 ms 時 45／109；40 ms 時 13／77；**60 ms 起 late = 0、concealed = 64 = 真正遺失的數目**，之後每多 20 ms 的 buffer 只是平均延遲多 20 ms（60 ms → 68 ms、200 ms → 208 ms）。

**取捨**：buffer 越大 → 越少 late／concealed，但延遲越高。ITU-T G.114 說單向延遲 150 ms 以內通話品質好，400 ms 以上就會「互相搶話」。
所以 buffer 不是越大越好，報告第 2 題要你們**依量到的抖動**選一個值並說理由。

### 2.4 遺失補償 PLC（5 分鐘）

buffer 裡沒有那個 seq 時，喇叭還是要東西。兩種最簡單的補法（規格要求都做）：

| 方法 | 做法 | 聽起來 | 問題 |
|---|---|---|---|
| zero | 補 320 個 0 | 「喀」一聲的斷音 | 波形突然跳到 0 → 高頻的 click |
| repeat | 重複上一個 frame | 單掉一包幾乎聽不出 | 連續掉好幾包 → 同一個 20 ms 一直重複，變成「嗡」的機械音 |

真的 codec（G.711 Appendix I、Opus）會用 pitch 週期去「延續」波形並逐漸衰減——那是 1.3 節 F₀ 的應用。加分項：repeat 每次 ×0.5 衰減、或接縫做 2 ms 淡入淡出。

### 2.5 baseline 導覽：先聽壞掉的（10 分鐘）

[baseline/](../../team_projects/team2_voicelink/baseline/) 給你們：
- **wrapper/**：`wav.h`（讀寫 WAV，照 chunk 走）、`udp.h`（開 socket、`sendto`／`recvfrom` 加逾時、big-endian 工具）、`clock.h`（單調時鐘、精準的 20 ms sleep）、`audio.h`（麥克風與喇叭，用 miniaudio；`audio_start_duplex(callback)` 每 320 個 sample 叫你一次）。
- **voicelink_raw.c**：用 wrapper 做的**最天真**串流：640 bytes 一包、沒有 header、收到什麼寫什麼。
- **udp_impair.py**：搗蛋的中繼。

現場做（兩個視窗；教室用熱點時可以兩台電腦）：

| 步驟 | 視窗 A | 視窗 B |
|---|---|---|
| 編譯 | `cd team_projects/team2_voicelink/baseline && make` | |
| 正常 | `./voicelink_raw recv 5004 out.wav` | `./voicelink_raw send 127.0.0.1 5004 ../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` |
| 搗蛋 | `python3 udp_impair.py 5003 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1`（第三個視窗）；A 照舊 | `send 127.0.0.1 5003 …`（送給中繼） |

聽 `out.wav`：掉包的地方**整段變短**（raw 版沒有補，時間軸整個往前擠）、亂序的地方**聲音倒著播**、抖動時**忽快忽慢**。
你們要做的就是讓它在同樣的搗蛋下**長度不變、順序對、不卡**——所以要 header、seq／ts、buffer、PLC。

### 2.6 量測、驗收、時程與角色（10 分鐘）

- **STATS 一行**（規格「量測輸出」）是評測抓的東西，欄位順序與名稱不可改。`frames_out = 播出的真封包 + concealed` 這條等式要成立。
- **驗收 0–7 可以自動測**：`tests/run_mock_tests.py` 對自己跑一遍（用 Python 版當對手端也行）；評測當天用老師的 WAV 跑同一支。
- **python_ref/voicelink.py** 是 mock 模式的完整版（`--probe` 印每一拍的 buffer）：可以先用它當對手端確認你們的封包格式對不對，再兩邊都換成自己的。
- **時程**（11/1 更新）：第 1–2 週（10/19–11/1）建 repo、跑通 baseline、兩台 UDP 互通；11/2 第三節起改照 [tutorial](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 七步做；11/9、11/16 工作坊（助教主持，每步 push）；**11/22（日）18:00 登錄 SHA**；11/23 評測。驗收已簡化為三層，見規格。
- **角色**：P 報告；D×2 一位網路與 buffer、一位音訊與 live；V 測試與量測。每個人都要有 C 的 commit。

---

## 第三節｜分組公告與補驗時段 1

### 3.1 Team 2 分組與角色（5 分鐘）

名單課堂投影（只有組別、學號、姓名、角色）。四人組 P＋D＋D＋V、三人組 P＋D＋V；每人三輪各當一次 P、D、V。
同組想換角色今天當場登記。下課前每組要完成：互留聯絡方式、決定誰建 team repo（`mmsp2026-team2-gXX`，private，加老師與助教為 collaborator）、約好第一次討論時間。

### 3.2 補驗時段 1（45 分鐘）

- **Team 1 口試未達 15/30 的同學**：依公告順序到前面補驗一次（補驗後該次上限 59）。
- **其他同學**（分組後立刻開始，老師與助教在教室巡）：
  1. 建 team repo，把 baseline 複製進去，`make` 編得過。
  2. 跑 2.5 的「正常」與「搗蛋」各一次，把 `out.wav` 聽一遍。
  3. **兩台電腦用熱點互傳**一次（`recv` 的那台先開防火牆允許）。
  4. 在 `docs/interface.md` 寫下封包格式（可以直接照規格抄），`TEAM_LOG.md` 記今天的出席與分工。

---

## 範例程式（examples/、slides/、data/）

| 檔案 | 用途 | 執行 |
|---|---|---|
| [commands.md](commands.md) | **上課跟著打的指令（一頁版）** | 上課時開著 |
| [slides/stft_steps.html](slides/stft_steps.html) | STFT 四步驟的互動動畫：framing → windowing → zero padding → DFT → spectrogram；可切窄頻／寬頻、三種窗 | 瀏覽器開啟；鍵盤 ← → |
| [slides/jitter_buffer.html](slides/jitter_buffer.html) | jitter buffer 動畫：封包飛行、buffer 排序、播放時鐘、STATS 計數器、buffer 掃描圖 | 瀏覽器開啟 |
| [slides/samples.js](slides/samples.js)、[examples/make_samples_js.py](examples/make_samples_js.py) | 網頁內建的母音檔（base64）與產生它的腳本 | `python3 examples/make_samples_js.py` |
| [examples/frame_demo.py](examples/frame_demo.py) | 32 ms 的語音 frame、自相關估 F₀、接縫的不連續 | `python3 frame_demo.py 檔案.wav [秒]` |
| [examples/window_demo.py](examples/window_demo.py) | 三種窗的主瓣寬與 side lobe；bin-centred 與否的 leakage | `python3 window_demo.py` |
| [examples/stft_demo.py](examples/stft_demo.py) | 自己寫的 FFT 做 STFT，輸出 MP5 格式；`--ascii` 印文字版 spectrogram | `python3 stft_demo.py 檔案.wav [--w_ms 32 --dft_ms 32 --hop_ms 10 --win hamming --ascii]` |
| [examples/jitter_sim.py](examples/jitter_sim.py) | 離線模擬 Team 2 接收端：buffer 大小 vs late／concealed／延遲 | `python3 jitter_sim.py --jitter 30 --loss 0.05` |
| [data/aeueo-16kHz.wav](data/aeueo-16kHz.wav) | 五個母音，16 kHz、16-bit、單聲道，7 秒（MP5 的測試音檔之一） | 1.3、1.6、網頁內建 |
| [data/speech_osr_16k.wav](data/speech_osr_16k.wav) | 真實語音 33.6 秒，16 kHz（由第 3 週的 8 kHz 檔升取樣，頻寬仍是 4 kHz） | Team 2 的 mock 測試 |
| [Team 2 baseline](../../team_projects/team2_voicelink/baseline/) | wrapper、`voicelink_raw`、`udp_impair.py` | `make`；見規格 |
| [Team 2 python_ref](../../team_projects/team2_voicelink/python_ref/) | mock 模式完整版，`--probe` | `python3 voicelink.py send/recv …` |

Python 的示範程式只用標準函式庫（MP5 的參考實作 spectrogram.py 才需要 numpy）。

## 回家作業／下週前要做的事

1. **MP5**：先用參考答案產生 16 個輸出看格式，再寫 C；用 `run_tests.sh mp5` 對答案。**10/23 18:00 與 MP1–MP4 一起截止。**
2. **手算一次**：f_s = 16 000、P = 400（25 ms）、N = 512、M = 160：頻率解析度幾 Hz？一個 frame 的 DFT 要幾次乘法、FFT 幾次？1 秒的語音有幾個 frame？（答案在 `stft_demo.py` 的輸出裡）
3. **Team 2 第 1 週目標**：建 repo、跑通 baseline、**兩台電腦 UDP 互通**、`docs/interface.md` 初稿、`TEAM_LOG.md`。
4. 10/26 停課；11/2 第一節期中考（範圍到本週），第二、三節語音實驗。

## 素材來源與授權

| 檔案 | 來源 | 授權 |
|---|---|---|
| `data/aeueo-16kHz.wav` | 本課程 MP5 的測試音檔（授課教師錄製），五個母音 | 課程教材 |
| `data/speech_osr_16k.wav` | **Open Speech Repository** 的 `OSR_us_000_0010_8k.wav`（第 3 週的 `speech_osr_8k.wav`）用 ffmpeg 升取樣到 16 kHz（`-ar 16000`），內容未變 | 可自由使用，**須註明來源為「Open Speech Repository」**（聲明全文見[第 3 週 data/README.md](../wk03_0921_team1-kickoff/data/README.md)） |
| `team_projects/team2_voicelink/baseline/wrapper/miniaudio.h` | David Reid, miniaudio，<https://miniaud.io/> | Public domain／MIT No Attribution（擇一） |

## 參考

- 江振宇，DSP 2026 講義〈第三部分：語音信號的表示〉1.3–1.6（DFT 的頻率、窗函數、spectrogram 的三步驟、音符序列的例子）：
  <https://github.com/cychiang-ntpu/dsp2026/blob/master/docs/lectures/day03_speech_signal_representation.md>——本講義 1.3–1.6 的敘述出處。
- 2023 講義 [Chapter 3 Digital Audio Representation](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023/tree/master/Chapter-3)（聲音的來源、量測與感知、DFT 複習；含上課影片）——本講義 1.2 的出處。
- X. Huang, A. Acero, H.-W. Hon, *Spoken Language Processing*, Prentice Hall, 2001, pp. 274–281（STFT、窗函數、spectrogram；MP5 題目的參考書）。
- A. V. Oppenheim and R. W. Schafer, *Discrete-Time Signal Processing*, 3rd ed., Pearson, 2010, ch. 8（DFT）、ch. 10（spectral analysis with the DFT：leakage、窗、time-dependent Fourier transform）。
- F. J. Harris, "On the Use of Windows for Harmonic Analysis with the Discrete Fourier Transform," *Proceedings of the IEEE*, vol. 66, no. 1, pp. 51–83, Jan. 1978. [doi:10.1109/PROC.1978.10837](https://doi.org/10.1109/PROC.1978.10837)——各種窗的主瓣寬與 side lobe 的標準出處（1.5 的數字）。
- J. W. Cooley and J. W. Tukey, "An Algorithm for the Machine Calculation of Complex Fourier Series," *Mathematics of Computation*, vol. 19, no. 90, pp. 297–301, 1965. [doi:10.1090/S0025-5718-1965-0178586-1](https://doi.org/10.1090/S0025-5718-1965-0178586-1)——FFT。
- RFC 3550（RTP）、RFC 768（UDP）、ITU-T G.711 Appendix I（PLC）、ITU-T G.114（單向延遲）：見 [Team 2 規格的參考文獻](../../team_projects/team2_voicelink/README.md#參考文獻)。
- 本 repo：[MP5 樣本與評語](../../samples_2025-C/mini_project_5/README.md)、[第 3 週講義](../wk03_0921_team1-kickoff/README.md)（A/D、WAV）、[第 2 週講義](../wk02_0914_text-utf8/README.md)（socket、UDP 模式）。
