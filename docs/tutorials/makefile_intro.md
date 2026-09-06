# Makefile 入門：從打 gcc 指令升級到打 make

每次編譯聊天程式都要打這麼長的指令：

```
gcc -Wall -Wextra -std=c99 chat.c -o chat.exe -lws2_32
```

有沒有辦法只打 4 個字母就好？有——把指令寫進一個叫 **Makefile** 的檔案，
以後只要打：

```
make
```

`nettcpudp` 專案裡就附了一個 Makefile，這份文件帶你看懂它的每一行。

> **Windows 同學注意**：`make` 指令來自 MSYS2，若打 `make` 說找不到，
> 先在 MSYS2 終端機執行 `pacman -S make`，
> 或安裝 `pacman -S mingw-w64-ucrt-x86_64-make` 後用 `mingw32-make` 代替。

---

## Makefile 的基本語法：規則

Makefile 由一條條「規則」組成，長這樣：

```makefile
目標: 材料
	怎麼做（指令）
```

用白話說：「想做出**目標**，需要**材料**，做法是執行**指令**」。例如：

```makefile
chat.exe: chat.c
	gcc chat.c -o chat.exe -lws2_32
```

意思是：想做出 `chat.exe`，材料是 `chat.c`，做法是那行 gcc 指令。

> ⚠️ **天字第一號地雷**：指令那行的開頭必須是一個 **Tab**，
> 用空白會出現 `missing separator` 錯誤。在 VSCode 貼上時特別注意。

make 還有一個聰明之處：如果 `chat.c` 從上次編譯後**沒有改過**，
再打 `make` 它會說 `up to date`，直接跳過——檔案多的專案能省下大量時間。

---

## 逐行看懂 nettcpudp 的 Makefile

```makefile
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99
```

這是**變數**：`CC` 存編譯器名稱、`CFLAGS` 存編譯選項。
之後用 `$(CC)`、`$(CFLAGS)` 取出來用，要換編譯器或加選項只需改這裡。

順便認識這三個選項（建議你自己編譯時也都加上）：
- `-Wall -Wextra`：打開幾乎所有警告，幫你抓潛在 bug。
- `-std=c99`：使用 C99 標準（允許在 for 裡宣告 `int i` 等寫法）。

```makefile
ifeq ($(OS),Windows_NT)
    TARGET = chat.exe
    LIBS   = -lws2_32
else
    TARGET = chat
    LIBS   = -lpthread
endif
```

這是**條件判斷**：Windows 系統會有環境變數 `OS=Windows_NT`，
藉此決定輸出檔名（Windows 要 `.exe`）和要連結的函式庫
（Windows 網路用 `ws2_32`、Linux/macOS 執行緒用 `pthread`）。
這就是「同一份 Makefile 三個平台都能用」的祕密。

```makefile
$(TARGET): chat.c
	$(CC) $(CFLAGS) chat.c -o $(TARGET) $(LIBS)
```

主規則。把變數展開，在 Windows 上它就等於：

```
gcc -Wall -Wextra -std=c99 chat.c -o chat.exe -lws2_32
```

```makefile
clean:
	rm -f chat chat.exe
```

`clean` 是慣例上的「打掃」規則：`make clean` 會刪掉編譯產物，
讓你可以從乾淨狀態重新編譯。它沒有「材料」，純粹是給人手動呼叫的指令集。

---

## 實際操作

```
make            # 編譯（執行第一條規則）
make            # 再打一次 → 'up to date'，因為 chat.c 沒改
make clean      # 刪掉編譯產物
make            # 又會重新編譯了
```

---

## 練習：幫自己的作業寫一個 Makefile

在你的 `hello_c` 資料夾建立檔案（檔名就叫 `Makefile`，沒有副檔名），內容：

```makefile
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

hello.exe: hello.c
	$(CC) $(CFLAGS) hello.c -o hello.exe

clean:
	rm -f hello.exe
```

存檔後打 `make` 試試（記得指令行開頭是 Tab！）。

**進階練習**：加一條 `run` 規則，讓 `make run` 能編譯完直接執行：

```makefile
run: hello.exe
	./hello.exe
```

---

## 常見問題

**Q1：`missing separator. Stop.`？**
指令行開頭用了空白而不是 Tab。刪掉重打一個 Tab。

**Q2：`make: command not found`？**
還沒安裝 make，見文件開頭的 Windows 注意事項。

**Q3：明明改了程式，make 卻說 up to date？**
檔案忘了存檔（`Ctrl+S`），或你改的檔案不在規則的「材料」清單裡。

**Q4：Makefile 需要重新產生執行檔卻沒反應？**
確認檔名是 `Makefile` 或 `makefile`（M 大小寫皆可，但不能有 .txt 之類的副檔名）。
