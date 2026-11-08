# m2_udp：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m2_udp_send$(EXE) m2_udp_recv$(EXE)

m2_udp_send$(EXE): m2_udp_send.c $(HDRS)
	$(CC) $(CFLAGS) m2_udp_send.c -o $@ $(LIBS)

m2_udp_recv$(EXE): m2_udp_recv.c $(HDRS)
	$(CC) $(CFLAGS) m2_udp_recv.c -o $@ $(LIBS)

clean:
	-$(RM_F) m2_udp_send m2_udp_recv m2_udp_send.exe m2_udp_recv.exe

.PHONY: all clean
