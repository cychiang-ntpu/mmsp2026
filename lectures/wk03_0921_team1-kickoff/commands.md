# 第 3 週｜上課跟著打的指令（一頁版）

這一頁把[本週講義](README.md)裡散在各節的指令依上課順序收在一起。左欄 macOS／Linux／WSL，右欄 Windows 的 VSCode PowerShell。
每一格都可以直接複製貼上。

**Windows 三個差別**：`make` 改打 `mingw32-make`、`python3` 改打 `python`、執行自己編出來的程式前面加 `.\`、後面有 `.exe`。
中文如果印成亂碼，先打 `chcp 65001`。

## 0. 開始之前：拿到本週內容

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd mmsp2026` | `cd mmsp2026` |
| `git pull` | `git pull` |
| `cd lectures/wk03_0921_team1-kickoff/examples` | `cd lectures\wk03_0921_team1-kickoff\examples` |
| `make` | `mingw32-make` |

看到 `huffman_trace` 與 `entropy`（Windows 是 `.exe`）兩支程式就成功了。以下第 1、2 節的指令都在這個 `examples` 資料夾裡打。

## 1. 第一節｜Huffman coding

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 算檔案的熵（1.3） | `./entropy ../README.md` | `.\entropy.exe ..\README.md` |
| 同一件事的 Python 版 | `python3 entropy.py ../README.md` | `python entropy.py ..\README.md` |
| 確認 C 與 Python 輸出相同 | `make check` | `mingw32-make check` |
| Huffman：預設 ABRACADABRA（1.6） | `./huffman_trace` | `.\huffman_trace.exe` |
| 一步一步走，按 Enter 繼續 | `./huffman_trace --step` | `.\huffman_trace.exe --step` |
| 講義的 8 色影像 | `make trace` | `mingw32-make trace` |
| 換成中文（符號＝UTF-8 字元） | `./huffman_trace "多媒體多媒多"` | `.\huffman_trace.exe "多媒體多媒多"` |
| 自己給頻率表 | `./huffman_trace --freq a:5,b:2,c:1` | `.\huffman_trace.exe --freq a:5,b:2,c:1` |
| Python 精簡版（會印打包後的 bytes） | `python3 huffman_demo.py` | `python huffman_demo.py` |
| 回家作業對答案 | `./huffman_trace "MISSISSIPPI RIVER"` | `.\huffman_trace.exe "MISSISSIPPI RIVER"` |

互動網頁（用瀏覽器開；也可以直接在檔案總管點兩下）：

| macOS | Linux / WSL | Windows PowerShell |
|---|---|---|
| `open ../slides/media_size.html` | `xdg-open ../slides/media_size.html` | `start ..\slides\media_size.html` |
| `open ../slides/huffman_steps.html` | `xdg-open ../slides/huffman_steps.html` | `start ..\slides\huffman_steps.html` |

## 2. 第二節｜WAV 與 PCM

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| aliasing：3000 Hz 與 5000 Hz（2.2） | `python3 alias_demo.py` | `python alias_demo.py` |
| 量化與 SQNR 的表（2.3、2.4） | `python3 quantize_demo.py` | `python quantize_demo.py` |
| 振幅減半，SQNR 少 6 dB | `python3 quantize_demo.py 0.5` | `python quantize_demo.py 0.5` |
| 聽 4-bit 量化的聲音 | macOS：`open out_4bit.wav`；Linux：`xdg-open out_4bit.wav` | `start out_4bit.wav` |
| 讀真實語音的 WAV 檔頭（2.6、2.7） | `python3 wav_info.py ../data/speech_osr_8k.wav` | `python wav_info.py ..\data\speech_osr_8k.wav` |
| 讀真實音樂的 WAV 檔頭 | `python3 wav_info.py ../data/music_sousa_44k.wav` | `python wav_info.py ..\data\music_sousa_44k.wav` |
| MP2 參考程式（2.8；需要 numpy） | `python3 ../../../samples_2025-python/mini_project_2/wavegen.py 8000 16 1 sine 440 1.0 1 sine16.wav` | `python ..\..\..\samples_2025-python\mini_project_2\wavegen.py 8000 16 1 sine 440 1.0 1 sine16.wav` |

沒有 numpy 的話：`python3 -m pip install numpy`（Windows：`python -m pip install numpy`）。

互動網頁：

| macOS | Linux / WSL | Windows PowerShell |
|---|---|---|
| `open ../slides/a2d_steps.html` | `xdg-open ../slides/a2d_steps.html` | `start ..\slides\a2d_steps.html` |
| `open ../slides/wav_bmp_viewer.html` | `xdg-open ../slides/wav_bmp_viewer.html` | `start ..\slides\wav_bmp_viewer.html` |

## 3. 第三節｜Team 1：starter 與 Python 完整版

先從 `examples` 走到 starter，編譯並跑測試：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd ../../../team_projects/team1_textlink/starter` | `cd ..\..\..\team_projects\team1_textlink\starter` |
| `make` | `mingw32-make` |
| `make test` | `mingw32-make test` |
| `./textlink` | `.\textlink.exe` |

`make test` 會印 `PASS 0、FAIL 0、TODO 46`，最後有一行 `Error 1`：這是正常的，代表 5 個 place holder 還沒寫。全部寫完會變成 `PASS 82`。

聊天要開**兩個終端機視窗**（都先 `cd` 到 starter）：

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 視窗 A：等人連進來 | `./textlink chat server 5000` | `.\textlink.exe chat server 5000` |
| 視窗 B：連過去（同一台電腦） | `./textlink chat client 127.0.0.1 5000` | `.\textlink.exe chat client 127.0.0.1 5000` |
| 視窗 B：連到別台電腦 | `./textlink chat client <對方的IP> 5000` | `.\textlink.exe chat client <對方的IP> 5000` |
| 查自己的 IP | macOS：`ipconfig getifaddr en0`；Linux：`hostname -I` | `ipconfig`（看「IPv4 位址」） |

畫面上的黃字會列出還沒完成的 TODO。聊天畫面裡可以打 `/help`、`/files`、`/send 檔名`、`/raw`、`/huff`、`/quit`。

**Python 完整版**（可以真的聊天、傳檔、看 Huffman 的中間過程；也能和你們的 C 版互連）：

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 走到資料夾 | `cd ../python_ref` | `cd ..\python_ref` |
| 分析一個檔案的壓縮率 | `python3 textlink.py inspect ../../../lectures/wk03_0921_team1-kickoff/data/speech_osr_8k.wav` | `python textlink.py inspect ..\..\..\lectures\wk03_0921_team1-kickoff\data\speech_osr_8k.wav` |
| 同上，印出中間過程 | 後面再加 ` --probe` | 後面再加 ` --probe` |
| 視窗 A：聊天 server | `python3 textlink.py chat server 5000` | `python textlink.py chat server 5000` |
| 視窗 B：聊天 client | `python3 textlink.py chat client 127.0.0.1 5000` | `python textlink.py chat client 127.0.0.1 5000` |
| 視窗 A：收檔案 | `python3 textlink.py recv 5001 received` | `python textlink.py recv 5001 received` |
| 視窗 B：送檔案（結束時印 STATS） | `python3 textlink.py send 127.0.0.1 5001 textlink.py --huff` | `python textlink.py send 127.0.0.1 5001 textlink.py --huff` |

**測黏包與半包（3.3）**：視窗 A 先開 **Python 版**的 chat server，視窗 B 從 `python_ref` 打（等你們的 C 版寫完 TODO 1、2，再把視窗 A 換成自己的 `textlink` 來測）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 ../../../lectures/wk03_0921_team1-kickoff/examples/chunk_send.py 127.0.0.1 5000` | `python ..\..\..\lectures\wk03_0921_team1-kickoff\examples\chunk_send.py 127.0.0.1 5000` |

視窗 A 應該出現 6 則分開、內容完整的訊息：5 則「黏包測試」加 1 則「半包測試」（裡面的 emoji 不能破）。

## 常見狀況

| 看到什麼 | 怎麼辦 |
|---|---|
| `make : 無法辨識 'make' 詞彙…`（Windows） | 打 `mingw32-make` |
| `python3` 開啟 Microsoft Store 或沒反應（Windows） | 打 `python` |
| `./huffman_trace : 無法辨識…`（Windows） | 用反斜線並加 `.exe`：`.\huffman_trace.exe` |
| `No such file or directory` | 先打 `pwd` 看自己在哪個資料夾，對照每一節開頭說的位置 |
| `bind: Address already in use`／連不上 | 上一個 server 還沒關，或 port 被佔用：換一個 port（例如 5002），兩邊要用同一個 |
| 跨電腦連不上 | 確認兩台在同一個網路；Windows 第一次執行會跳防火牆視窗，要按「允許」；校園 Wi-Fi 不通就改用手機熱點 |
