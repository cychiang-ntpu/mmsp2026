# python_ref/：VoiceLink 的純 Python 完整版（mock 模式）

`voicelink.py` 是 Team 2 規格（[`../README.md`](../README.md)）的 **mock 模式**完整實作：`send`／`recv`、封包格式、jitter buffer、PLC、STATS。
只用標準函式庫，Python 3.8 以上都能跑。它有兩個用途：

1. **對照用**：每一步都用最直白的高階寫法（`struct.pack`、`dict`、`select`），旁邊的註解標了「★ 對應 C」——你們的 C 程式就是把這些翻譯成環狀陣列、`recvfrom` 逾時、單調時鐘。
2. **評測的對手端**：評測會用它的 `send` 對你們的 `recv`、它的 `recv` 對你們的 `send` 各跑一次。先在家裡互測過，評測當天就不會有驚喜。

`call`／`devices`（live 模式）不在 Python 版裡：只印一句話、結束碼 2。要做的是 C。

## 用法

```
python voicelink.py send <ip> <port> <in.wav> [--burst]
python voicelink.py recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]
```

預設與規格相同：`--buffer 60`、`--plc repeat`、`--timeout 2000`、`--bind 0.0.0.0`。任何指令加 `--probe` 會把中間過程印到 stderr。

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
[probe] 第一包到了（seq=0 ts=0 ssrc=0xEB3BEAD7），60 ms 後開播
[probe] 開播：expected=0，buf=[0, 1, 2, 3]
[probe] tick     4  expected=4      buf=[4, 6, 7]  play  delay=58.3 ms
[probe] tick     5  expected=5      buf=[6, 7, 8]  conceal(repeat)        ← seq 5 掉了：補一個，seq 照樣前進
[probe] 收到 seq=6（ext 6）：亂序（最大是 7），還來得及，排回去           ← reordered
[probe] 收到 seq=22（ext 22）：重複，丟掉                                  ← dup
[probe] 收到 seq=40（ext 40）：遲到（已經播到 42），丟掉                   ← late
[probe] BYE：seq=352 ts=112541，結尾 = ext 352，排空 buffer（[349, 350, 351]）後結束
STATS role=recv buffer_ms=60 plc=repeat frames_out=352 received=343 lost=17 late=0 dup=8 reordered=34 overflow=0 concealed=17 avg_delay_ms=69.2 max_delay_ms=87.8
```

每一拍一行：`expected` 是下一個該播的 seq，`buf` 是此刻 buffer 裡有哪些 seq，後面是播了（和它從到達到播放等了多久）還是補了。
看著這個輸出對照規格的「jitter buffer 與播放時鐘」那一節，每一條規則都找得到對應的那一行。

## 程式結構（對照 C 的時候照這個順序讀）

| 區塊 | 函式 | 對應規格 | C 版要換成 |
|---|---|---|---|
| 1 WAV | `read_wav_pcm`、`write_wav_pcm` | 音訊格式；照 chunk 走 | wrapper 的 `wav.h`（已經會讀；確認它拒絕非 16 kHz／16-bit／單聲道） |
| 2 封包 | `pack_header`、`parse_packet`、`extend_seq` | 封包格式；16-bit seq 繞回 | 逐 byte 位移填 header（不要 `memcpy` struct）；`uint16_t` 相減再轉 `int16_t` |
| 3 send | `cmd_send` | 每 20 ms 一包、3 次 BYE、`STATS role=send` | `clock.h` 的 `sleep_until`（不是 `sleep(20)`，誤差會累積） |
| 4 recv | `JitterBuffer.put`／`tick`、`cmd_recv` | jitter buffer、播放時鐘、PLC、`STATS role=recv` | `dict` → 固定大小的環狀陣列（`seq % 容量`）；`select` → `recvfrom` 加逾時 |
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

## 自動化驗收

[`../tests/run_mock_tests.py`](../tests/run_mock_tests.py) 自己啟動 `udp_impair.py`，跑規格「最低驗收」第 1–7 項；
`send_cmd`／`recv_cmd` 可以各自指定 Python 版或你們的 C 版，所以四種組合都能自動測：

```
cd tests
python run_mock_tests.py "python ../python_ref/voicelink.py send" "python ../python_ref/voicelink.py recv" in.wav
python run_mock_tests.py "../build/voicelink send"               "python ../python_ref/voicelink.py recv" in.wav
python run_mock_tests.py "python ../python_ref/voicelink.py send" "../build/voicelink recv"               in.wav
```

## 量測時會看到的一件事：第一包決定了時鐘

開播時刻 = **第一包**到達 + `--buffer`。之後第 k 包的拍子固定在那之後 20·k ms。所以第 k 包遲到的條件是
「它的網路延遲 > 第一包的延遲 + buffer」——第一包自己的延遲也算進 buffer 裡了。
`udp_impair.py --jitter 30` 的延遲是 uniform(0, 30)：第一包平均已經延遲 15 ms，`--buffer 20` 時幾乎不會有 late；
要看到 late，jitter 要比 buffer 大很多（`--jitter 60`），而且第一包要剛好抽到很短的延遲（換 `--seed` 多跑幾次就會遇到）。
報告的 buffer 掃描每個點跑 3 次（換 seed）就是為了這個；也可以想想：真實系統怎麼避免「第一包抽到的運氣」決定一切？
