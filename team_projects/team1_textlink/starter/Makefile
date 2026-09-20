# TextLink starter 的跨平台 Makefile（逐行解說見 docs/tutorials/makefile_intro.md）
#   macOS / Linux            : make / make test / make clean
#   Windows (PowerShell/cmd) : mingw32-make / mingw32-make test / mingw32-make clean
#
#   make       產生 textlink（Windows 為 textlink.exe）
#   make test  編譯並執行 tests/test_codec：frame 標頭、UTF-8、Huffman 的離線測試
#
# 這個檔案告訴 make「要產生哪個檔案、它由哪些檔案做出來、用什麼指令做」。完成五個 TODO 不需要改它；
# 之後自己新增 .c 檔時，把檔名加進下面的 CODEC 或 SHELL_SRC 就會一起編譯。
#
# 名稱 = 值 是變數，用 $(名稱) 取值。CFLAGS 是交給 gcc 的選項：
#   -Wall -Wextra  打開幾乎所有警告。規格要求「無警告」：警告多半就是 bug（變數沒用到、有號無號混著比…）
#   -std=c99       使用 C99 標準（可以寫 for (int i = 0; …)、可以在區塊中間宣告變數）
#   -O2            最佳化
#   -Iinclude      #include "textlink.h" 時到 include/ 資料夾找
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Iinclude

# ifeq … else … endif 是 make 自己的 if：Windows 會設定環境變數 OS=Windows_NT，用它分辨平台。
# 三個平台的差別：執行檔的副檔名、要連結的函式庫、刪檔案的指令、執行目前資料夾裡的程式時前面要加什麼。
ifeq ($(OS),Windows_NT)          # Windows：連結 Winsock
    EXE  = .exe
    LIBS = -lws2_32
    ifdef MSYSTEM                 # MSYS2 / Git Bash 終端機：有 rm、執行要加 ./
        RM_F = rm -f
        RUN  = ./
    else                          # PowerShell / cmd 下的 mingw32-make：recipe 由 cmd 執行
        RM_F = del /q
        RUN  = .\$(EMPTY)
    endif
else                              # macOS / Linux：連結 pthread
    EXE  =
    LIBS = -lpthread
    RM_F = rm -f
    RUN  = ./
endif

HDR   = include/textlink.h include/platform.h
CODEC = src/frame.c src/utf8.c src/huffman.c          # 有 place holder 的三個檔
SHELL_SRC = src/main.c src/net.c src/chat.c src/transfer.c

# 規則的格式：
#     目標: 它依賴的檔案
#     <Tab>做出目標的指令          ← 行首一定要是 Tab 字元，不能用空白；編輯器若把 Tab 換成空白，make 會報錯
# make 會比較修改時間：依賴的檔案有任何一個比目標新，才重新執行指令。所以改了任何 .c 或 .h 再打 make，就會重新編譯。
# 指令裡的 $@ 代表「這條規則的目標」。只打 make、不給目標時，做的是檔案裡的第一個目標，也就是 all。
all: textlink$(EXE)

textlink$(EXE): $(SHELL_SRC) $(CODEC) $(HDR)
	$(CC) $(CFLAGS) $(SHELL_SRC) $(CODEC) -o $@ $(LIBS)

# 測試程式只需要 codec 三個檔與 net.c（tl_strerror 等），不開 socket
test_codec$(EXE): tests/test_codec.c src/net.c $(CODEC) $(HDR)
	$(CC) $(CFLAGS) tests/test_codec.c src/net.c $(CODEC) -o $@ $(LIBS)

# 先確定 test_codec 是最新的，再執行它。還有 TODO 或 FAIL 時 test_codec 的結束碼不是 0，
# make 會印出 Error 1：那是「測試還沒全過」的提醒，不是編譯失敗。
test: test_codec$(EXE)
	$(RUN)test_codec$(EXE)

# 指令最前面的 - 表示「這一行失敗也沒關係，繼續」（有些檔案本來就不存在）。
clean:
	-$(RM_F) textlink textlink.exe test_codec test_codec.exe

# .PHONY 宣告這三個目標不是真的檔案名稱：就算資料夾裡剛好有一個叫 test 的檔案，make test 仍然會執行。
.PHONY: all test clean
