# macOS C 語言開發環境設定教學（新手入門）

這份文件是 [vscode_c_starter.md](vscode_c_starter.md)（Windows 版）的 **macOS 平行版本**。
步驟編號刻意對齊，上課時老師說「看步驟 5」，你就看這裡的步驟 5。
Intel 與 Apple Silicon（M1／M2／M3／M4）的 Mac 做法完全一樣。

---

## 你會安裝的三樣東西

| 工具 | 用途 | macOS 上怎麼來 |
|------|------|------|
| **VSCode** | 寫程式用的編輯器 | 官網下載 |
| **C 編譯器** | 把 C 程式碼翻譯成執行檔 | Apple 內建的 **Command Line Tools**，一行指令就裝好，裡面的 `gcc` 指令其實是 clang |
| **C/C++ 擴充套件** | 讓 VSCode 看得懂 C | VSCode 內安裝 |

> 為什麼 Mac 的 `gcc` 是 clang？Apple 把 `gcc` 這個名字指向自家的 clang 編譯器。
> 本課程所有作業用 clang 或 GCC 都能編譯，差別只在錯誤訊息的措辭。
> 想裝真正的 GCC 可以用 Homebrew（`brew install gcc`，指令名是 `gcc-14`），但**不需要**。

---

## 步驟 1：安裝 VSCode

1. 前往 <https://code.visualstudio.com/>，點 **Download for macOS**。
   （網站會自動偵測 Intel 或 Apple Silicon；下錯了也能跑，只是慢一點。）
2. 打開下載的 zip，把 **Visual Studio Code** 拖進「應用程式」資料夾。
3. 第一次打開若出現「無法確認開發者」，到「系統設定 > 隱私權與安全性」按「強制打開」。
4. （強烈建議）在 VSCode 按 `Cmd+Shift+P`，輸入 `shell command`，
   選 **Shell Command: Install 'code' command in PATH**。之後在終端機打 `code .` 就能開目前資料夾。

> 想要中文介面：`Cmd+Shift+X` 打開擴充套件，搜尋「Chinese (Traditional)」安裝後重啟。

---

## 步驟 2：安裝 C 編譯器（Command Line Tools）

1. 按 `Cmd+空白鍵` 打開 Spotlight，輸入 `Terminal`（終端機）按 Enter。
2. 輸入以下指令後按 Enter：

   ```
   xcode-select --install
   ```

3. 會跳出視窗問你要不要安裝，按「安裝」，同意授權，等它下載（約 1–3 GB，看網速 5–20 分鐘）。
   如果顯示「command line tools are already installed」代表早就裝好了，直接到第 4 步。
4. 驗證安裝：

   ```
   gcc --version
   make --version
   python3 --version
   ```

   三個都有版本號就成功了。`gcc --version` 會印出 `Apple clang version 15.x`，這是正常的（見上方說明）。

> 不需要裝 Xcode 本體（十幾 GB），Command Line Tools 就夠了。

---

## 步驟 3：安裝 C/C++ 擴充套件

1. 打開 VSCode，按 `Cmd+Shift+X`。
2. 搜尋 `C/C++`，安裝 **Microsoft 出品的「C/C++」**。
3. （建議）順便安裝 **Code Runner**。
4. （除錯用，之後才需要）安裝 **CodeLLDB**。Mac 上的除錯器是 lldb 不是 gdb，
   [vscode_debug_tutorial.md](vscode_debug_tutorial.md) 裡的 gdb 設定在 Mac 要改用這個擴充套件，見本文 FAQ Q5。

---

## 步驟 4：寫你的第一個程式 Hello, World!

1. 建立資料夾，例如 `~/hello_c`（`~` 代表你的家目錄 `/Users/你的名字`）。
   > 資料夾與檔案名稱**避免中文和空白**。
2. VSCode 按 `File > Open Folder...`（或終端機 `cd ~/hello_c` 後打 `code .`）。
3. 新增檔案 `hello.c`，輸入並存檔（`Cmd+S`）：

   ```c
   #include <stdio.h>

   int main(void)
   {
       printf("Hello, World!\n");
       return 0;
   }
   ```

---

## 步驟 5：編譯並執行

### 方法一：使用終端機（建議先學會這個）

1. VSCode 按 `` Ctrl+` ``（Ctrl 加左上角反引號；Mac 也是 Ctrl 不是 Cmd）打開內建終端機。
2. 編譯：

   ```
   gcc hello.c -o hello
   ```

3. 執行（注意是 `./` 不是 Windows 的 `.\`）：

   ```
   ./hello
   ```

4. 看到 `Hello, World!` 🎉

### 方法二：Code Runner

打開 `hello.c` 按 `Ctrl+Option+N`。

---

## 常見問題（FAQ）

**Q1：`xcode-select: error: command line tools are already installed`？**
已經裝好了，直接跳到步驟 2 第 4 步驗證。

**Q2：`zsh: permission denied: ./hello`？**
編譯沒成功所以檔案不存在，或你打成 `.\hello`。先看上一步有沒有紅字。

**Q3：中文亂碼？**
macOS 終端機預設就是 UTF-8，極少亂碼。若 `wc -m` 算出的字元數和 bytes 數一樣，
是語系沒設：打 `export LANG=en_US.UTF-8` 再試。

**Q4：`ld: library not found for -lpthread`？**
不會發生。Mac 的 pthread 內建在系統函式庫，`-lpthread` 可加可不加，課程 Makefile 加了也沒問題。

**Q5：想用 VSCode 逐行除錯？**
Mac 沒有 gdb（裝了也要簽章，很麻煩）。裝 **CodeLLDB** 擴充套件，
編譯時加 `-g`（`gcc -g hello.c -o hello`），然後 `Run > Start Debugging`，
第一次會要你選環境，選 **LLDB**。中斷點、逐行、看變數的操作和 gdb 版教學相同。

**Q6：第一次執行網路程式跳出「要允許『chat』接受連入網路連線嗎？」**
按「允許」。這是 macOS 防火牆，不允許的話同學連不進來。

---

## 步驟 6：實戰！執行迷你 LINE 聊天程式

### 6-1 取得程式碼

程式在課程 repo 裡：`team_projects/team1_textlink/baseline/`（`chat.c` 與 `Makefile`）。
還沒 clone repo 的話先看 [git_intro.md](git_intro.md)。

```
cd ~/mmsp2026/team_projects/team1_textlink/baseline
code .
```

### 6-2 編譯

```
make
```

或手動：`gcc chat.c -o chat -lpthread`。資料夾裡會多出 `chat`（沒有 .exe 副檔名）。

### 6-3 執行：先在自己電腦上測試

開兩個終端機（VSCode 終端機右上角 `+`）：

```
終端機1:  ./chat tcp server 5000
終端機2:  ./chat tcp client 127.0.0.1 5000
```

UDP 模式：

```
終端機1:  ./chat udp 5000 127.0.0.1 6000
終端機2:  ./chat udp 6000 127.0.0.1 5000
```

打字按 Enter 互傳，`/quit` 離開。

### 6-4 和同學的電腦互連

1. 兩台電腦連**同一個 Wi-Fi**。
2. 查自己的 IP：

   ```
   ipconfig getifaddr en0
   ```

   （`en0` 通常是 Wi-Fi；沒印出東西就試 `en1`，或到「系統設定 > Wi-Fi > 詳細資訊」看 IP 位址。）
3. 把 `127.0.0.1` 換成對方的 IP。
4. 第一次會跳出防火牆詢問，按「允許」（FAQ Q6）。

連不上的檢查順序：IP 打錯 → 不同 Wi-Fi → 防火牆 → 對方是 Windows 且防火牆擋了。

---

## Mac 與 Windows 指令對照（本課程會用到的）

| 做什麼 | Windows PowerShell | macOS 終端機 |
|---|---|---|
| 執行程式 | `.\hello.exe` | `./hello` |
| 路徑分隔 | `\` | `/` |
| 編譯網路程式 | `gcc chat.c -o chat.exe -lws2_32` | `gcc chat.c -o chat -lpthread` |
| make | `mingw32-make` | `make` |
| 查 IP | `ipconfig` | `ipconfig getifaddr en0` |
| 看檔案 bytes | `Format-Hex f.txt` | `hexdump -C f.txt` |
| 輸入重導向 | `cmd /c ".\a.exe < in.txt"` | `./a < in.txt` |
| Python | `python` | `python3` |
| 清畫面 | `cls` | `clear` |

---

## 下一步

和 Windows 版相同，照 [vscode_c_starter.md](vscode_c_starter.md) 文末的順序讀其他教學。
那些教學以 Windows 為主，遇到 `.\` 就想成 `./`、遇到 `mingw32-make` 就想成 `make`，其餘相同。
