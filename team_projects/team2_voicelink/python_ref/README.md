# python_ref/：VoiceLink 的純 Python 完整版（mock 模式）

`voicelink.py` 是 Team 2 規格（[`../README.md`](../README.md)）的 **mock 模式**完整實作：`send`／`recv`、封包格式、jitter buffer、PLC（packet loss concealment，封包遺失補償）、STATS。
只用標準函式庫，Python 3.8 以上都能跑。它有兩個用途：

1. **對照用**：每一步都用最直白的高階寫法（`struct.pack`、`dict`、`select`），旁邊的註解標了「★ 對應 C」——你們的 C 程式就是把這些翻譯成環狀陣列、`recvfrom` 逾時、單調時鐘。
2. **評測的對手端**：評測會用它的 `send` 對你們的 `recv`、它的 `recv` 對你們的 `send` 各跑一次。先在家裡互測過，評測當天就不會有驚喜。

`call`／`devices`（live 模式）不在 Python 版裡：只印一句話、結束碼 2。要做的是 C。

## 用法

```
python voicelink.py send <ip> <port> <in.wav> [--burst] [--codec pcm|mulaw|adpcm]
python voicelink.py recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]
```

預設與規格相同：`--buffer 60`、`--plc repeat`、`--timeout 2000`、`--bind 0.0.0.0`、`--codec pcm`。任何指令加 `--probe` 會把中間過程印到 stderr。
`recv` 沒有 `--codec`：以收到的第一個合法 AUDIO 封包的 type 為準（見下面「語音壓縮」）。

同一台電腦自測（開兩個視窗；recv 先開）：

```
python voicelink.py recv 5004 out.wav --probe
python voicelink.py send 127.0.0.1 5004 in.wav
```

夾著搗蛋的中繼（三個視窗；recv 聽 5004、中繼聽 5005 轉給 5004、send 送到 5005）：

```
python voicelink.py recv 5004 out.wav --buffer 100 --probe
python ../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1 --dup 0.02 --seed 1
python voicelink.py send 127.0.0.1 5005 in.wav
```

## `--probe` 會印什麼

```
[probe] 第一包到了（seq=0 ts=0 ssrc=0xEB3BEAD7 codec=pcm，640 bytes 解成 640），60 ms 後開播
[probe] 開播：expected=0，buf=[0, 1, 2, 3]
[probe] tick     4  expected=4      buf=[4, 6, 7]  play  delay=58.3 ms
[probe] tick     5  expected=5      buf=[6, 7, 8]  conceal(repeat)        ← seq 5 掉了：補一個，seq 照樣前進
[probe] 收到 seq=6（ext 6）：亂序（最大是 7），還來得及，排回去           ← reordered
[probe] 收到 seq=22（ext 22）：重複，丟掉                                  ← dup
[probe] 收到 seq=40（ext 40）：遲到（已經播到 42），丟掉                   ← late
[probe] BYE：seq=352 ts=112541，結尾 = ext 352，排空 buffer（[349, 350, 351]）後結束
STATS role=recv buffer_ms=60 plc=repeat codec=pcm frames_out=352 received=343 lost=17 late=0 dup=8 reordered=34 overflow=0 concealed=17 avg_delay_ms=69.2 max_delay_ms=87.8
```

每一拍一行：`expected` 是下一個該播的 seq，`buf` 是此刻 buffer 裡有哪些 seq，後面是播了（和它從到達到播放等了多久）還是補了。
看著這個輸出對照規格的「jitter buffer 與播放時鐘」那一節，每一條規則都找得到對應的那一行。

## 程式結構（對照 C 的時候照這個順序讀）

| 區塊 | 函式 | 對應規格 | C 版要換成 |
|---|---|---|---|
| 1 WAV | `read_wav_pcm`、`write_wav_pcm` | 音訊格式；照 chunk 走 | wrapper 的 `wav.h`（已經會讀；確認它拒絕非 16 kHz／16-bit／單聲道） |
| 2 封包 | `pack_header`、`parse_packet`、`extend_seq` | 封包格式（長度要和 type 配：652／332／172／12）；16-bit seq 繞回 | 逐 byte 位移填 header（不要 `memcpy` struct）；`uint16_t` 相減再轉 `int16_t` |
| 2b codec | `mulaw_encode`／`mulaw_decode`、`adpcm_encode`／`adpcm_decode` | 功能 4：μ-law（type 0x04、320 bytes）、ADPCM（0x05、160 bytes） | 兩張查表（μ-law）；兩張 const 表＋一個 state struct（ADPCM） |
| 3 send | `cmd_send` | 每 20 ms 一包（先 encode）、3 次 BYE、`STATS role=send codec=…` | `clock.h` 的 `sleep_until`（不是 `sleep(20)`，誤差會累積） |
| 4 recv | `JitterBuffer.put`／`tick`、`cmd_recv` | 第一個 AUDIO 的 type 決定 codec → decode 成 16-bit → jitter buffer、播放時鐘、PLC、`STATS role=recv … codec=…` | `dict` → 固定大小的環狀陣列（`seq % 容量`）；`select` → `recvfrom` 加逾時 |
| 5 main | `main` | 命令列、結束碼 0／1／2 | `argv` 解析 |

計數器的定義在 `JitterBuffer` 的註解裡，設計成彼此一致：`frames_out = (received − late − dup − overflow) + concealed`，
沒有搗蛋時全部是 0、輸出 PCM 與輸入逐 byte 相同。

## 兩個「規格沒說、這支程式這樣做」的約定

你們的 C 版**可以**採用同樣的約定（和這支程式互測時輸出就會逐 byte 相同），不採用也能過驗收。

- **BYE 的 `ts` = 整個檔案真正的 sample 數**。規格只說 AUDIO 的 ts 是 320·k，BYE 的沒規定。最後一包不足 640 bytes 要補 0，
  但 header 沒有「這包有幾個 sample」的欄位；接收端收到 BYE 就用這個數字把輸出截到正確長度（只有當它落在最後一包裡才採用）。
  三次 BYE 全掉了就留著補的 0（長度多 320 個 sample 以內，評測容許）。
  它還有第二個用途：3 次 BYE 的 seq 各 +1，第一次 BYE 掉了或被亂序扣住時，只看 seq 會多算 1–2 個 frame；
  `ceil(ts / 320)` 和 seq 差 0、1、2 就知道這是第幾個 BYE。ts 和 seq 對不上（對方沒放 sample 數）就只能信 seq。
- **逾時收尾時去掉「送端死掉之後」補的 frame**。送端中途被殺，播放時鐘在等 `--timeout` 的這 2 秒還是每 20 ms 一拍、一直補；
  那些不是遺失補償，寫進 WAV 只是 2 秒的「嗡」聲。所以逾時時把最後一個真的封包之後的補償從輸出與 `frames_out`／`concealed` 裡去掉。
  （live 模式沒有這個問題：喇叭本來就要一直有東西。）

## 語音壓縮（功能 4）：`--codec mulaw`／`adpcm`

送端 `--codec` 決定封包的 type 與每包的音訊 bytes；收端不用參數，**第一個合法 AUDIO 封包的 type** 就是這次串流的 codec，
之後 type 不同的封包當壞封包（丟掉、計數、不當掉）。解碼在進 jitter buffer **之前**做，所以 buffer、PLC、STATS 的程式完全沒動——
`repeat` 重複的是解碼後的 16-bit frame。

| codec | type | 每包 | 位元率（含 12 bytes header） | 輸出 vs 輸入 |
|---|---|---|---|---|
| `pcm`（預設） | 0x01 | 12＋640 bytes | 256 kb/s（線上 260.8） | 逐 byte 相同 |
| `mulaw` | 0x04 | 12＋320 bytes | 128 kb/s（線上 132.8） | 有失真，看 SQNR |
| `adpcm`（必做） | 0x05 | 12＋160 bytes | 64 kb/s（線上 68.8） | 有失真，看 SQNR |

**μ-law 用的曲線**：和第 9 週 `examples/mulaw_demo.py` 一模一樣（μ = 255 的連續公式，不是 G.711 的分段折線）：
`y = sign(x)·ln(1 + μ|x|)／ln(1 + μ)`（x = sample／32768），`q = clamp(round(y·128), −128, 127)`，存成 1 byte = q + 128；
解碼 `x' = sign(q)·((1 + μ)^(|q|/128) − 1)／μ`。程式裡是兩張查表（65 536 格、256 格）。
你們的 C 版若用 G.711 的折線表也能互通——收端用自己的表解對方的 byte，曲線差一點點只是多一點失真，評測看的是 SQNR ≥ 30 dB。

**ADPCM 的設計**（規格只說「4-bit DPCM／ADPCM」，細節是這支程式自己定的）：
- 一階 DPCM：預測值 = 上一個**重建**的 sample（編碼端也用重建值預測，不是原始值，否則解碼端會跟不上）。
- 殘差量化成 4 bits：1 bit 正負＋3 bits 大小（和 step、step/2、step/4 比三次），重建時加 step/8 的半階修正。
- 階距自適應：IMA ADPCM 的 89 格 step 表（每格約 ×1.1）與 index 調整表 `[−1, −1, −1, −1, +2, +4, +6, +8]`——小殘差讓 step 慢慢變小，大殘差讓它快速變大。
- 打包：每 sample 4 bits、兩個一個 byte、先到的在高 4 bits；320 samples → 160 bytes。
- **每個 frame 獨立編碼**：frame 開頭 pred = 0，第一個 sample 用粗階距（index 48，step 724）一步跳到訊號附近，第二個 sample 起 index = 32（step 157）再自己調。
  編碼與解碼端都在 frame 邊界重置，所以一包只靠自己就能解：**掉一包不會讓之後的預測器失步**，PLC 補完下一包照常解。
  代價是每包開頭幾個 sample 要從 0「追」上訊號，SQNR 比整檔連續編碼低幾 dB（老師的語音檔：連續 34.8 dB，逐 frame 25.2 dB）。
  IMA 的真實格式是每個 block 開頭多 4 bytes 的初始狀態；這裡一包剛好 160 bytes 沒位置放，所以用固定初值。
  兩個初值是掃出來的折衷：0 dBFS 到 −30 dB 的音量都還有 ≥ 20 dB（初值更大，大聲時更好、小聲時會崩掉）。
  報告第 5 題「ADPCM 掉一包後怎麼重新同步」——這就是一種答案；另一種是不重置、靠 PLC 的 frame 把預測器「帶過去」，可以比較看看。

**實測**（`python_ref` 對 `python_ref`，沒有搗蛋，SQNR 對整個檔案算；`bytes` 是 `STATS role=send` 的數字 = AUDIO 封包的總和，不含 BYE）：

| 檔案 | pcm bytes | mulaw bytes（比例） | mulaw SQNR | adpcm bytes（比例） | adpcm SQNR |
|---|---|---|---|---|---|
| `speech_osr_16k.wav`（33.6 s，1 682 包） | 1 096 664 | 558 424（0.509） | 37.6 dB | 289 304（0.264） | 25.2 dB |
| `aeueo-16kHz.wav`（7.0 s，352 包） | 229 504 | 116 864（0.509） | 37.8 dB | 60 544（0.264） | 21.4 dB |

比例不是剛好 1/2 與 1/4，因為 12 bytes 的 header 不會縮（332/652 = 0.509、172/652 = 0.264）。`--loss 0.05` 時三種 codec 的 `concealed` 都 = `lost`，
因為 PLC 在解碼後做、每個 frame 又獨立編碼。

## 自動化驗收

[`../tests/run_mock_tests.py`](../tests/run_mock_tests.py) 自己啟動 `udp_impair.py`，跑規格「驗收與評分」的測試 1–7（pcm）、8 mulaw、9 adpcm；
第 10 個測試 Huffman 是加分項，要加 `--with-huffman` 才跑，否則印 `[SKIP]`（Python 版不會解 0x06，所以第 10 項兩端都要用你們的程式）；
`send_cmd`／`recv_cmd` 可以各自指定 Python 版或你們的 C 版，所以四種組合都能自動測：

```
cd tests
python run_mock_tests.py "python ../python_ref/voicelink.py send" "python ../python_ref/voicelink.py recv" in.wav
python run_mock_tests.py "../build/voicelink send"               "python ../python_ref/voicelink.py recv" in.wav
python run_mock_tests.py "python ../python_ref/voicelink.py send" "../build/voicelink recv"               in.wav
python run_mock_tests.py "python ../python_ref/voicelink.py send" "../build/voicelink recv"               in.wav --only 8,9
python run_mock_tests.py "../build/voicelink send"               "../build/voicelink recv"               in.wav --with-huffman --only 10
```

## 量測時會看到的一件事：第一包決定了時鐘

開播時刻 = **第一包**到達 + `--buffer`。之後第 k 包的拍子固定在那之後 20·k ms。所以第 k 包遲到的條件是
「它的網路延遲 > 第一包的延遲 + buffer」——第一包自己的延遲也算進 buffer 裡了。
`udp_impair.py --jitter 30` 的延遲是 uniform(0, 30)：第一包平均已經延遲 15 ms，`--buffer 20` 時幾乎不會有 late；
要看到 late，jitter 要比 buffer 大很多（`--jitter 60`），而且第一包要剛好抽到很短的延遲（換 `--seed` 多跑幾次就會遇到）。
報告的 buffer 掃描每個點跑 3 次（換 seed）就是為了這個；也可以想想：真實系統怎麼避免「第一包抽到的運氣」決定一切？
