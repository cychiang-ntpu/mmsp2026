# 跨平台 Makefile
#   macOS / Linux           : make / make trace / make check / make clean
#   Windows (PowerShell/cmd): mingw32-make / mingw32-make trace / mingw32-make check / mingw32-make clean
#   make trace : 跑 huffman_trace（投影片的 8 色例子）
#   make check : entropy 的 C 版與 Python 版輸出逐 byte 比對（沒有差異就是一致）
#
# 這個檔案在做什麼（對應講義開頭「先把本週的範例編譯起來」、1.6 的 make trace、1.7 的 make check）：
#   make 讀這個檔案，依照「規則」決定要執行哪些指令。一條規則長這樣：
#       目標: 它依賴的檔案
#       <Tab>要執行的指令（叫 recipe；開頭一定要是 Tab，不能用空白代替）
#   make 會比較檔案的修改時間：目標不存在、或依賴的檔案比目標新，才執行 recipe；
#   所以改了 .c 再打 make 會重新編譯，沒改就什麼都不做。
#   只打 make（不給目標）時，做的是檔案裡的「第一條」規則，也就是下面的 all。
#
# ── 變數：把會重複、或各平台不同的東西集中在上面，下面的規則用 $(名稱) 取值 ──
# CC 是編譯器；CFLAGS 是給編譯器的選項：
#   -Wall -Wextra  打開大部分的警告（警告常常就是 bug，請養成「零警告」的習慣）
#   -std=c99       用 C99 標準（才能在 for 裡面宣告變數等）
# 注意：變數那一行的後面不要加註解或多餘的空白——「=」後面到行尾的東西（含結尾的空白）都會算進變數的值。
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

# ── 依作業系統設定不同的值，同一份 Makefile 才能在三種環境下用 ──
# ifeq (A,B) … else … endif 是 make 的條件式，在「讀檔的時候」就決定用哪一段（不是 recipe 裡的 shell 指令）。
# Windows 一定會有環境變數 OS=Windows_NT，macOS／Linux 沒有，用它來分辨。各變數的意思：
#   EXE   執行檔的副檔名：Windows 是 .exe，其他系統沒有
#   LIBS  連結時要加的函式庫：macOS／Linux 用到 log2() 等數學函式要加 -lm；
#         Windows 不需要 -lm，但 huffman_trace.c 用 CommandLineToArgvW 取得 UTF-8 的命令列參數，要加 -lshell32
#   PY    Python 的指令名稱（Windows 通常叫 python，其他系統叫 python3）
#   RM_F  刪除檔案的指令；RUN 是執行「目前資料夾裡的程式」要加的前綴；DIFF 是比較兩個檔案的指令
# Windows 還要再分兩種：在 Git Bash／MSYS2 裡（會有環境變數 MSYSTEM）recipe 由 bash 執行，指令和 Linux 一樣；
# 在 PowerShell／cmd 裡用 mingw32-make 時，recipe 由 cmd 執行，所以要換成 del、.\、fc。
#   RUN = .\$(EMPTY) 的小技巧：一行的最後一個字元如果是反斜線，make 會當成「下一行接續」；
#   在後面接一個沒有定義過（所以是空字串）的變數 $(EMPTY)，反斜線就不在行尾了，而展開之後的值仍然是「.\」。
#   （連註解也一樣：註解行如果以反斜線結尾，下一行會被併進註解裡，所以這幾行註解都刻意不讓反斜線落在行尾。）
ifeq ($(OS),Windows_NT)
    EXE  = .exe
    LIBS = -lshell32
    PY   = python
    ifdef MSYSTEM                 # 在 MSYS2 / Git Bash 裡：有 rm、執行要加 ./
        RM_F = rm -f
        RUN  = ./
        DIFF = diff
    else                          # mingw32-make 在 PowerShell / cmd 裡：recipe 由 cmd 執行
        RM_F = del /q
        RUN  = .\$(EMPTY)
        DIFF = fc
    endif
else
    EXE  =
    LIBS = -lm
    PY   = python3
    RM_F = rm -f
    RUN  = ./
    DIFF = diff
endif

# make check 用來比對的測試檔：第 2 週的中英文混合文字檔（講義 1.7 的例子）。
TEXT = ../../wk02_0914_text-utf8/data/sample_zh_en.txt

# ── 規則 ──
# all：預設目標。它自己沒有 recipe，只是「依賴」兩支程式，make 為了完成 all 就會去把兩支都編譯出來。
all: huffman_trace$(EXE) entropy$(EXE)

# 編譯 huffman_trace。依賴 huffman_trace.c：.c 比執行檔新的時候才重新編譯。
#   $@ 是 make 的自動變數，代表「這條規則的目標名稱」（這裡是 huffman_trace 或 huffman_trace.exe），-o $@ 指定輸出檔名。
#   $(LIBS) 要放在 .c 的後面：連結器由左到右處理，函式庫要出現在用到它的程式之後。
huffman_trace$(EXE): huffman_trace.c
	$(CC) $(CFLAGS) huffman_trace.c -o $@ $(LIBS)

# 編譯 entropy，寫法同上。
entropy$(EXE): entropy.c
	$(CC) $(CFLAGS) entropy.c -o $@ $(LIBS)

# make trace：先確定 huffman_trace 已經編譯好（依賴），再用講義 1.3／1.6 的 8 色影像頻率表執行它。
# 執行目前資料夾裡的程式要寫 ./程式名（cmd 是 .\程式名），這就是 $(RUN) 的用途。
trace: huffman_trace$(EXE)
	$(RUN)huffman_trace$(EXE) --freq black:100,white:100,yellow:20,blue:20,orange:5,red:5,purple:3,green:3

# make check：「C 實作、Python 對答案」（講義 1.7）。對同一個檔案各跑一次 C 版與 Python 版的 entropy，
#   「> 檔名」把螢幕輸出改存到檔案；最後用 diff（cmd 裡是 fc）逐 byte 比較兩個檔案。
#   兩個檔案相同時 diff 什麼都不印、結束碼是 0，make 安靜地結束，這就代表通過；
#   有任何不同，diff 會印出差異並回傳非 0，make 顯示 Error 並停下來。
check: entropy$(EXE)
	$(RUN)entropy$(EXE) $(TEXT) > c_entropy.txt
	$(PY) entropy.py $(TEXT) > py_entropy.txt
	$(DIFF) c_entropy.txt py_entropy.txt

# make clean：刪掉所有「可以重新產生」的檔案（執行檔、check 的兩個輸出檔、Python 示範寫出的 WAV）。
#   recipe 開頭的減號「-」表示：這個指令就算失敗（例如檔案本來就不存在）也不要當成錯誤。
#   有 .exe 與沒有 .exe 的檔名兩種都列出來，這樣不管在哪個平台編譯的都刪得到。
clean:
	-$(RM_F) huffman_trace huffman_trace.exe entropy entropy.exe c_entropy.txt py_entropy.txt out_16bit.wav out_8bit.wav out_4bit.wav tone_3000.wav tone_5000_aliased.wav

# .PHONY 告訴 make：這幾個目標只是「動作的名字」，不是真的要產生的檔案。
#   少了這一行的話，資料夾裡萬一剛好有一個叫 clean 或 check 的檔案，make 會以為「已經做好了」而什麼都不做。
.PHONY: all trace check clean
