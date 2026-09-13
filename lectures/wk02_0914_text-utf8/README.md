# 第 2 週（2026/9/14）｜文字表示、UTF-8、MP1 導讀、TCP 黏包

對應：MP1、Team 1 前導
時間：13:10–16:00，三節。

> **課前準備（同學）**：clone 本 repo，照 [vscode_c_starter.md](../../docs/tutorials/vscode_c_starter.md)（Windows）、
> [macos_c_starter.md](../../docs/tutorials/macos_c_starter.md) 或 [wsl_c_starter.md](../../docs/tutorials/wsl_c_starter.md)
> 裝好 gcc 與 Python 3，在 [examples/](examples/) 能編譯成功
> （macOS／Linux 打 `make`；Windows PowerShell 打 `mingw32-make`，見 [examples/README.md](examples/README.md)）。
>
> **指令怎麼讀**：本講義每段指令都分兩欄，左邊 macOS／Linux 終端機，右邊 Windows 的 VSCode PowerShell。
> Windows 執行檔要寫 `.\xxx.exe`，路徑用 `\`；中文亂碼先打 `chcp 65001`；`python3` 在 Windows 通常叫 `python`。
> **WSL 同學看左欄**（你在 Linux 裡），但和同學互連前先讀 wsl_c_starter.md 的 6-4。

## 本週目標

上完課你應該能：

1. 說出「文字檔在硬碟上就是一串 bytes」，並用 hex 工具（`hexdump`、`Format-Hex` 或 VSCode Hex Editor）數出一個中文字佔 3 bytes。
2. 讀懂 UTF-8 的前導 byte 規則，寫出判斷「這個字元幾 bytes」的函式。
3. 說明 MP1 的輸入、輸出格式與排序規則，並用 `diff` 對照 Python 參考答案。
4. 從去年三份樣本判斷「為什麼這份拿 0 分、75 分、100 分」。
5. 親眼看到 TCP 黏包，並說出為什麼 Team 1 一定要做長度前綴。

## 時程

| 節次 | 時間 | 內容 | 教材／範例 |
|---|---|---|---|
| 1 | 13:10–14:00 | 多媒體系統總覽到「資料就是 bytes」；ASCII、Big5、UTF-8；`hexdump`／`Format-Hex`／Hex Editor 現場數 byte | [data/sample_zh_en.txt](data/sample_zh_en.txt)、[examples/utf8_dump.c](examples/utf8_dump.c) |
| 2 | 14:10–15:00 | MP1 題意、正確輸出長相、100/0/75 分樣本導讀、`-Wall` 警告、`diff` 對答案 | [samples_2025-C/mini_project_1/](../../samples_2025-C/mini_project_1/)、[symbol_stats.py](../../samples_2025-python/mini_project_1/symbol_stats.py) |
| 3 | 15:10–16:00 | chat.c 互連；socket 骨架；黏包實驗；下週預告與回家作業 | [chat.c](../../team_projects/team1_textlink/baseline/chat.c)、[examples/sticky_send.c](examples/sticky_send.c) |

---

## 第一節｜從多媒體到「資料就是 bytes」

### 1.1 本學期的地圖（8 分鐘）

多媒體系統 = 輸入裝置 → 數位資料 → 處理／壓縮／傳輸 → 輸出裝置。
本學期三條線各對應一次 Team Project，終點是自己寫的 MiniLINE：

| 資料 | 表示 | 壓縮 | 傳輸 | Team |
|---|---|---|---|---|
| 文字 | UTF-8 | 熵編碼（FLC、Huffman） | TCP 封包 | Team 1 TextLink（9/21 開題） |
| 聲音 | PCM、WAV | （STFT 分析） | UDP、jitter buffer | Team 2 VoiceLink |
| 影像 | BMP、YCbCr | JPEG | 分片、MJPEG | Team 3 MiniLINE |

補充閱讀：2023 課程 [Chapter 1 Introduction](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023/tree/master/Chapter-1)
（多媒體作為訊號與系統、日常系統、產業與會議）。2026 年的產業／會議清單見 [slides/outline.md](slides/outline.md) 末段。

### 1.2 文字怎麼變成 bytes（15 分鐘）

> 完整的歷史脈絡與參考文獻見 [encoding_history.md](encoding_history.md)（課後閱讀，約 15 分鐘）。
> 課堂上只走這條時間軸，重點是**每一代都在解決上一代的問題**：

| 年 | 編碼 | 解決了什麼 | 留下什麼問題 |
|---|---|---|---|
| 1963 | **ASCII** | 7 bits、128 個符號，統一了英文與控制字元（`\n`=10、`\r`=13、`\t`=9） | 只有英文 |
| 1980s | code page（Latin-1、CP1252…） | 用第 8 個 bit 放各國字元 | 同一個 byte 各國解讀不同，亂碼 |
| 1984 | **Big5** | 2 bytes 一個中文字，13,053 字（常用 5,401＋次常用 7,652），台灣沿用至今 | 第二個 byte 可能是 `\`（許功蓋問題）；找不到字元邊界；缺字要造字 |
| 1991 | **Unicode** | 全世界的字放進一張表，每個字一個編號 U+XXXX（「多」= U+591A） | 只說了「編號」，沒說怎麼存 |
| 1992 | **UTF-8** | Thompson 與 Pike 設計：與 ASCII 相容、自同步、前導 byte 自述長度 | 一個中文字變 3 bytes；Windows 內部仍用 UTF-16，衍生 BOM 問題 |

一句話：**Unicode 給字編號，UTF-8 決定編號怎麼存成 bytes。** 現在網頁、Git、Slack、Python 3 全是 UTF-8。

UTF-8 的規則只有一張表：

| code point 範圍 | bytes | 前導 byte | 續位元組 |
|---|---|---|---|
| U+0000–U+007F | 1 | `0xxxxxxx` | 無 |
| U+0080–U+07FF | 2 | `110xxxxx` | `10xxxxxx` |
| U+0800–U+FFFF | 3 | `1110xxxx` | `10xxxxxx 10xxxxxx` |
| U+10000–U+10FFFF | 4 | `11110xxx` | `10xxxxxx ×3` |

看前導 byte 就知道這個字元幾 bytes；續位元組永遠是 `10xxxxxx`，
所以從任何位置都能往回找到字元開頭（[chat.c 第 237 行](../../team_projects/team1_textlink/baseline/chat.c#L237) 折行就是這樣做的）。

### 1.3 現場 demo：數 byte（10 分鐘）

先進到本週資料夾（兩個平台都一樣，PowerShell 也接受 `/`）：

```
cd lectures/wk02_0914_text-utf8
```

| 要看什麼 | macOS / Linux | Windows PowerShell |
|---|---|---|
| 看起來是文字 | `cat data/sample_zh_en.txt` | `Get-Content data\sample_zh_en.txt -Encoding utf8` |
| 其實是 bytes | `hexdump -C -n 64 data/sample_zh_en.txt` | `Format-Hex data\sample_zh_en.txt \| Select-Object -First 6` |
| 幾 bytes？ | `wc -c data/sample_zh_en.txt` | `(Get-Item data\sample_zh_en.txt).Length` |
| 幾個字元？ | `wc -m data/sample_zh_en.txt` | `python -c "print(len(open('data/sample_zh_en.txt','rb').read().decode('utf-8')))"` |

兩個數字為什麼不同？（VSCode 同學也可以裝 Hex Editor 擴充套件直接看。）
macOS／Linux 若兩個數字一樣，是終端機沒設 UTF-8 語系：先打 `export LANG=en_US.UTF-8` 再跑一次。

問題：`多媒體訊號處理：把系統做出來。` 這行有幾個字元、幾 bytes？（答案：15 個字元，45 bytes，加換行 46）

再用我們自己寫的 40 行程式驗證：

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd examples` | `cd examples` |
| `make` | `mingw32-make` |
| `./utf8_dump < ../data/sample_zh_en.txt` | `cmd /c ".\utf8_dump.exe < ..\data\sample_zh_en.txt"` |

（PowerShell 不支援 `<` 輸入重導向，所以 Windows 這類指令一律用 `cmd /c "..."` 包起來；或直接打 `mingw32-make demo`。）

輸出每列是「第幾個字元、幾 bytes、hex、code point、字元本身」。
請同學找出：emoji 😀 幾 bytes？`\r` 在哪一行？這就是 MP1 要處理的全部特殊情況。

同一件事的 Python 版 [utf8_dump.py](examples/utf8_dump.py) 只用 `bytes.decode`，看不到「前導 byte」的邏輯，
但輸出格式與 C 版逐 byte 相同。`make check`（Windows `mingw32-make check`）會同時跑兩版並 diff，
沒有差異就代表你的 C 程式解碼正確。這就是本學期「C 實作、Python 對答案」的模式。

### 1.4 工具箱：怎麼 byte by byte 看任何檔案（5 分鐘，其餘課後讀）

本學期從文字、WAV 到 JPEG，每一種資料最後都要用這些工具看它「真正的樣子」。至少學會一個命令列工具和一個 GUI。

| 工具 | 平台 | 怎麼用 | 說明 |
|---|---|---|---|
| `hexdump -C` | macOS、Linux 內建 | `hexdump -C -n 32 data/sample_zh_en.txt` | **首選**。左邊 hex、右邊 ASCII，`-n` 限制 bytes 數，`-s` 指定起點 |
| `xxd` | macOS 內建；Linux 隨 vim 安裝 | `xxd -l 32 data/sample_zh_en.txt` | 與 hexdump 類似；`xxd -r` 可以把 hex 轉回 bytes，改檔案用 |
| `od` | macOS、Linux 內建 | `od -A x -t x1 -N 32 data/sample_zh_en.txt` | 最古老的一個，任何 Unix 都有 |
| `Format-Hex` | Windows PowerShell 內建 | `Format-Hex data\sample_zh_en.txt \| Select-Object -First 4` | 每列 16 bytes，右邊有 ASCII |
| Python 一行 | 三平台（已裝 Python） | `python3 -c "print(open('data/sample_zh_en.txt','rb').read(32).hex(' '))"` | 指令三平台完全相同；Windows 用 `python` |
| **VSCode Hex Editor** | 三平台 | 擴充套件 `ms-vscode.hexeditor`；檔案上按右鍵「Open With…」選 Hex Editor | 課程建議的 GUI，寫作業時可直接看 WAV、JPEG 的檔頭 |
| HxD | Windows | 免費，<https://mh-nexus.de/en/hxd/> | Windows 最常用的獨立 hex 編輯器 |
| Hex Fiend | macOS | 免費，<https://hexfiend.com/> | 可開數 GB 的大檔 |
| GHex／Okteta | Linux | 套件管理器安裝 | GNOME／KDE 的 hex 編輯器 |
| `utf8_dump` | 三平台（本週寫的） | 見 1.3 | 不只看 bytes，還幫你切成 UTF-8 字元 |

三個命令列工具的輸出長這樣（同一個檔案的前 16 bytes）：

```
$ hexdump -C -n 16 data/sample_zh_en.txt
00000000  48 65 6c 6c 6f 2c 20 4d  4d 53 50 20 32 30 32 36  |Hello, MMSP 2026|

$ xxd -l 16 data/sample_zh_en.txt
00000000: 4865 6c6c 6f2c 204d 4d53 5020 3230 3236  Hello, MMSP 2026

PS> Format-Hex data\sample_zh_en.txt | Select-Object -First 4
           00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000   48 65 6C 6C 6F 2C 20 4D 4D 53 50 20 32 30 32 36  Hello, MMSP 2026
```

讀法：最左邊是**位移**（offset，從檔頭數第幾個 byte，十六進位），中間是 bytes 的 hex，右邊把可見 ASCII 印出來、
不可見的印 `.`。中文在右欄一律是 `...`，因為每個 byte 單獨看都不是 ASCII。
下週 Team 1 抓封包、第 7 週（10/19）看 WAV 檔頭的 `RIFF`、第 12 週（11/23）看 JPEG 的 `FF D8`，用的都是同一招。

### 1.5 BOM：看不見，但會咬人（12 分鐘）

**它是什麼。** BOM（Byte Order Mark）是 Unicode 字元 U+FEFF，UTF-8 存成 3 bytes `EF BB BF`。
它原本是 UTF-16 用來標示位元組順序的記號；UTF-8 沒有順序問題，所以 Unicode 標準說 UTF-8 的 BOM
「既不要求也不建議」。但很多工具還是會寫，於是它成了同學抓資料時最常踩的坑。

**你會在哪裡碰到它。**

| 來源 | 會不會有 BOM |
|---|---|
| Windows 記事本存成「UTF-8」 | 舊版一定有；Windows 10 1903 後預設無 BOM，但選單裡仍有「UTF-8 with BOM」 |
| Excel「另存新檔 → CSV UTF-8」 | **一定有**。這是為了讓 Excel 自己重開時認得編碼 |
| PowerShell 5 的 `Out-File -Encoding utf8`、`>` 重導向 | 有（PowerShell 7 改為無 BOM） |
| 從網站、政府開放資料平台下載的 CSV／JSON | 常有，尤其是 Windows 主機產生的檔案 |
| HTTP 回應的 JSON | 偶爾有，`JSON.parse` 與多數解析器會直接報錯 |
| VSCode、macOS、Linux 工具、Git、Python 的預設 | 無 |

**它會造成什麼。** 檔案第一個「字」多了 3 bytes：CSV 第一欄名稱變成 `\ufeffid`、
JSON 解析失敗、`gcc` 對 `.c` 檔報 `stray '\357' in program`、shell script 第一行 `#!/bin/bash` 不被認得、
你的 MP1 多算一個看不見的符號、比對檔案時「明明一樣卻 diff 不過」。

**現場做一次。** [data/sample_bom.txt](data/sample_bom.txt) 內容與第一個檔案前兩行相同，但檔頭多了 BOM：

| macOS / Linux | Windows PowerShell |
|---|---|
| `hexdump -C -n 8 data/sample_bom.txt` | `Format-Hex data\sample_bom.txt \| Select-Object -First 3` |
| `./utf8_dump < ../data/sample_bom.txt \| head -2` | `cmd /c ".\utf8_dump.exe < ..\data\sample_bom.txt" \| Select-Object -First 2` |
| `diff data/sample_bom.txt <(head -2 data/sample_zh_en.txt)` | `fc.exe data\sample_bom.txt data\sample_zh_en.txt` |

hex 的前 3 bytes 是 `EF BB BF`；utf8_dump 第 0 個字元標示 `(BOM)`；`diff` 說第一行不同，但用眼睛看完全一樣。
Windows 同學再自己做一個：

```
"hello" | Out-File -Encoding utf8 bom_test.txt      # PowerShell 5
Format-Hex bom_test.txt | Select-Object -First 2     # 看到 EF BB BF 了嗎？
```

**怎麼處理。** 原則是：**BOM 是標記，不是內容；讀入時偵測並跳過，寫出時不要加。**

- C：讀檔後檢查前 3 bytes，是 `EF BB BF` 就從第 4 個 byte 開始處理。
  [examples/utf8_dump.c](examples/utf8_dump.c) 只是印出來讓你看見；MP1 要真的跳過。
- Python：`open(path, encoding="utf-8-sig")`，有 BOM 自動去掉、沒有也不會出錯。課程的 Python 參考實作就是這樣寫。
- 命令列一次去掉：macOS／Linux `tail -c +4 in.txt > out.txt`（先確認真的有 BOM 再用），
  或 `sed -i '' $'1s/^\xEF\xBB\xBF//' in.txt`；PowerShell 7 `Get-Content in.txt | Set-Content -Encoding utf8NoBOM out.txt`。
- VSCode：右下角點「UTF-8 with BOM」→ Save with Encoding → UTF-8。

**本課程的規則（MP1、MP3、MP4 都適用）：** 輸入檔開頭若有 BOM，跳過它、不計入任何統計；
輸出檔一律不寫 BOM。自動測試的私有測資含有帶 BOM 的檔案。去年的 C 樣本把 BOM 當一個符號計數，
**今年不再如此**，這是今年與去年樣本唯二的差異之一（另一個是檔名參數）。

**與 Team 1 的關係**：TCP 送出「多媒體」是 9 bytes，如果對方 `recv` 只收到前 7 bytes，
第三個字就是壞的。Team 1 的驗收項「中文訊息 round-trip」與「半包／黏包」都源自這裡。

---

## 第二節｜MP1 實作導讀：怎麼寫、怎麼錯

### 2.1 題意（10 分鐘）

> 正式規格就是 [samples_2025-C/mini_project_1/README.md](../../samples_2025-C/mini_project_1/README.md) 連到的[作業規定](https://hackmd.io/@c5tGGitKRe-78lHzuuOkIg/mmsp-2025-mini-project-1)；以下是摘要。評分測資今年不同，照抄樣本過不了。

- 執行：**今年改為檔名參數** `./mp1 input.txt output.csv`，與 Python 版相同，才能自動測試
  （去年樣本是 `< input > output` 從 stdin 讀，只差開頭 `fopen` 幾行）。
- 輸入：文字檔，可能混合 ASCII、Big5 與 UTF-8。
- 輸出 CSV，每列 `"符號",次數,機率`，機率 15 位小數。
- 特殊符號：`\n`、`\r`、`\t` 輸出成 `"\n"`、`"\r"`、`"\t"`；雙引號依 CSV 慣例寫成 `""""`。
- 排序：次數多到少；同次數者 byte 長度短到長；再依 byte 值。

先看正確答案長什麼樣：

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../../samples_2025-python/mini_project_1/symbol_stats.py data/sample_zh_en.txt py.csv` | `python ..\..\samples_2025-python\mini_project_1\symbol_stats.py data\sample_zh_en.txt py.csv` |
| `head -20 py.csv` | `Get-Content py.csv -Encoding utf8 \| Select-Object -First 20` |

（`py.csv` 產生在本週資料夾，`.csv` 已被 .gitignore 排除，不會誤 commit。）

留意最前面幾列：空白、`"\n"`、`"\t"`、`""""`。這四個就是去年最常扣分的地方。

### 2.2 逐段讀 100 分的程式（20 分鐘）

[mini_prj_1_100.c](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c)，依序看四個區塊：

| 行 | 做什麼 | 重點 |
|---|---|---|
| [8–13](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c#L8-L13) | `Symb` 結構：bytes、長度、次數、機率 | 符號不是 `char`，是「最多 4 bytes 的一小段」 |
| [26–36](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c#L26-L36) | `utf8_len`、`is_utf8_follow` | 和我們的 utf8_dump.c 一模一樣 |
| [78–134](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c#L78-L134) | 讀一個符號：先試 UTF-8，失敗 `ungetc` 退回再試 Big5，都不是就當 1 byte | `ungetc` 是「讀錯了放回去」 |
| [39–47](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c#L39-L47) | `qsort` 的比較函式 | 三層排序條件寫成三個 `if` |
| [49–63](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c#L49-L63) | `csv_char` 輸出符號 | 特殊符號與雙引號在這裡處理 |

### 2.3 猜分數：三份對照（15 分鐘）

先不看評語，讓同學讀 2 分鐘後猜「幾分、為什麼」：

- [LOW/mini_prj_1_0.c](../../samples_2025-C/mini_project_1/LOW/mini_prj_1_0.c)：
  用 `wchar_t` 與 `fgetwc` 讀檔，指望 C 函式庫幫忙解碼。
  在多數環境 locale 不對，整個輸出空白。**教訓：MP1 要你自己解 bytes，不要交給 locale。**
- [MEDIUM/mini_prj_1_75.c](../../samples_2025-C/mini_project_1/MEDIUM/mini_prj_1_75.c)：
  結構對了，但機率分母算錯、`\n` `\r` 沒輸出、雙引號沒雙寫。
  **教訓：核心演算法對只拿一半分數，格式邊界是另一半。**
- [HIGH/mini_prj_1_90.c](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_90.c)：
  只差雙引號。順便看 [第 160 行](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_90.c#L160)：

  ```
  warning: result of comparison of constant 65536 with expression of type 'unsigned char' is always true
  ```

  `unsigned char` 最大 255，永遠小於 65536，這個 `if` 是死的。程式仍然能跑，但
  **`-Wall` 的警告是在告訴你「你以為的檢查其實沒發生」**。本學期作業一律開 `-Wall -Wextra`。

### 2.4 對答案：diff 就是你的 CI（5 分鐘）

| macOS / Linux | Windows PowerShell |
|---|---|
| `gcc -Wall -Wextra -o mp1 ../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c` | `gcc -Wall -Wextra -o mp1.exe ..\..\samples_2025-C\mini_project_1\HIGH\mini_prj_1_100.c` |
| `./mp1 < data/sample_zh_en.txt > c.csv` | `cmd /c ".\mp1.exe < data\sample_zh_en.txt > c.csv"` |
| `diff c.csv py.csv` | `fc.exe c.csv py.csv` |

沒有輸出（Windows 顯示「找不到任何差異」）就是全對。
今年這件事由 GitHub Actions 自動做：把 [tools/ci/mp-ci.yml](../../tools/ci/mp-ci.yml) 放進個人 repo，
每次 push 就會編譯並和 Python 版比對，本機也可以 `bash ../mmsp2026/tools/ci/run_tests.sh mp1` 跑同一支腳本。
設定步驟見 [github_actions_ci.md](../../docs/tutorials/github_actions_ci.md)，下週開始交 MP 前務必設好。
Windows 這裡刻意用 `cmd /c` 包起來，因為 PowerShell 的 `>` 會把輸出重新編碼成 UTF-16，diff 會全錯。今年 MP1–MP5 的評分就是這種自動比對，
所以「自己先跑 diff」是交作業前的最後一步，也是 Team Project `tests/` 目錄要放的東西。

> 現場會發現 diff 有差異：C 版多了一列 `"\r"`，Python 版沒有。
> 原因在 Python 那邊：`open()` 文字模式會把 `\r\n` 自動轉成 `\n`。
> 這正是去年 60 分樣本被扣的「`\r` 未顯示」。**對答案的工具也可能有 bug，看到差異先想「誰對」。**

---

## 第三節｜TCP 聊天程式與黏包

### 3.1 兩人互連（15 分鐘）

| 步驟 | macOS / Linux | Windows PowerShell |
|---|---|---|
| 編譯 | `cd team_projects/team1_textlink/baseline && make` | `cd team_projects\team1_textlink\baseline; mingw32-make` |
| 電腦 A（server） | `./chat tcp server 5000` | `.\chat.exe tcp server 5000` |
| 電腦 B（client） | `./chat tcp client <A 的 IP> 5000` | `.\chat.exe tcp client <A 的 IP> 5000` |
| 查自己 IP | macOS `ipconfig getifaddr en0`；Linux `hostname -I` | `ipconfig`，看「IPv4 位址」 |

Windows 第一次執行會跳出防火牆詢問，選「允許存取」，否則同學連不進來。
兩人互傳一句中文，確認泡泡顯示正常。同一台電腦測試用 `127.0.0.1`。

### 3.2 socket 骨架（15 分鐘）

只看五個函式，其餘先跳過：

| 函式 | chat.c 位置 | 一句話 |
|---|---|---|
| `socket` | [第 362 行](../../team_projects/team1_textlink/baseline/chat.c#L362) | 跟 OS 要一個通訊端點 |
| `bind` + `listen` + `accept` | [第 375–379 行](../../team_projects/team1_textlink/baseline/chat.c#L375-L379) | server 端：綁 port、等人、接起來 |
| `connect` | [第 406 行](../../team_projects/team1_textlink/baseline/chat.c#L406) | client 端：三方交握 |
| `send` | [第 491 行](../../team_projects/team1_textlink/baseline/chat.c#L491) | 把 `strlen(input)` 個 bytes 丟進串流 |
| `recv` | [第 318 行](../../team_projects/team1_textlink/baseline/chat.c#L318) | 從串流拿**最多** `MSG_LEN-1` bytes，回傳實際拿到幾個 |

今天只要記住一句話：**`recv` 回來的 bytes 數，不一定等於對方 `send` 的 bytes 數。**
TCP 是位元組串流，沒有「一則訊息」的概念。

### 3.3 黏包實驗（15 分鐘）

一邊開 chat server，另一邊用 sticky_send 連續送 5 則不停頓：

| 終端機 | macOS / Linux | Windows PowerShell |
|---|---|---|
| 1（在 baseline/） | `./chat tcp server 5000` | `.\chat.exe tcp server 5000` |
| 2（在 examples/） | `./sticky_send 127.0.0.1 5000 5 0` | `.\sticky_send.exe 127.0.0.1 5000 5 0` |

參數：IP、port、則數 5、間隔 0 ms。沒編譯成功的同學可用 Python 版：`python3 sticky_send.py 127.0.0.1 5000 5 0`（Windows 用 `python`）。
Python 版與 C 版一樣，泡泡數會少於 5 且每次不同。**黏不黏只取決於時序，程式碼一個字都沒改。**

送了 5 則，server 畫面卻通常只有**一顆泡泡** `第1則第2則第3則第4則第5則`，
有時是兩顆（例如 `第1則` 和 `第2則第3則第4則第5則`）。**泡泡數少於 5，而且每次跑可能不一樣**，
這正是重點：接收端拿到幾塊，取決於它什麼時候醒來 `recv`，不取決於你 `send` 了幾次。
再跑一次加上間隔：

| macOS / Linux | Windows PowerShell |
|---|---|
| `./sticky_send 127.0.0.1 5000 5 500` | `.\sticky_send.exe 127.0.0.1 5000 5 500` |

間隔改成 500 ms，變成五顆泡泡。

同樣的程式碼，只差送的節奏，收到的結果就不同。這叫**黏包**；
反過來一則長訊息被拆成兩次 `recv` 叫**半包**。兩者都不是 bug，是 TCP 的本性。

解法是 [nettcpudp_homework.md 作業 3](../../docs/tutorials/nettcpudp_homework.md)：
每則訊息前面先送 4 bytes 長度，收端寫 `recv_all` 湊滿再處理。
這是 Team 1 最低驗收「切段輸入（半包／黏包）仍可還原」的核心。

UDP 為什麼沒這個問題？（`chat udp 5000 127.0.0.1 6000` 與 `chat udp 6000 127.0.0.1 5000` 開兩個試試看；答案：UDP 保留封包邊界，一次 `recvfrom` 就是一包。）

### 3.4 收尾（5 分鐘）

- **MP1–MP5 統一截止 2026/10/23（五）18:00**，push 到個人 repo 並登錄 commit SHA。
- **下週 9/21**：分組（11 組）、Team 1 TextLink 開題，官方 baseline 與固定 API 當天發布。
- 每人三輪各擔任一次 P／D／V 角色，先想想自己第一輪想做哪個。

## 範例程式（examples/）

| 檔案 | 用途 | 執行 |
|---|---|---|
| [utf8_dump.c](examples/utf8_dump.c) | 逐字元印 bytes 與 code point，MP1 的第一塊積木 | `make demo`／`mingw32-make demo` |
| [utf8_dump.py](examples/utf8_dump.py) | 上者的 Python 對照版，輸出逐 byte 相同 | `python3 utf8_dump.py ../data/sample_zh_en.txt` |
| `make check` | C 版與 Python 版輸出 diff | `make check`／`mingw32-make check` |
| [sticky_send.c](examples/sticky_send.c) | 對 chat server 連續 send，重現黏包 | `sticky_send <IP> <port> [則數] [間隔ms]` |
| [sticky_send.py](examples/sticky_send.py) | 上者的 Python 版，不需編譯，參數相同 | `python3 sticky_send.py 127.0.0.1 5000 5 0` |
| [data/sample_zh_en.txt](data/sample_zh_en.txt) | 含中英文、tab、引號、CRLF、emoji 的測試檔 | 給 utf8_dump 與 MP1 用 |
| [data/sample_bom.txt](data/sample_bom.txt) | 檔頭帶 BOM（`EF BB BF`）的 UTF-8 檔 | 示範看不見的 3 bytes |

## 回家作業／下週前要做的事

0. 讀 [encoding_history.md](encoding_history.md) 與其中必讀的 Spolsky 文章（英文，約 20 分鐘）。
1. 讀完 [nettcpudp_homework.md](../../docs/tutorials/nettcpudp_homework.md)，完成**作業 1**（暱稱與時間戳記）與**作業 3**（長度前綴）。
   完成後用 sticky_send 驗證：5 則間隔 0 ms 也要顯示五顆泡泡。
2. 開始寫 MP1：先讓 utf8_dump.c 的邏輯變成「計數」，再加排序與 CSV 輸出。
   用 `diff` 對 Python 參考輸出，遇到 `\r` 的差異回想今天第二節。
3. 把自己的 MP1 push 到個人 repo，確認 `gcc -Wall -Wextra` 無警告。
   Windows 同學：讀檔記得用 `"rb"` 或 `_setmode`（見 utf8_dump.c 開頭），否則 `\r` 會被 C 函式庫吃掉；
   MP1 要能處理帶 BOM 的輸入（跳過、不計入），用 data/sample_bom.txt 與 sample_zh_en.txt 前兩行比對輸出是否相同。

## 參考

- 本週補充閱讀：[encoding_history.md](encoding_history.md)，含 12 篇參考文獻；
  必讀三篇是 Spolsky 的 Unicode 入門文、Pike 的 UTF-8 history、Pike 與 Thompson 1993 年的 USENIX 論文。
- 2023 講義：[Chapter 1 Introduction](https://github.com/cychiang-ntpu/ntpu-ce-mmsp-2023/tree/master/Chapter-1)
- UTF-8 規格：[RFC 3629](https://www.rfc-editor.org/rfc/rfc3629)
- 本 repo：[c_cheatsheet.md](../../docs/tutorials/c_cheatsheet.md)（位元運算、`fgetc`、`qsort`）、
  [c_error_guide.md](../../docs/tutorials/c_error_guide.md)（看懂 warning）
