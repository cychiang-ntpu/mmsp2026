# Team 2 的核心：不等、會掉、要補（11/9 討論會前必讀；10/19 沒講到的部分）

> 搭配 [tutorial/TUTORIAL.md](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 第 1–3 步做一遍，起點層的測試 1、2、5 就過了。


### 1 為什麼不用 TCP

| | TCP（Team 1） | UDP（Team 2） |
|---|---|---|
| 掉包 | 重傳，後面的**全部等** | 掉了就掉，**從不等** |
| 邊界 | byte stream，要自己加長度前綴 | 一個 `sendto` 就是一個封包 |
| 順序 | 保證 | 不保證，可能亂序、重複 |

語音每 20 ms 一包：掉一包少 20 ms 的聲音，補一下幾乎聽不出；等重傳一次 RTT（50–200 ms）後面全部延後，而且延遲會累積。**即時媒體寧可掉，不要等**——這是整個 Team 2 的設計原則。

### 2 封包格式：seq、ts、ssrc 各回答一個問題

20 ms = 320 samples = 640 bytes（16-bit PCM），加 12 bytes header：

```
magic 'V' | type | seq(2) | ts(4) | ssrc(4) | 640 bytes PCM        多 byte 欄位 big-endian
```

| 欄位 | 回答的問題 | 用途 |
|---|---|---|
| `seq`（每包 +1，16-bit 會繞回） | 有沒有**掉**、**亂序**、**重複** | 偵測與計數 |
| `ts`（單位 sample，每包 +320） | 這包**什麼時候該播** | 對齊播放時鐘 |
| `ssrc`（隨機） | 這是**哪一次**的串流 | 對方重啟就重置 |

這是 RTP（Real-time Transport Protocol，即時傳輸協定）（RFC 3550）的精簡版。`type` 另有 0x04 μ-law（320 bytes）、0x05 ADPCM（160 bytes）——功能 4，見第 5b 節。

### 3 jitter buffer 與播放時鐘

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

### 4 PLC：封包掉了，喇叭不能停

**PLC = packet loss concealment（遺失補償）**：第 3 步「沒有就補一個」補的那個 frame 怎麼做。規格要求兩種都做：

| 方法 | 做法 | 聽起來 | 問題 |
|---|---|---|---|
| `zero` | 補 320 個 0 | 「喀」一聲斷音 | 波形突然跳到 0 → click |
| `repeat` | 重複上一個播出去的 frame | 單掉一包幾乎聽不出 | 連續掉好幾包 → 同一個 20 ms 一直重複，「嗡」的機械音 |

注意 repeat 重複的是「**收端手上已經有的**」東西（上一個播出去的 frame），這和 2.3 DPCM「預測器要用解碼端重建的值」是同一個道理。
真的 codec（G.711 Appendix I、Opus）是重複**上一個 pitch 週期**並逐包衰減、接縫做 2 ms 淡入淡出——第 7 週 1.3 的 F₀ 就用在這裡；規格的加分項「衰減」「淡入淡出」就是這兩個。更進階的 FEC（下一包帶上一包的低品質版）、DTX／CNG（沒人講話不送）、自適應 buffer（WebRTC NetEQ）見第 7 週 2.4 與本講義課後讀的 3.6。

### 5 手把手：跟著 tutorial 做第 1–3 步

[team_projects/team2_voicelink/tutorial/TUTORIAL.md](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 七步，每步一支完整、有註解的程式。課堂上投影 `step2_header.c` 與 `step3_buffer.c` 的差異：header 怎麼打包（`put_u16`／`put_u32`）、環形 buffer 怎麼用 `seq % 16` 當索引、播放時鐘怎麼用「下一拍的時間」當 `udp_recv` 的 timeout 所以不用執行緒、沒有就 `memcpy` 上一個 frame（repeat）。做完第 3 步就過起點層的測試 1、2、5。

### 5b 新功能 4：μ-law 壓縮（進階 A3）、ADPCM（加分）

tutorial 第 6 步；做法見 [audio_coding.md](audio_coding.md) 的圖解，直接用：`--codec mulaw` 把每個 sample 16-bit → 8-bit（2.2 的公式或查表），一包 320 bytes、位元率減半，封包 `type` = 0x04；收端解回 16-bit 再進 jitter buffer，**PLC 不用改**。驗收第 10 項：沒有搗蛋時輸出對原檔 SQNR ≥ 30 dB（`mulaw_demo.py` 實測 36–38 dB）、`bytes` 減半。
ADPCM（2.3 的做法，4 bits、160 bytes、type 0x05）是加分：每個 frame 獨立編碼，掉一包預測器才不會失步——報告第 5 題要討論。
對手端 `python_ref/voicelink.py send --codec mulaw` 已支援，先拿它確認你們的解碼對不對。

### 6 接下來三週怎麼跑

評分（規格「驗收與評分」，11/8 改版）：**起點層 40**（tutorial 第 7 步原樣：測試 1–7 與兩台 live）、**壓縮層 30**（μ-law 15、ADPCM 15）、**加分 10**（Huffman）、**口試 20**；另有 11/9、11/16 工作坊的過程分 30。

- **11/9 工作坊 ①**（助教主持）：全班跟著 [tutorial](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md) 做 Part A 模組與第 1–3 步，每步 push。
- **11/16 工作坊 ②**：第 4–7 步（完整 STATS、壞封包、μ-law、live）、組間互測、live 預演；進度報告＋小成品 11/22 與 SHA 一起交。
- **11/22（日）18:00 截止；11/23 評測**：mock 用老師的 WAV 跑 `run_mock_tests.py` 兩個方向、live 用熱點；評測前老師先回答兩次討論會收集的問題。
- 工作坊的指令與簽核表在 [lab.md](lab.md)；先把 lab.md 的 2.1「聽掉封包」跑一次。

課後讀（原本第三節的內容，期末會用到）：第 7 週 2.5–2.6（baseline 導覽）、以及下面的「延伸：LPC／CELP 與感知編碼」。

---

