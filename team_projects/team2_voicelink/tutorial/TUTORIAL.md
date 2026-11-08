# Team 2 VoiceLink 手把手教學：先拆成九個模組看懂，再七步組裝成官方起點（step7）＋μ-law；m9 是 ADPCM 的零件

> **11/8 起**：第 7 步 `step7_live.c` 就是作業的**官方起點**（起點層 40 分）。這份教學的目的是讓你們**讀懂**起點的每一行，因為接下來要在它上面加 ADPCM（必做）與 Huffman（加分）——見規格「驗收與評分」。

這份教學假設你修過一學期的 C，看過第 2 週的 `chat.c`（所以知道 socket 是什麼、`sendto`／`recvfrom` 長什麼樣），
其他都從零講。分成兩部分：

- **Part A 拆解**：九個 50–130 行的小程式 `m1_wav.c` … `m9_adpcm.c`，每個只做**一件事**、印出看得見的結果。
  先把每一塊零件玩過一遍（讀 WAV、寄一張 UDP 明信片、每 20 ms 醒來、錄音、放音、打包 header、環狀 buffer、μ-law、ADPCM）。
- **Part B 組裝**：七個程式 `step1_raw.c` … `step7_live.c`，每一個都是前一個的「超集」，把零件接起來變成規格的 `voicelink`。

Part B 的七步：

```
step1_raw      640 bytes 一包、沒有 header                       ← 和 baseline/voicelink_raw 一樣笨
step2_header   + 12 bytes header（magic/type/seq/ts/ssrc）+ BYE   ← 看得到 seq 跳號了
step3_buffer   + jitter buffer + 播放時鐘 + PLC（packet loss concealment，封包遺失補償） repeat + STATS    ← 過測試 1,2,5（起點層的核心）
step4_counters + late/dup/reordered/overflow/延遲 + --plc + --burst ← 過測試 1,2,3,5,7
step5_robust   + 壞封包、ssrc 換了、seq 繞回、--bind、結束碼        ← 過測試 1–7（起點層 S1–S6）
step6_mulaw    + --codec mulaw（功能 4）                           ← 過測試 8（壓縮層 C1 μ-law）
step7_live     + call／devices（麥克風 ↔ 喇叭，執行緒）             ← 起點層 S0；**這就是官方起點**
```

每一步都可以單獨編譯、單獨執行、單獨測；想知道「這一步到底改了什麼」就 `diff step3_buffer/step3_buffer.c step4_counters/step4_counters.c`。
七個檔案只用 `baseline/wrapper/` 的四個標頭（`udp.h`、`clock.h`、`wav.h`、`audio.h`），什麼都沒複製，編譯時 `-I../baseline/wrapper`。

**每支程式開頭的 `#include "udp.h"`、`"clock.h"`、`"wav.h"`、`"audio.h"` 都不是系統函式庫**，是課程自己寫的 wrapper，放在 [`../baseline/wrapper/`](../baseline/wrapper/)（Makefile 用 `-I` 指過去）。
`udp.h` 底下在 Windows 是 Winsock、在 macOS／Linux 是 POSIX socket；它一定要是第一個 `#include`，因為它要在任何系統標頭之前定義 `_POSIX_C_SOURCE`，而且 `winsock2.h` 必須排在 `windows.h` 前面。每個 `.c` 檔的 include 區都有一段註解說明這件事（只有 m7 完全不用 wrapper，是純 C）。

規格在 [`../README.md`](../README.md)：先把「名詞解釋」那張表看過一遍，下面每一步會指回去。
對照用的 Python 完整版在 [`../python_ref/voicelink.py`](../python_ref/voicelink.py)：它是評測的對手端，所以每一步都拿它互測。

## 0. 準備

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial` | `cd team_projects\team2_voicelink\tutorial` |
| `make`（全部）；`make step3` 只編第 3 步；或 `cd step3_buffer && make` | `mingw32-make`；`mingw32-make step3`；或 `cd step3_buffer; mingw32-make` |
| `W=../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

**每個模組與每一步各自一個資料夾**，裡面有自己的 `README.md`（該步的完整說明）、`.c` 與 `Makefile`；這一頁是總覽，照順序點進去做。
各資料夾的 README 開頭都會重新定義 `$W`（因為多一層，路徑多一個 `..`）。下面所有指令都用 `$W` 代表那個 7 秒的測試 WAV（16 kHz、單聲道）。要聽真的語音就換成同一個資料夾的 `speech_osr_16k.wav`（34 秒）。
每一步的測試都要開**兩個**（或三個）終端機視窗：收端先開、再開送端；夾搗蛋中繼時中繼第二個開。

`make`（`mingw32-make`）會把 Part A 的模組和 Part B 的七步全部編出來（`make modules` 只編模組）。
`make check`（Git Bash／macOS／Linux）先跑不用互動的模組 m3／m6／m7／m8 並檢查結果，再自動跑 step3–step6 該過的測試、印總表；
`make check-ref` 改用 Python 版當送端（評測就是這樣跑）。
Windows 的 PowerShell 沒有 `sh`，`check` 請在 Git Bash 裡跑；單一步的指令在 PowerShell 都能跑。

---

# Part A 拆解：一次只看一個模組

每個模組都是獨立的一支程式，只用 wrapper 的標頭，50–80 行，註解比程式多。建議順序 m1 → m9，每個 10 分鐘（m8、m9 各 20 分鐘，它們是功能 4 的兩個 codec，README 寫得比較長、邊做邊學）：跑一次、看輸出、改一個地方再跑一次。

為什麼要先拆成模組：整支 voicelink 有九個互不相干的零件（讀 WAV、寄 UDP、準時醒來、錄音、放音、打包 header、環狀 buffer、μ-law、ADPCM），
一次全部看會糊掉；每個零件單獨拿出來跑、改一處、看結果，10 分鐘就能確定「這一塊我懂了」，Part B 組裝時才知道哪一行是哪個零件。

| 模組 | 為什麼要有它 | 在 Part B 變成 |
|---|---|---|
| m1 | 聲音全部是 16 kHz 單聲道 16-bit 的 WAV，不會讀寫就什麼都做不了 | step1 的讀檔／寫檔 |
| m2 | 整個作業的「掉包、亂序、重複」都來自 UDP；先親手看到一包就是一包、寄丟了送端不知道 | step1 的 send／recv 迴圈 |
| m3 | 送端每 20 ms 一包、收端每 20 ms 播一包，都靠「絕對時刻」的時鐘；用「睡 20 ms」會越來越慢 | step1 的節奏、step3 的播放時鐘 |
| m4、m5 | live 通話的麥克風與喇叭是 callback 驅動的，先在沒有網路時看懂它什麼時候被叫、一次給多少 | step7 的 `audio_cb` |
| m6 | 每包前面 12 bytes 的 header 要和 python_ref 互通，位元組順序一個都不能錯，先用 hex 確認 | step2 的 `pack_header`／`parse_header` |
| m7 | Team 2 的心臟：亂序放回、重複丟掉、沒到就補、拍子一定前進——先用 8 個數字想清楚 | step3 的 `jb_put`／`jb_tick` |
| m8 | 功能 4 的第一個 codec（C1）；先在檔案上把表與 SQNR 做對 | step6 的 μ-law 編解碼 |
| m9 | 功能 4 的第二個 codec（C2）：只送「差」、刻度自己調；`--trace` 看編碼器每一步在想什麼 | **沒有現成的 step**：你們要自己加進 step7 的 `--codec adpcm` |

每個模組資料夾的 README 開頭都有「這個模組的目的」四點：做什麼、為什麼要先做它、在 Part B 哪裡用到、做完要能回答什麼。

## m1_wav.c：讀 WAV、印格式、寫出前 1 秒

**看什麼**：`wav.h` 的 `wav_read(path, &w)` 把整個檔案讀進 `wav_t`（一個 struct：取樣率、聲道、bits、`pcm` 指標、sample 數）；
`&w` 是「輸出參數」——C 沒有回傳兩個值，所以把變數的位址交給函式填。`wav_write` 反過來。Team 2 只收 16 kHz／單聲道／16-bit。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m1_wav/README.md](m1_wav/README.md)**（程式：`m1_wav/m1_wav.c`）。

## m2_udp_send.c／m2_udp_recv.c：一次 sendto = 一個 datagram

**看什麼**：UDP 沒有 connect／accept。送端 `udp_open(&u, NULL, 0)`（隨便一個 port）＋ `udp_set_peer`（記下寄給誰）＋ `udp_send`；
收端 `udp_open(&u, NULL, port)` ＋ `udp_recv`（有逾時）。打一行寄一張明信片，對面一次收一整張——不會半行、不會黏行（TCP 才會）。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m2_udp/README.md](m2_udp/README.md)**（程式：`m2_udp/m2_udp_send.c`、`m2_udp/m2_udp_recv.c`）。

## m3_clock.c：每 20 ms 醒來一次，誤差多少？

**看什麼**：`clock.h` 的 `sleep_until_ms(t0 + 20k)`——第 k 拍的截止時刻是**絕對時刻**，不是「上一拍 + 20」，所以誤差不累積。
這是送端「每 20 ms 一包」和收端「播放時鐘」都依賴的東西。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m3_clock/README.md](m3_clock/README.md)**（程式：`m3_clock/m3_clock.c`）。

## m4_mic_to_wav.c：錄 3 秒麥克風

**看什麼**：`audio.h` 的 **callback**——你不是去「要」聲音，而是交一個函式給音訊系統，它每 20 ms 在它自己的執行緒上呼叫你，
給你 `n` 個剛錄到的 sample、要你填 `n` 個要播的 sample（沒東西就填 0）。`n` 通常是 320 但不保證。`user` 指標把我們的 struct 帶進 callback。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m4_mic_to_wav/README.md](m4_mic_to_wav/README.md)**（程式：`m4_mic_to_wav/m4_mic_to_wav.c`）。

## m5_play_wav.c：透過 callback 播一個 WAV

**看什麼**：喇叭每次 callback 跟你要 `n` 個 sample，你從檔案的下一段複製過去；檔案播完就填 0。
live 模式的喇叭就是這樣餵的——只是「下一段」改成向 jitter buffer 要一個 frame、沒有就 PLC。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m5_play_wav/README.md](m5_play_wav/README.md)**（程式：`m5_play_wav/m5_play_wav.c`）。

## m6_header.c：12 bytes 的 header，big-endian

**看什麼**：`put_u16`／`put_u32`（`udp.h`）一個 byte 一個 byte 用位移放：`0x1234` 變成 `12 34`（高位在前）。
不能把 struct 直接 `memcpy` 進封包（對齊空隙、CPU 是 little-endian）。`get_u16`／`get_u32` 解析回來要一模一樣。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m6_header/README.md](m6_header/README.md)**（程式：`m6_header/m6_header.c`）。

## m7_ringbuffer.c：沒有網路的 jitter buffer（step3 的心臟）

**看什麼**：16 格環狀陣列，第 `seq` 包放在第 `seq % 16` 格；播放時鐘每拍要 `next_seq`，看一格就知道在不在：有 → PLAY、沒有 → REPEAT，
**拍子一定前進**。假的到達順序 `0,1,3,2,5,5,4,7`：3 比 2 先到（亂序）、5 到兩次（重複）、6 沒到（遺失）。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m7_ringbuffer/README.md](m7_ringbuffer/README.md)**（程式：`m7_ringbuffer/m7_ringbuffer.c`）。

## m8_mulaw.c：μ-law 壓成 8 bits 再解回來，SQNR 多少？

**看什麼**：第 9 週 2.2 的公式照抄，建兩張表（`enc[65536]`、`dec[256]`），之後每個 sample 查一次表。bytes 剛好一半；SQNR ≈ 37 dB（規格 ≥ 30）。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[m8_mulaw/README.md](m8_mulaw/README.md)**（程式：`m8_mulaw/m8_mulaw.c`）。

## m9_adpcm.c：ADPCM 壓成 4 bits 再解回來——只送「差」、刻度自己調

**看什麼**：ADPCM 三個字母三個想法——**D** 不送 sample 送「和預測值的差」、**PCM** 把差用 4 bits（1 bit 正負＋3 bits 大小）量化、**A** 刻度 step 看剛才的碼自己放大縮小，兩邊同一張表所以不用送。
最重要的一句話：預測要用**重建值**不用原始值（編碼端裡藏一個解碼器，兩邊呼叫同一個 `adpcm_reconstruct`）。`--trace 8` 逐 sample 印出預測、殘差、step、碼、重建；`--frame 0`／`--fixed-step` 兩個開關讓你聽到「為什麼每包歸零」與「為什麼要自適應」。
和 `python_ref` 同一張表、同樣初值，所以搬進 step7 之後能互通、過測試 9。

→ 完整說明、逐行解讀、四個實驗、搬進 step7 的步驟與常見錯誤在 **[m9_adpcm/README.md](m9_adpcm/README.md)**（程式：`m9_adpcm/m9_adpcm.c`）。

# Part B 組裝：七步做出 voicelink

## 第 1 步｜step1_raw.c：把 WAV 切成 20 ms 一包用 UDP 送出去

**目標**：`send` 每 20 ms 送一包 640 bytes、`recv` 收到什麼就照到達順序寫成 WAV——先讓聲音「過得去」。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[step1_raw/README.md](step1_raw/README.md)**（程式：`step1_raw/step1_raw.c`）。

## 第 2 步｜step2_header.c：每包加 12 bytes 的 header，親眼看見 seq 跳號

**目標**：送端每包前面加 `magic/type/seq/ts/ssrc`，最後送 3 次 BYE；收端解析、驗證、印出每包的 seq／ts，但還是照到達順序寫。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[step2_header/README.md](step2_header/README.md)**（程式：`step2_header/step2_header.c`）。

## 第 3 步｜step3_buffer.c：jitter buffer、播放時鐘、PLC——起點層的核心

**目標**：收端先存 `--buffer` ms 再播，之後每 20 ms 一拍向 buffer 要「下一個 seq」，有就播、沒有就補，拍子一定前進；過測試 1、2、5。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[step3_buffer/README.md](step3_buffer/README.md)**（程式：`step3_buffer/step3_buffer.c`）。

## 第 4 步｜step4_counters.c：把每一包都算清楚（late／dup／reordered／overflow／延遲）

**目標**：每個收到的 AUDIO 封包剛好落在一類，`frames_out = (received − late − dup − overflow) + concealed` 永遠成立；加 `--plc zero|repeat`、
`send --burst`；過測試 1、2、3、5、7。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[step4_counters/README.md](step4_counters/README.md)**（程式：`step4_counters/step4_counters.c`）。

## 第 5 步｜step5_robust.c：壞封包不當掉、ssrc 換了、seq 繞回、--bind、結束碼

**目標**：對 port 丟 100 包亂碼不當掉；對方重啟（ssrc 換了）就重置；seq 從 65535 繞到 0 照樣對；過測試 1–7（含 4、6）。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[step5_robust/README.md](step5_robust/README.md)**（程式：`step5_robust/step5_robust.c`）。

## 第 6 步｜step6_mulaw.c：功能 4 語音壓縮——`--codec mulaw`，位元率減半

**目標**：送端把每個 16-bit sample 壓成 8 bits（一包 320 bytes，type 0x04），收端解回 16-bit 再進 buffer；SQNR ≥ 30 dB、`bytes` 減半；過測試 8。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[step6_mulaw/README.md](step6_mulaw/README.md)**（程式：`step6_mulaw/step6_mulaw.c`）。

## 第 7 步｜step7_live.c：`call`——麥克風進去、對方喇叭出來（執行緒只在這一步出現）

**目標**：`call <my_port> <peer_ip> <peer_port>` 兩台電腦戴耳機通話；同一套 jitter buffer 與 PLC；按 Enter 結束印 STATS；`devices` 列裝置。

→ 完整說明、指令、程式碼導讀與「改一個地方」在 **[step7_live/README.md](step7_live/README.md)**（程式：`step7_live/step7_live.c`）。

## 接下來：變成你們自己的 voicelink

1. 把 `step7_live/step7_live.c` 改名放進 team repo 的 `src/`，`wrapper/` 的四個標頭放 `include/`（或照 baseline 的路徑 `-I`），Makefile 照 `tutorial/step7_live/Makefile`＋`tutorial/common.mk` 改。
   一支程式同時有 `send`／`recv`／`call`／`devices`，就是規格的 `voicelink`。
2. `tests/` 放 `run_mock_tests.py` 的呼叫腳本（`make test`），四種組合都跑：自己對自己、Python 送我收、我送 Python 收。
3. 報告三題（第三題是 codec 比較表）：buffer 設多少（用 `avg_delay_ms`／`concealed` 掃 20–200 ms）、`repeat` 與 `zero` 的差別（用耳朵和 `concealed` 的數字）。
4. 接著做這一輪真正的作業：**ADPCM**（必做，`python_ref` 有做法）、**Huffman**（加分）；見規格「驗收與評分」。

| 步驟 | 要過的測試（`--only`） | 規格的驗收項目 |
|---|---|---|
| step3 | 1,2,5 | 起點層 S1、S2、S4 |
| step4 | 1,2,3,5,7 | 起點層 S3、S5 |
| step5 | 1,2,3,4,5,6,7 | 起點層 S6（dup、壞封包） |
| step6 | 8 | 壓縮層 C1 μ-law |
| step7 | 現場 | 起點層 S0 live；**官方起點**。接下來：加 `--codec adpcm`（C2，測試 9）、`huff-*`（X1，測試 10） |
