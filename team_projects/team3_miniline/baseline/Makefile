# Team 3 MiniLINE baseline 的跨平台 Makefile（寫法和 Team 1／Team 2 一樣；逐行解說見 docs/tutorials/makefile_intro.md）
#   macOS / Linux            : make / make smoke / make clean
#   Windows (Git Bash)       : mingw32-make / mingw32-make smoke / mingw32-make clean
#   Windows (PowerShell/cmd) : mingw32-make / mingw32-make clean（smoke 請在 Git Bash 跑：它要把 recv 放到背景）
#
#   make        產生 make_sequence、miniline_raw、chat（Windows 多 .exe）
#   make smoke  make_sequence 產生 20 張 → 在 127.0.0.1 上 raw send → recv → smoke_check.py 數「到了幾張、幾張逐 byte 相同」
#
# CFLAGS：-Wall -Wextra 幾乎所有警告都開（規格要求零警告）、-std=c99、-O2 最佳化。
# 例外只有一個：wrapper/jpeg.c 裡的 stb_image.h 有一個「參數沒用到」的警告（stbi__is_16_main 的 s），
# 那是第三方的程式碼，我們不改它，所以 jpeg.c 單獨多加 -Wno-unused-parameter；其他檔案一律零警告。
# wrapper/audio.c 裡的 miniaudio 在這組旗標下本來就是零警告。第一次編 audio.c 要 10–20 秒（miniaudio 有 10 萬行）。
CC          = gcc
CFLAGS      = -Wall -Wextra -std=c99 -O2
JPEG_CFLAGS = $(CFLAGS) -Wno-unused-parameter

ifeq ($(OS),Windows_NT)          # Windows：Winsock、timeBeginPeriod、Media Foundation（攝影機）、strmiids（曝光）、GDI（視窗）
    EXE      = .exe
    LIBS     = -lws2_32 -lwinmm -lmfplat -lmf -lmfreadwrite -lmfuuid -lstrmiids -lole32 -lgdi32 -luser32
    PY       = python
    CAM_OBJ  = camera_win.o
    DISP_OBJ = display_win.o
    ifdef MSYSTEM                 # MSYS2 / Git Bash 終端機：有 rm、執行要加 ./
        RM_F  = rm -f
        RM_RF = rm -rf
        RUN   = ./
    else                          # PowerShell / cmd 下的 mingw32-make：recipe 由 cmd 執行
        RM_F  = del /q
        RM_RF = rmdir /s /q
        RUN   = .\$(EMPTY)
    endif
else
    EXE   =
    PY    = python3
    RM_F  = rm -f
    RM_RF = rm -rf
    RUN   = ./
    ifeq ($(shell uname -s),Darwin)   # macOS：攝影機 AVFoundation、視窗 Cocoa；.m 檔用 clang 編（不用 ARC）
        CC        = clang
        OBJCFLAGS = -Wall -Wextra -O2 -fno-objc-arc
        LIBS      = -lpthread -lm -framework AVFoundation -framework CoreMedia -framework CoreVideo \
                    -framework Foundation -framework Cocoa
        CAM_OBJ   = camera_mac.o
        DISP_OBJ  = display_mac.o
    else                              # Linux：攝影機 V4L2（kernel 標頭就有）、視窗 X11（要裝 libx11-dev）
        LIBS      = -ldl -lpthread -lm
        CAM_OBJ   = camera_linux.o
        # 找得到 X11/Xlib.h 才編 display_linux.c；找不到就用 display_none.c（display_open 回傳 -1，只寫檔不開視窗）。
        # 這樣沒有 X11 的機器（SSH、WSL、CI）一樣能 make、能跑 mock 模式與評測 1–7。
        HAVE_X11 := $(shell test -f /usr/include/X11/Xlib.h -o -f /usr/local/include/X11/Xlib.h && echo 1)
        ifeq ($(HAVE_X11),1)
            DISP_OBJ = display_linux.o
            LIBS    += -lX11
        else
            DISP_OBJ = display_none.o
            $(info 注意：找不到 X11/Xlib.h，--display window 不會有視窗。要視窗：sudo apt install libx11-dev，再 make clean && make)
        endif
    endif
endif

WRAPPER_HDR = wrapper/udp.h wrapper/clock.h wrapper/wav.h wrapper/audio.h wrapper/jpeg.h wrapper/camera.h wrapper/display.h
SMOKE_PORT  = 5304

all: make_sequence$(EXE) miniline_raw$(EXE) chat$(EXE)

# ---- wrapper 的 .o（慢的、或是有特別旗標的，各自編一次就好）----
jpeg.o: wrapper/jpeg.c wrapper/jpeg.h wrapper/stb_image.h wrapper/stb_image_write.h
	$(CC) $(JPEG_CFLAGS) -c wrapper/jpeg.c -o $@

audio.o: wrapper/audio.c wrapper/audio.h wrapper/miniaudio.h
	$(CC) $(CFLAGS) -c wrapper/audio.c -o $@

camera_win.o: wrapper/camera_win.c wrapper/camera.h wrapper/camera_util.h
	$(CC) $(CFLAGS) -c wrapper/camera_win.c -o $@

display_win.o: wrapper/display_win.c wrapper/display.h
	$(CC) $(CFLAGS) -c wrapper/display_win.c -o $@

camera_linux.o: wrapper/camera_linux.c wrapper/camera.h wrapper/camera_util.h
	$(CC) $(CFLAGS) -c wrapper/camera_linux.c -o $@

display_linux.o: wrapper/display_linux.c wrapper/display.h
	$(CC) $(CFLAGS) -c wrapper/display_linux.c -o $@

display_none.o: wrapper/display_none.c wrapper/display.h
	$(CC) $(CFLAGS) -c wrapper/display_none.c -o $@

camera_mac.o: wrapper/camera_mac.m wrapper/camera.h wrapper/camera_util.h
	$(CC) $(OBJCFLAGS) -c wrapper/camera_mac.m -o $@

display_mac.o: wrapper/display_mac.m wrapper/display.h
	$(CC) $(OBJCFLAGS) -c wrapper/display_mac.m -o $@

# ---- 程式 ----
make_sequence$(EXE): make_sequence.c jpeg.o wrapper/jpeg.h
	$(CC) $(CFLAGS) make_sequence.c jpeg.o -o $@ -lm

miniline_raw$(EXE): miniline_raw.c jpeg.o audio.o $(CAM_OBJ) $(DISP_OBJ) $(WRAPPER_HDR)
	$(CC) $(CFLAGS) miniline_raw.c jpeg.o audio.o $(CAM_OBJ) $(DISP_OBJ) -o $@ $(LIBS)

chat$(EXE): chat.c
	$(CC) $(CFLAGS) chat.c -o $@ $(LIBS)

# smoke：產生 20 張 → recv 放到背景聽 5304 → send 把 20 張各自當一個 datagram 送過去 → recv 在 2 秒沒封包後自己結束
# → smoke_check.py 數到了幾張、幾張逐 byte 相同。127.0.0.1 不經過網卡，通常 20 張全到；真的網路就不是這樣了。
# 一行裡用 ; 和 & 串起來，所以要在 sh（Git Bash／macOS／Linux）裡跑；PowerShell 下的 mingw32-make 不支援。
smoke: all
	-$(RM_RF) smoke_seq smoke_recv
	$(RUN)make_sequence$(EXE) smoke_seq --frames 20
	$(RUN)miniline_raw$(EXE) video-recv $(SMOKE_PORT) smoke_recv --idle 2000 & pid=$$!; \
	sleep 1; \
	$(RUN)miniline_raw$(EXE) video-send 127.0.0.1 $(SMOKE_PORT) smoke_seq; src=$$?; \
	wait $$pid; rrc=$$?; \
	echo "exit codes: send=$$src recv=$$rrc"; \
	test $$src -eq 0 -a $$rrc -eq 0
	PYTHONIOENCODING=utf-8 $(PY) smoke_check.py smoke_seq smoke_recv

clean:
	-$(RM_F) make_sequence make_sequence.exe miniline_raw miniline_raw.exe chat chat.exe \
	        jpeg.o audio.o camera_win.o display_win.o camera_linux.o display_linux.o display_none.o camera_mac.o display_mac.o
	-$(RM_RF) smoke_seq smoke_recv

.PHONY: all smoke clean
