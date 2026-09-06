# 跨平台 Makefile（逐行解說見 ../envsetup 產生的 makefile_intro.md）：在 Linux / macOS 直接 `make`；
# Windows (MinGW/MSYS2) 也可用 `make`，或手動執行下面對應的 gcc 指令。

CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

ifeq ($(OS),Windows_NT)          # Windows：連結 Winsock
    TARGET = chat.exe
    LIBS   = -lws2_32
else                              # macOS / Linux：連結 pthread
    TARGET = chat
    LIBS   = -lpthread
endif

$(TARGET): chat.c
	$(CC) $(CFLAGS) chat.c -o $(TARGET) $(LIBS)

clean:
	rm -f chat chat.exe
