# 第 9 週｜Team 2 實作工作坊（助教帶；指令一頁版）

目標：在 11/22 截止前，把測試 1、2、5 在教室至少過一次（照 tutorial 第 1–3 步做就會過）；有餘力往第 4–7 步走。做到哪一項就請助教簽；下課前每組回報。

| # | 簽核項目 | 證據 |
|---|---|---|
| 1 | 兩台電腦（不同 IP）baseline raw 互通 | `received N packets` |
| 2 | 你們的 send → 參考的 recv：`lost=0 concealed=0` | STATS 截圖進 `TEAM_LOG.md` |
| 3 | 參考的 send → 你們的 recv：PCM 逐 byte 相同 | `run_mock_tests.py` 測試 1 |
| 4 | `run_mock_tests.py` 你們 vs 你們：1–7 全 PASS | 終端機輸出 |
| 5 | buffer 掃描至少 3 個點、CSV 已 commit | repo |
| 6 | live 通話 30 秒、兩種 PLC（packet loss concealment，封包遺失補償） 都試過 | 助教現場聽 |
| 7 | `docs/interface.md` 與規格一致 | repo |
| 8 | `--codec mulaw`：你們 send → 參考 recv，SQNR ≥ 30 dB、bytes 減半 | STATS 與 `run_mock_tests.py` 測試 8 |

常見格式錯誤的症狀：`--probe` 看到 seq 一直跳 2 → 送端 seq 從 1 開始或加兩次；每一拍都 conceal → ts 單位錯（要是 sample 數，每包 +320）或 buffer 沒累積就開始播；輸出比輸入長 640 的倍數 → 沒用 BYE 的 ts 截長度。


左欄 macOS／Linux／WSL，右欄 Windows PowerShell（`make`→`mingw32-make`、`python3`→`python`、執行檔前加 `.\` 後加 `.exe`）。
`<A的IP>` 換成收的那台電腦的 IPv4（macOS `ipconfig getifaddr en0`、Linux `hostname -I`、Windows `ipconfig`）。兩台都連同一個熱點。
下面的 `in.wav` 用 `lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav`（33 秒）或 `aeueo-16kHz.wav`（7 秒，跑得快）。

## 0. 先更新

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd mmsp2026 && git pull` | `cd mmsp2026; git pull` |
| `cd team_projects/team2_voicelink/baseline && make` | `cd team_projects\team2_voicelink\baseline; mingw32-make` |

## 2.1 聽掉封包（官方 raw 版，兩台電腦）

| | A（收） | B（送） |
|---|---|---|
| 正常（macOS/Linux） | `./voicelink_raw recv 5004 out.wav` | `./voicelink_raw send <A的IP> 5004 ../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` |
| 正常（Windows） | `.\voicelink_raw.exe recv 5004 out.wav` | `.\voicelink_raw.exe send <A的IP> 5004 ..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav` |
| 搗蛋：A 多開一個視窗當中繼 | `python3 udp_impair.py 5003 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1`（Windows 用 `python`） | B 改送到 **5003**：`… send <A的IP> 5003 …` |
| 聽 | macOS `open out.wav`；Linux `xdg-open out.wav`；Windows `start out.wav` | |

## 2.2 用 Python 完整版當對手端

（在 `team_projects/team2_voicelink/python_ref`；你們的程式在自己的 repo，路徑自己補）

| 方向 | A（收） | B（送） |
|---|---|---|
| 你們 → 參考 | `python3 voicelink.py recv 5004 out.wav --probe` | 你們的 `voicelink send <A的IP> 5004 in.wav` |
| 參考 → 你們 | 你們的 `voicelink recv 5004 out.wav` | `python3 voicelink.py send <A的IP> 5004 in.wav` |
| 比對 PCM（在 A） | `python3 ../baseline/smoke_check.py in.wav out.wav` | `python ..\baseline\smoke_check.py in.wav out.wav` |

## 2.3 自動驗收 1–7（一台電腦就能跑，在 `team_projects/team2_voicelink/tests`）

| 誰對誰 | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 參考 vs 參考（先確認環境） | `python3 run_mock_tests.py "python3 ../python_ref/voicelink.py" "python3 ../python_ref/voicelink.py" in.wav` | `python run_mock_tests.py "python ..\python_ref\voicelink.py" "python ..\python_ref\voicelink.py" in.wav` |
| 你們 vs 你們 | `python3 run_mock_tests.py "/path/to/voicelink" "/path/to/voicelink" in.wav` | `python run_mock_tests.py "C:\path\voicelink.exe" "C:\path\voicelink.exe" in.wav` |
| 你們 send、參考 recv | `python3 run_mock_tests.py "/path/to/voicelink" "python3 ../python_ref/voicelink.py" in.wav` | `python run_mock_tests.py "C:\path\voicelink.exe" "python ..\python_ref\voicelink.py" in.wav` |
| 參考 send、你們 recv | `python3 run_mock_tests.py "python3 ../python_ref/voicelink.py" "/path/to/voicelink" in.wav` | `python run_mock_tests.py "python ..\python_ref\voicelink.py" "C:\path\voicelink.exe" in.wav` |

port 被佔用時加 `--port 5300` 換一組。

## 3.1 buffer 掃描（一台電腦、三個視窗；V 把它寫成腳本）

| 視窗 | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 中繼 | `python3 ../baseline/udp_impair.py 5003 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.05 --seed 1` | `python ..\baseline\udp_impair.py 5003 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.05 --seed 1` |
| 收（換 buffer 重跑） | `./voicelink recv 5004 o.wav --buffer 60 2>> sweep.txt` | `.\voicelink.exe recv 5004 o.wav --buffer 60 2>> sweep.txt` |
| 送 | `./voicelink send 127.0.0.1 5003 in.wav` | `.\voicelink.exe send 127.0.0.1 5003 in.wav` |
| 離線預測 | `python3 ../../../lectures/wk07_1019_audio-pcm/examples/jitter_sim.py --jitter 30 --loss 0.05 --reorder 0.05` | `python ..\..\..\lectures\wk07_1019_audio-pcm\examples\jitter_sim.py --jitter 30 --loss 0.05 --reorder 0.05` |

`sweep.txt` 裡每行一個 STATS；buffer 取 20、40、60、100、140、200，各 3 次。

## 3.2 live 通話（兩台電腦，戴耳機）

| | A | B |
|---|---|---|
| macOS / Linux | `./voicelink call 5005 <B的IP> 5005` | `./voicelink call 5005 <A的IP> 5005` |
| Windows | `.\voicelink.exe call 5005 <B的IP> 5005` | `.\voicelink.exe call 5005 <A的IP> 5005` |
| 換 PLC／buffer | 後面加 `--plc zero` 或 `--buffer 200` | 同左 |
| 列裝置 | `./voicelink devices` | `.\voicelink.exe devices` |

Windows 第一次會跳防火牆，兩台都要按「允許」（私人網路）。

## 常見狀況

| 看到什麼 | 怎麼辦 |
|---|---|
| 收不到任何封包 | 兩台是否同一個熱點；防火牆；port 兩邊一致；送端的 IP 打對（是收的那台） |
| 參考的 recv 說 `received=0` 但你們的 send 有送 | header 不合法被丟掉：檢查 magic 0x56、長度 652、big-endian |
| 每一拍都 conceal | ts 單位錯（要是 sample 數，每包 +320）、或 buffer 還沒累積就開始播 |
| 輸出比輸入長 640 的倍數 | 最後一包的補 0 沒截掉（收到 BYE 要用它的 ts 截長度） |
| `udp_impair.py` 一直印 drop | 正常，`--quiet` 關掉 |
