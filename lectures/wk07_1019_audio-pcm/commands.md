# 第 7 週｜上課跟著打的指令（一頁版）

這一頁把[本週講義](README.md)裡的指令依上課順序收在一起。左欄 macOS／Linux／WSL，右欄 Windows 的 VSCode PowerShell。每一格都可以直接複製貼上。

**Windows 三個差別**：`make` 改打 `mingw32-make`、`python3` 改打 `python`、執行自己編出來的程式前面加 `.\`、後面有 `.exe`。
中文如果印成亂碼，先打 `chcp 65001`。

## 0. 開始之前：拿到本週內容

**電腦教室的公用電腦**：先做 [../wk03_0921_team1-kickoff/setup_first.md](../wk03_0921_team1-kickoff/setup_first.md)（貼一行指令裝好 Git、gcc、make、Python，並把課程 repo 抓到桌面），做完從下表第 3 列開始（資料夾在桌面的 `mmsp2026` 裡）。自己的筆電照下表：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd mmsp2026` | `cd mmsp2026` |
| `git pull` | `git pull` |
| `cd lectures/wk07_1019_audio-pcm` | `cd lectures\wk07_1019_audio-pcm` |

本週第一節沒有要編譯的 C 程式（都是 Python 與網頁）；第二節才編 Team 2 的 baseline。

## 1. 第一節｜聲音的時頻分析

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 看 32 ms 的語音 frame、估 F₀（1.3） | `python3 examples/frame_demo.py data/aeueo-16kHz.wav 1.0` | `python examples\frame_demo.py data\aeueo-16kHz.wav 1.0` |
| 換一個時間點（氣音的地方沒有週期） | `python3 examples/frame_demo.py data/aeueo-16kHz.wav 0.2` | `python examples\frame_demo.py data\aeueo-16kHz.wav 0.2` |
| 三種窗的主瓣與 side lobe、leakage（1.5） | `python3 examples/window_demo.py` | `python examples\window_demo.py` |
| STFT：窄頻（32 ms）文字版 spectrogram（1.6） | `python3 examples/stft_demo.py data/aeueo-16kHz.wav --ascii` | `python examples\stft_demo.py data\aeueo-16kHz.wav --ascii` |
| STFT：寬頻（4 ms） | `python3 examples/stft_demo.py data/aeueo-16kHz.wav --w_ms 4 --dft_ms 4 --ascii` | `python examples\stft_demo.py data\aeueo-16kHz.wav --w_ms 4 --dft_ms 4 --ascii` |
| rectangular 窗看 leakage | `python3 examples/stft_demo.py data/aeueo-16kHz.wav --win rectangular --ascii` | `python examples\stft_demo.py data\aeueo-16kHz.wav --win rectangular --ascii` |
| MP5 參考答案（需要 numpy；1.7） | `python3 ../../samples_2025-python/mini_project_5/spectrogram.py gen 8000 s-8k.wav` | `python ..\..\samples_2025-python\mini_project_5\spectrogram.py gen 8000 s-8k.wav` |
| MP5 參考答案：spec | `python3 ../../samples_2025-python/mini_project_5/spectrogram.py spec 32 hamming 32 10 s-8k.wav s-8k.Set2.txt` | `python ..\..\samples_2025-python\mini_project_5\spectrogram.py spec 32 hamming 32 10 s-8k.wav s-8k.Set2.txt` |

互動網頁（用瀏覽器開；也可以在檔案總管點兩下）：

| macOS | Linux / WSL | Windows PowerShell |
|---|---|---|
| `open slides/stft_steps.html` | `xdg-open slides/stft_steps.html` | `start slides\stft_steps.html` |

## 2. 第二節｜Team 2：即時語音

離線模擬 jitter buffer（2.3）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 examples/jitter_sim.py --jitter 30 --loss 0.05` | `python examples\jitter_sim.py --jitter 30 --loss 0.05` |

| macOS | Linux / WSL | Windows PowerShell |
|---|---|---|
| `open slides/jitter_buffer.html` | `xdg-open slides/jitter_buffer.html` | `start slides\jitter_buffer.html` |

baseline（2.5）——先走到資料夾並編譯：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd ../../team_projects/team2_voicelink/baseline` | `cd ..\..\team_projects\team2_voicelink\baseline` |
| `make` | `mingw32-make` |
| `./voicelink_raw devices` | `.\voicelink_raw.exe devices` |

正常傳一次（**兩個視窗**，都先 `cd` 到 baseline；先開 A 再開 B）：

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 視窗 A：收 | `./voicelink_raw recv 5004 out.wav` | `.\voicelink_raw.exe recv 5004 out.wav` |
| 視窗 B：送 | `./voicelink_raw send 127.0.0.1 5004 ../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `.\voicelink_raw.exe send 127.0.0.1 5004 ..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav` |
| 聽 | macOS：`open out.wav`；Linux：`xdg-open out.wav` | `start out.wav` |

搗蛋一次（**三個視窗**；中繼聽 5003、轉給 5004）：

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 視窗 A：收 | `./voicelink_raw recv 5004 bad.wav` | `.\voicelink_raw.exe recv 5004 bad.wav` |
| 視窗 C：中繼 | `python3 udp_impair.py 5003 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1` | `python udp_impair.py 5003 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1` |
| 視窗 B：送給中繼 | `./voicelink_raw send 127.0.0.1 5003 ../../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `.\voicelink_raw.exe send 127.0.0.1 5003 ..\..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav` |
| 聽 | macOS：`open bad.wav`；Linux：`xdg-open bad.wav` | `start bad.wav` |

Python 完整版（2.6；同樣兩個視窗，從 baseline 走到 python_ref）：

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 走到資料夾 | `cd ../python_ref` | `cd ..\python_ref` |
| 視窗 A：收（印每一拍的 buffer） | `python3 voicelink.py recv 5004 out.wav --probe` | `python voicelink.py recv 5004 out.wav --probe` |
| 視窗 B：送 | `python3 voicelink.py send 127.0.0.1 5004 ../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `python voicelink.py send 127.0.0.1 5004 ..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav` |
| 自動跑驗收 1–7（一個視窗就夠） | `cd ../tests && python3 run_mock_tests.py "python3 ../python_ref/voicelink.py" "python3 ../python_ref/voicelink.py" ../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `cd ..\tests; python run_mock_tests.py "python ..\python_ref\voicelink.py" "python ..\python_ref\voicelink.py" ..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav` |

## 3. 第三節｜分組後馬上做

| 做什麼 | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 查自己的 IP | macOS：`ipconfig getifaddr en0`；Linux：`hostname -I` | `ipconfig`（看「IPv4 位址」） |
| 兩台電腦互傳：收的那台 | `./voicelink_raw recv 5004 out.wav` | `.\voicelink_raw.exe recv 5004 out.wav` |
| 送的那台 | `./voicelink_raw send <對方的IP> 5004 ../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `.\voicelink_raw.exe send <對方的IP> 5004 ..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav` |
| 兩台電腦通話（戴耳機；兩邊各打一行） | `./voicelink_raw call 5005 <對方的IP> 5005` | `.\voicelink_raw.exe call 5005 <對方的IP> 5005` |

## 常見狀況

| 看到什麼 | 怎麼辦 |
|---|---|
| `make : 無法辨識…`（Windows） | 打 `mingw32-make` |
| `python3` 開啟 Microsoft Store（Windows） | 打 `python` |
| `bind` 失敗／`Address already in use` | 上一個 recv 還沒關，或 port 被佔用：換一個 port，兩邊要一致 |
| 跨電腦收不到 | 兩台要在同一個網路（校園 Wi-Fi 常擋 UDP → 用手機熱點）；收的那台 Windows 第一次會跳防火牆，按「允許」 |
| `devices` 列不出麥克風 | 教室電腦可能沒有麥克風；mock 模式（send／recv）不需要麥克風，live 回家再試 |
| `out.wav` 比原檔短、聲音倒著播 | 正常！raw 版沒有 seq、沒有 buffer——這就是 Team 2 要解決的問題 |
