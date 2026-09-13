# examples/

| 動作 | macOS / Linux | Windows PowerShell（MSYS2 gcc） |
|---|---|---|
| 編譯 | `make` | `mingw32-make`（或在 MSYS2 終端機打 `make`） |
| 跑 utf8_dump | `make demo` | `mingw32-make demo` |
| C 版 vs Python 版比對 | `make check` | `mingw32-make check` |
| Python 版（免編譯） | `python3 utf8_dump.py ../data/sample_zh_en.txt` | `python utf8_dump.py ..\data\sample_zh_en.txt` |
| 手動編譯 | `gcc -Wall -Wextra -std=c99 utf8_dump.c -o utf8_dump` | `gcc -Wall -Wextra -std=c99 utf8_dump.c -o utf8_dump.exe` |
| 手動編譯（網路） | `gcc -Wall -Wextra -std=c99 sticky_send.c -o sticky_send` | `gcc -Wall -Wextra -std=c99 sticky_send.c -o sticky_send.exe -lws2_32` |
| 清除 | `make clean` | `mingw32-make clean` |

- `utf8_dump.c`：讀 stdin，逐字元印出 bytes、code point 與長度。
- `utf8_dump.py`：同功能 Python 版，輸出逐 byte 相同；`make check` 會 diff 兩者。
- `sticky_send.c`：連到 chat.c 的 TCP server，連續 send 多則訊息以重現黏包。
- `sticky_send.py`：同功能 Python 版，參數相同，免編譯。
  先在另一個終端機開 chat server，再執行 sticky_send，見上層 README 第三節。
- Windows 終端機若中文變亂碼，先打 `chcp 65001`。
