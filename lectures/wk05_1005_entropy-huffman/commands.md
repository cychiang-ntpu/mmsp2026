# 第 5 週｜上課跟著打的指令（一頁版）

這一頁把[本週講義](README.md)裡散在各節的指令依上課順序收在一起。左欄 macOS／Linux／WSL，右欄 Windows 的 VSCode PowerShell。
每一格都可以直接複製貼上。

**Windows 三個差別**：`make` 改打 `mingw32-make`、`python3` 改打 `python`、執行自己編出來的程式前面加 `.\`、後面有 `.exe`。
中文如果印成亂碼，先打 `chcp 65001`。

## 0. 開始之前：拿到本週內容

**電腦教室的公用電腦**：先做第 3 週的 [setup_first.md](../wk03_0921_team1-kickoff/setup_first.md)（貼一行指令裝好 Git、gcc、make、Python，並把課程 repo 抓到桌面）。自己的筆電照下表：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `cd mmsp2026` | `cd mmsp2026` |
| `git pull` | `git pull` |
| `cd lectures/wk05_1005_entropy-huffman/examples` | `cd lectures\wk05_1005_entropy-huffman\examples` |
| `make` | `mingw32-make` |

看到 `bitio_trace` 與 `huffman_build`（Windows 是 `.exe`）兩支程式就成功了。以下第 1、2 節的指令都在這個 `examples` 資料夾裡打。

## 1. 第一節｜從作業到 bytes

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 回家作業對答案（1.1；第 3 週的程式） | `../../wk03_0921_team1-kickoff/examples/huffman_trace "MISSISSIPPI RIVER"` | `..\..\wk03_0921_team1-kickoff\examples\huffman_trace.exe "MISSISSIPPI RIVER"` |
| 第 3 週的程式還沒編譯的話 | `make -C ../../wk03_0921_team1-kickoff/examples` | `mingw32-make -C ..\..\wk03_0921_team1-kickoff\examples` |
| bit writer／reader：ABRACADABRA → 6E 8A DC（1.2） | `./bitio_trace` | `.\bitio_trace.exe` |
| 自己給字串與 code 表 | `./bitio_trace "多媒體多媒多" --codes 多:0,媒:10,體:11` | `.\bitio_trace.exe "多媒體多媒多" --codes 多:0,媒:10,體:11` |
| 不告訴 reader 該停：補位的 0 被解成符號（1.3） | `./bitio_trace --no-stop` | `.\bitio_trace.exe --no-stop` |
| MP3 參考實作：定長編碼（1.4） | `python3 ../../../samples_2025-python/mini_project_3/flc_codec.py encode ../data/mississippi_river.txt mr3.csv mr3.bin` | `python ..\..\..\samples_2025-python\mini_project_3\flc_codec.py encode ..\data\mississippi_river.txt mr3.csv mr3.bin` |
| 看 codebook | `cat mr3.csv` | `type mr3.csv` |
| 看 bin 的 bytes | `od -An -tx1 mr3.bin`（macOS 也可 `xxd mr3.bin`） | `Format-Hex mr3.bin` |
| 檢查 codebook，印出位元流與補位（1.4、2.5） | `python3 codebook_check.py mr3.csv mr3.bin --bits` | `python codebook_check.py mr3.csv mr3.bin --bits` |
| MP3 的 decode | `python3 ../../../samples_2025-python/mini_project_3/flc_codec.py decode mr3.bin mr3.csv mr3_out.txt` | `python ..\..\..\samples_2025-python\mini_project_3\flc_codec.py decode mr3.bin mr3.csv mr3_out.txt` |
| 確認還原 | `cmp ../data/mississippi_river.txt mr3_out.txt && echo same` | `fc.exe /b ..\data\mississippi_river.txt mr3_out.txt` |
| 中英文混合檔的 FLC 大小（162 → 93 bytes） | `python3 ../../../samples_2025-python/mini_project_3/flc_codec.py encode ../../wk02_0914_text-utf8/data/sample_zh_en.txt zh3.csv zh3.bin && wc -c zh3.bin` | `python ..\..\..\samples_2025-python\mini_project_3\flc_codec.py encode ..\..\wk02_0914_text-utf8\data\sample_zh_en.txt zh3.csv zh3.bin; (Get-Item zh3.bin).Length` |

## 2. 第二節｜Huffman 的 C 實作

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 兩個佇列建樹：預設 ABRACADABRA（2.1） | `./huffman_build` | `.\huffman_build.exe` |
| 講義的 8 色影像（L 仍是 2.0938） | `make trace` | `mingw32-make trace` |
| 換成中文 | `./huffman_build "多媒體多媒多"` | `.\huffman_build.exe "多媒體多媒多"` |
| 整個檔案＋EOF 符號（和 MP4 一樣的符號集） | `./huffman_build --file ../data/mississippi_river.txt --eof` | `.\huffman_build.exe --file ..\data\mississippi_river.txt --eof` |
| K 很大：第 2 週的講義當輸入（只印結果，不逐輪印） | `./huffman_build --file ../../wk02_0914_text-utf8/README.md` | `.\huffman_build.exe --file ..\..\wk02_0914_text-utf8\README.md` |
| MP4 參考實作（2.4） | `python3 ../../../samples_2025-python/mini_project_4/huffman_codec.py encode ../data/mississippi_river.txt mr4.csv mr4.bin` | `python ..\..\..\samples_2025-python\mini_project_4\huffman_codec.py encode ..\data\mississippi_river.txt mr4.csv mr4.bin` |
| 看 codebook（依長度、code 排序） | `cat mr4.csv` | `type mr4.csv` |
| 檢查 codebook＋bin（2.5） | `python3 codebook_check.py mr4.csv mr4.bin --bits` | `python codebook_check.py mr4.csv mr4.bin --bits` |
| C 與 Python 的 N、K、H、L、bits 相同 | `make check` | `mingw32-make check` |
| 故意弄壞 codebook 再檢查（改一個 code 後存檔） | `python3 codebook_check.py mr4.csv mr4.bin` | `python codebook_check.py mr4.csv mr4.bin` |
| 個人 repo 的自動測試（在你的作業 repo 根目錄） | `bash ../mmsp2026/tools/ci/run_tests.sh mp3 mp4` | Git Bash：同左 |

互動網頁（第 3 週的建樹動畫，今天 2.1 對照用）：

| macOS | Linux / WSL | Windows PowerShell |
|---|---|---|
| `open ../../wk03_0921_team1-kickoff/slides/huffman_steps.html` | `xdg-open ../../wk03_0921_team1-kickoff/slides/huffman_steps.html` | `start ..\..\wk03_0921_team1-kickoff\slides\huffman_steps.html` |

## 3. 第三節｜Team 1 收尾

先在 `examples` 裡（3.0–3.3 的工具）：

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 逐欄拆開 python_ref 的區塊：ABRACADABRA 44 bytes（3.1） | `python3 block_dump.py` | `python block_dump.py` |
| 真實語音的 s16 區塊（檔頭、codebook 12,343 組、head、bitstream） | `python3 block_dump.py --file ../../wk03_0921_team1-kickoff/data/speech_osr_8k.wav` | `python block_dump.py --file ..\..\wk03_0921_team1-kickoff\data\speech_osr_8k.wav` |
| 把區塊存成檔案，拿去餵自己的 `huff_decode` | `python3 block_dump.py --file 某.txt --out block.bin` | `python block_dump.py --file 某.txt --out block.bin` |
| 只有長度怎麼重建 code、怎麼解碼（3.2） | `./canon_decode_trace` | `.\canon_decode_trace.exe` |
| 壞的長度表要拒絕 | `./canon_decode_trace --lens A:1,B:1,C:1` | `.\canon_decode_trace.exe --lens A:1,B:1,C:1` |
| 位元流不夠也要拒絕 | `./canon_decode_trace --n 20` | `.\canon_decode_trace.exe --n 20` |
| 報告表：語音檔以 s16、以 byte 為符號（3.3） | `make report` | `mingw32-make report` |
| 自己的檔案（.txt 用 char） | `./huffman_build --file 某.txt` | `.\huffman_build.exe --file 某.txt` |
| 自己的 WAV | `./huffman_build --file 某.wav --sym s16` | `.\huffman_build.exe --file 某.wav --sym s16` |

然後到 starter：

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 到 starter | `cd ../../../team_projects/team1_textlink/starter` | `cd ..\..\..\team_projects\team1_textlink\starter` |
| 離線測試（目標 82 個 PASS） | `make test` | `mingw32-make test` |
| 看 python_ref 的區塊格式與數字（3.1、3.3） | `python3 ../python_ref/textlink.py --probe inspect ../../../lectures/wk03_0921_team1-kickoff/data/speech_osr_8k.wav` | `python ..\python_ref\textlink.py --probe inspect ..\..\..\lectures\wk03_0921_team1-kickoff\data\speech_osr_8k.wav` |
| 兩台電腦傳 WAV（監聽端） | `./textlink recv 5000 out` | `.\textlink.exe recv 5000 out` |
| 兩台電腦傳 WAV（連線端，換成對方 IP） | `./textlink send 192.168.43.1 5000 a.wav --huff` | `.\textlink.exe send 192.168.43.1 5000 a.wav --huff` |
| 確認逐 byte 相同 | `cmp a.wav out/a.wav && echo same` | `fc.exe /b a.wav out\a.wav` |
| 壞輸入：黏包與半包（第 3 週的工具） | `python3 ../../../lectures/wk03_0921_team1-kickoff/examples/chunk_send.py 127.0.0.1 5000` | `python ..\..\..\lectures\wk03_0921_team1-kickoff\examples\chunk_send.py 127.0.0.1 5000` |

## 常見錯誤訊息

| 訊息 | 原因／處理 |
|---|---|
| `make: *** No rule to make target 'check'` | 不在 `examples` 資料夾裡；`cd` 過去再打 |
| `codebook_check.py`：`把 bin 解到底都沒遇到 EOF` | bit order 反了（一個 byte 裡先填了最低位）、或忘了寫 EOF、或 codebook 的 code 和 bin 不是同一次產生的 |
| `codebook_check.py`：`解出 N 個符號，codebook 的次數加起來是 M 個` | 補位沒處理好、最後一個 byte 多寫或少寫、或 encoder 統計的次數和實際寫出的不一致 |
| `codebook_check.py`：`不是 prefix code` | 某個 code 是另一個的開頭：建樹時符號沒有全放在葉上，或 code 字串寫反 |
| `run_tests.sh`：`codebook.csv 與參考不同`（MP3） | 排序規則、機率小數位數、跳脫、`\r`；用 `diff 你的.csv 參考.csv` 看第一個不同的地方 |
| `run_tests.sh`：`encoded.bin 大小 a bytes，參考 b bytes`（MP4） | 總 bits 不對：EOF 沒算進去、補位多了一個 byte、或樹不是最佳的（用 `codebook_check.py` 看 H ≤ L < H + 1） |
| `run_tests.sh`：`用 Python decoder 解你的 bin＋codebook，還原結果與原文不同` | 你的 codebook 不是合法 prefix code、或 CSV 格式 Python 讀不懂（code 要有引號、符號要跳脫） |
| Windows：中文印成亂碼 | 先打 `chcp 65001`；程式本身已經 `SetConsoleOutputCP(CP_UTF8)` |
