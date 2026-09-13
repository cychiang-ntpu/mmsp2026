# 跨平台 Makefile（逐行解說見 docs/tutorials/makefile_intro.md）：在 Linux / macOS 直接 `make`；
# Windows (MinGW/MSYS2) 也可用 `make`，或手動執行下面對應的 gcc 指令。

CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

ifeq ($(OS),Windows_NT)          # Windows：連結 Winsock
    TARGET = chat.exe
    LIBS   = -lws2_32
    ifdef MSYSTEM                 # MSYS2 / Git Bash 終端機有 rm
        RM_F = rm -f
    else                          # PowerShell / cmd 下的 mingw32-make：recipe 由 cmd 執行，用 del
        RM_F = del /q
    endif
else                              # macOS / Linux：連結 pthread
    TARGET = chat
    LIBS   = -lpthread
    RM_F   = rm -f
endif

$(TARGET): chat.c
	$(CC) $(CFLAGS) chat.c -o $(TARGET) $(LIBS)

clean:
	-$(RM_F) chat chat.exe
