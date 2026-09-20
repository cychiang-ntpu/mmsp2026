# TextLink starter：會動的殼＋五個 place holder

規格在上一層的 [../README.md](../README.md)。這個資料夾是一支**已經符合規格的命令列介面、網路連線、
聊天畫面、檔案傳輸流程與 `STATS` 輸出**的 C 程式，由 [../baseline/chat.c](../baseline/chat.c) 改寫延伸而來；
但「編碼」的部分全部挖空，留給你們寫：

| # | 檔案 | 函式 | 要做什麼 | 難度 |
|---|---|---|---|---|
| TODO 1 | [src/frame.c](src/frame.c) | `frame_pack_header` | 把 type 與長度打包成 5 bytes 標頭（length 為 big-endian） | ★☆☆ |
| TODO 2 | [src/frame.c](src/frame.c) | `frame_parse_header` | 解析標頭，拒絕 length 為 0 或超過 16 MiB | ★☆☆ |
| TODO 3 | [src/utf8.c](src/utf8.c) | `utf8_validate` | RFC 3629 合法性檢查（overlong、代理區、超過 U+10FFFF） | ★★☆ |
| TODO 4 | [src/huffman.c](src/huffman.c) | `huff_encode` | 切符號（文字＝UTF-8 字元、WAV＝16-bit sample、其他＝byte）→ 統計機率 → 建樹 → codebook → 位元打包 | ★★★ |
| TODO 5 | [src/huffman.c](src/huffman.c) | `huff_decode` | 讀 codebook → 逐 bit 解碼 → 把符號還原成 bytes；對壞資料要回報錯誤而不是當掉 | ★★★ |

place holder 目前一律回傳 `TL_ERR_TODO`。殼看到這個回傳值會告訴你「這一塊還沒實作」，不會當掉：
聊天畫面上方有一行黃字列出還沒做的項目，`send` 會印出錯誤並以結束碼 1 結束。

## 怎麼開始

把整個 `starter/` 的內容複製到你們的 team repo 根目錄（不要在課程 repo 裡直接改），然後：

| | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 編譯 | `make` | `mingw32-make` |
| 離線測試 | `make test` | `mingw32-make test` |
| 清除 | `make clean` | `mingw32-make clean` |

`make test` 會跑 [tests/test_codec.c](tests/test_codec.c)：不用網路，直接測五個 place holder，每一項顯示
`PASS`／`FAIL`／`TODO`。一開始是 46 個 `TODO`，而且 make 會顯示 `Error 1`，這是正常的：**還有 TODO 或 FAIL 時結束碼就不是 0**。
全部寫完後應該是 82 個 `PASS`（Huffman 每個 round-trip 通過後，會再多測兩項壞輸入；還會檢查「以字元／sample 為符號」真的比以 byte 為符號壓得小）。

建議的順序，每完成一步都有東西可以看：

1. **TODO 1、2（約 10 行）** → 聊天打 `/raw` 就能互傳文字；`textlink send … --raw` 能傳檔案並逐 byte 還原。
   這時去做規格的「跨機器連線」：找隊友的電腦互連一次，把防火牆、Wi-Fi 的問題先解決掉。
2. **TODO 3** → 收到非法 UTF-8 會被擋下來。用 `make test` 的 17 個案例對答案。
3. **TODO 4、5** → 先只跑 `make test`，讓 Huffman 的離線 round-trip 全過，再試 `--huff` 傳輸。
   不要一邊開著 socket 一邊除錯 Huffman。建議先做 `SYM_BYTE`（最單純），通了再加 `SYM_CHAR`（切字元）與 `SYM_S16`（解析 WAV）：
   三者只差「怎麼切符號、怎麼把符號寫回 bytes」，中間的統計、建樹、codebook、位元打包是同一套。

## 試跑

同一台電腦開兩個終端機（兩台電腦的話，把 `127.0.0.1` 換成監聽端的 IP）：

| 終端機 | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 1（監聽端） | `./textlink chat server 5000` | `.\textlink.exe chat server 5000` |
| 2（連線端） | `./textlink chat client 127.0.0.1 5000` | `.\textlink.exe chat client 127.0.0.1 5000` |

聊天指令：`/raw` 不壓縮、`/huff` Huffman 壓縮、`/help`、`/quit`。每顆泡泡下方的灰字是
`模式  原始 bytes -> 上線 bytes (百分比)`，功能 1 報告要的數字可以直接從這裡抄。

**在聊天畫面裡傳檔案**：先產生幾個試玩用的檔案（中文、英文文字檔與一個 WAV，各 1 MB 多）：

| macOS / Linux / WSL | Windows PowerShell |
|---|---|
| `python3 tests/make_samples.py` | `python tests\make_samples.py` |

然後在聊天中輸入 `/files samples` 會列出檔案與編號，`/send 1` 就用目前的模式傳給對方；也可以 `/send 路徑`。
對方的畫面會自動接收並存到 `received/`，兩邊都會顯示原始 bytes、上線 bytes、壓縮率與編、解碼時間。
先 `/huff` 傳一次、再 `/raw` 傳一次同一個檔案，比較兩次的數字；文字檔和 WAV 各做一次，差別很明顯。
（`make_samples.py` 產生的 WAV 是合成音，報告請另外找真的語音或音樂。）

用命令列傳檔案（自動測試與量測用，會印 `STATS`）：

| 終端機 | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 1（接收端） | `./textlink recv 5000 out` | `.\textlink.exe recv 5000 out` |
| 2（傳送端） | `./textlink send 127.0.0.1 5000 big.txt --huff` | `.\textlink.exe send 127.0.0.1 5000 big.txt --huff` |
| 比對 | `cmp big.txt out/big.txt && echo same` | `fc.exe /b big.txt out\big.txt` |

兩端結束前各在 stderr 印一行 `STATS …`，含 `wire_bytes`、`ratio`（壓縮率）、`encode_ms`／`decode_ms`、`total_ms`。
只想留下這一行：macOS／Linux `./textlink send … 2>stats.txt`；PowerShell `.\textlink.exe send … 2>stats.txt`。

## 不知道「做完長什麼樣子」？

[../python_ref/textlink.py](../python_ref/textlink.py) 是純 Python 的完整版，格式與這個殼相同，可以互連。
先完成 TODO 1、2，然後用 `python3 ../python_ref/textlink.py recv 5000 out` 當接收端、你們的 `./textlink send 127.0.0.1 5000 檔案 --raw` 當傳送端，
檔案逐 byte 相同就代表 frame 層對了；兩邊都加上觀察（Python 加 `--probe` 會印出每個 frame 的 hex）最容易找到錯在哪一個 byte。
寫 Huffman 之前，先用 `python3 ../python_ref/textlink.py --probe inspect 檔案` 看每一步的中間結果。

## 程式地圖

```
include/platform.h   Windows／POSIX 差異（socket、執行緒）；每個 .c 的第一個 #include
include/textlink.h   規格常數、frame type、錯誤碼、所有模組的函式宣告
src/main.c           解析命令列（IP、port、--bind、--raw／--huff）→ 分派
src/net.c            監聽／連線（逾時 10 秒、印出對方 IP:port）、send_all／recv_all、單調時鐘
src/frame.c        ★ TODO 1、2；frame_send／frame_recv 已寫好
src/utf8.c         ★ TODO 3
src/huffman.c      ★ TODO 4、5
src/chat.c           聊天畫面（泡泡、bytes 統計）、接收執行緒、鍵盤輸入、/files 與 /send 選檔傳送
src/transfer.c       FILE_BEGIN／DATA／END 流程（聊天與命令列共用）、進度條、.part 暫存檔、STATS
tests/test_codec.c   離線單元測試
tests/make_samples.py 產生試玩用的文字檔與 WAV（放在 samples/，不進版控）
docs/interface.md    介面文件的範本：殼已經定好的格式先幫你們填了，Huffman 的部分要你們寫
```

殼裡已經做好、你們應該讀懂（口試會問）的幾件事：

- `recv_all` 為什麼要迴圈、`frame_recv` 怎麼靠它同時解決半包與黏包（[src/net.c](src/net.c)、[src/frame.c](src/frame.c)）。
- 為什麼對方宣稱的大小都要「先檢查、再配置」（`frame_recv`、`transfer_recv`）。
- 為什麼收到的檔名要消毒、為什麼先寫 `.part` 再改名（[src/transfer.c](src/transfer.c)）。
- 為什麼顯示前要把控制字元濾掉（[src/chat.c](src/chat.c) 的 `add_message`）。
- 為什麼連線後要設 `TCP_NODELAY`、為什麼計時要用單調時鐘（[src/net.c](src/net.c)）。
- 為什麼 `frame_send` 要上鎖：聊天時兩條執行緒都會送 frame（[src/frame.c](src/frame.c)、[src/net.c](src/net.c)）。

## 殼沒有做、留給你們決定的事

- 殼用副檔名決定符號：`.txt` → 字元、`.wav` → sample、其他 → byte；`huff_encode` 回報資料不適用時自動退回 byte（[src/transfer.c](src/transfer.c)）。
- 聊天短訊息的 Huffman 策略：每則附 codebook、兩端內建固定 codebook、還是壓完變大就改送 `TEXT_RAW`
  （[src/chat.c](src/chat.c) 的 `send_text` 裡有註解標出位置）。
- `FILE_END` 目前不帶 checksum；要不要加 CRC-32 之類的完整性檢查。
- 檔名只保留 ASCII 英數字，其他字元換成 `_`（中文檔名在 Windows 上要用寬字元 API 開檔）。
- 整個檔案一次讀進記憶體、一次編碼（上限 64 MiB）；要不要改成分塊編碼。
- 泡泡寬度用 bytes 估算，中文與 emoji 會對不齊（baseline 留下來的進階題）。
- 規格要求的 `tests/` 壞輸入測試、量測腳本、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`。

殼的任何部分都可以改，只要命令列介面、frame 外框與 `STATS` 欄位仍符合[規格](../README.md)。
