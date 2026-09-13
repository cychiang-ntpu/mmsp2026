# WSL（Windows Subsystem for Linux）C 語言開發環境設定教學

這份文件是 [vscode_c_starter.md](vscode_c_starter.md)（Windows + MSYS2 版）的 **WSL 平行版本**。
步驟編號對齊，方便上課對照。

## 先決定：MSYS2 還是 WSL？

兩者都是在 Windows 上取得 gcc 的方法，本課程**兩種都接受**，但請只選一種，不要混用。

| | MSYS2（課程預設） | WSL |
|---|---|---|
| 是什麼 | 在 Windows 上的原生 GCC | Windows 裡跑一個真正的 Ubuntu Linux |
| 編出來的程式 | Windows 的 `.exe` | Linux 執行檔 |
| 網路程式 | 用 Winsock（`-lws2_32`） | 和 macOS／Linux 一樣用 `-lpthread` |
| 和同學互連 | 直接可以 | **要多一步設定**（見步驟 6-4），這是 WSL 最大的坑 |
| 適合誰 | 只想寫作業、不想學 Linux | 之後想用 Linux 工具、或已經在用 WSL 的同學 |

> 期末上機考的環境會另行公告。如果你不確定，選 MSYS2。

---

## 你會安裝的三樣東西

| 工具 | 用途 |
|------|------|
| **WSL + Ubuntu** | 在 Windows 裡的 Linux |
| **build-essential** | Ubuntu 的 gcc、make 等工具包 |
| **VSCode + WSL 擴充套件** | 讓 Windows 上的 VSCode 直接編輯、編譯 Linux 裡的檔案 |

---

## 步驟 1：安裝 WSL 與 Ubuntu

需要 Windows 10 版本 2004 以上或 Windows 11。

1. 按 `Win` 鍵，輸入 `PowerShell`，**右鍵 > 以系統管理員身分執行**。
2. 輸入：

   ```
   wsl --install
   ```

   會自動啟用功能並安裝 Ubuntu。完成後**重新開機**。
3. 重開機後 Ubuntu 視窗會自動打開，要你設定一個 Linux 使用者名稱與密碼
   （密碼打字時不會顯示，正常）。這組帳密之後 `sudo` 會用到。
4. 驗證：在 PowerShell 打 `wsl --version`，看到 WSL 版本號即可。
   打 `wsl -l -v`，Ubuntu 那列 VERSION 應該是 **2**（WSL2）。

> 已經裝過 WSL 但是 WSL1？在 PowerShell 打 `wsl --set-version Ubuntu 2`。

---

## 步驟 2：安裝 GCC（在 Ubuntu 裡）

1. 打開 Ubuntu（按 `Win` 鍵輸入 `Ubuntu`），輸入：

   ```
   sudo apt update
   sudo apt install -y build-essential gdb make git python3
   ```

   會要你輸入步驟 1 設定的密碼。
2. 驗證：

   ```
   gcc --version
   make --version
   ```

---

## 步驟 3：安裝 VSCode 與 WSL 擴充套件

1. 在 **Windows** 照 [vscode_c_starter.md](vscode_c_starter.md) 步驟 1 裝 VSCode（不是裝在 Ubuntu 裡）。
2. `Ctrl+Shift+X`，搜尋並安裝 **WSL**（Microsoft 出品，ID 是 `ms-vscode-remote.remote-wsl`）。
3. 回到 Ubuntu 終端機，打：

   ```
   mkdir -p ~/hello_c && cd ~/hello_c && code .
   ```

   第一次會自動安裝 VSCode Server，之後 VSCode 視窗左下角會出現綠色的 **WSL: Ubuntu**，
   代表你現在編輯的是 Linux 裡的檔案。
4. 在這個 WSL 模式的 VSCode 裡再裝一次 **C/C++** 擴充套件（擴充套件分 Windows 端與 WSL 端）。

> **檔案放哪裡很重要**：把專案放在 Linux 的家目錄（`~/`，也就是 `/home/你的名字/`），
> **不要**放在 `/mnt/c/...`（Windows 的 C 槽）。放 `/mnt/c` 編譯會慢 5–10 倍，git 也常出權限問題。
> 想從 Windows 檔案總管看 Linux 的檔案，網址列輸入 `\\wsl$\Ubuntu\home\你的名字`。

---

## 步驟 4：寫你的第一個程式 Hello, World!

在 VSCode（左下角顯示 WSL: Ubuntu）新增 `hello.c`：

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

VSCode 按 `` Ctrl+` `` 打開終端機，它已經是 Ubuntu 的 bash：

```
gcc hello.c -o hello
./hello
```

看到 `Hello, World!` 🎉。注意 WSL 裡一律用 `./`、`/`，和 macOS／Linux 相同，**不是** Windows 的 `.\`。

---

## 常見問題（FAQ）

**Q1：`wsl --install` 說找不到指令或失敗？**
Windows 太舊。到「設定 > Windows Update」更新，或照微軟文件手動啟用：<https://learn.microsoft.com/windows/wsl/install-manual>。

**Q2：VSCode 終端機打 `gcc` 說找不到，但 Ubuntu 視窗可以？**
你的 VSCode 不是 WSL 模式（左下角沒有綠色 WSL 標記）。關掉，回 Ubuntu 終端機用 `code .` 重開。

**Q3：`./hello` 出現 `Permission denied`？**
檔案在 `/mnt/c` 底下。搬到 `~/`（見步驟 3 的提醒）。

**Q4：中文亂碼？**
WSL 預設 UTF-8，通常不會。若有，`sudo apt install language-pack-zh-hant` 後 `export LANG=C.UTF-8`。

**Q5：Windows 的檔案 git 顯示整份都改過？**
換行字元問題（CRLF 與 LF）。在 Ubuntu 裡 `git config --global core.autocrlf input`。課程 repo 已用 `.gitattributes` 強制 LF，clone 後正常。

**Q6：想用 VSCode 逐行除錯？**
WSL 裡有 gdb，[vscode_debug_tutorial.md](vscode_debug_tutorial.md) 直接適用，只要把編譯器路徑改成 `/usr/bin/gcc`、除錯器改成 `/usr/bin/gdb`。

---

## 步驟 6：實戰！執行迷你 LINE 聊天程式

### 6-1 取得程式碼

在 Ubuntu 終端機 clone 課程 repo 到家目錄（見 [git_intro.md](git_intro.md)），然後：

```
cd ~/mmsp2026/team_projects/team1_textlink/baseline
code .
```

### 6-2 編譯

```
make
```

或 `gcc chat.c -o chat -lpthread`。WSL 是 Linux，**不用** `-lws2_32`。

### 6-3 執行：先在自己電腦上測試

```
終端機1:  ./chat tcp server 5000
終端機2:  ./chat tcp client 127.0.0.1 5000
```

UDP：

```
終端機1:  ./chat udp 5000 127.0.0.1 6000
終端機2:  ./chat udp 6000 127.0.0.1 5000
```

### 6-4 和同學的電腦互連（WSL 的坑）

WSL2 預設有**自己的虛擬網卡**，IP 長得像 `172.x.x.x`，同學從外面連不到。
在 Ubuntu 打 `hostname -I` 看到的就是這個內部 IP，**不能給同學用**。三種解法，選一種：

**解法 A：鏡像網路模式（Windows 11 22H2 以上，最省事）**

1. 在 Windows 建立檔案 `C:\Users\你的名字\.wslconfig`，內容：

   ```
   [wsl2]
   networkingMode=mirrored
   ```

2. PowerShell 打 `wsl --shutdown`，再重開 Ubuntu。
3. 之後 WSL 與 Windows 共用同一個 IP，用 Windows 的 `ipconfig` 查到的 IPv4 給同學即可。

**解法 B：port forwarding（Windows 10 或不想改設定）**

在**系統管理員 PowerShell** 打（把 5000 換成你用的 port）：

```
netsh interface portproxy add v4tov4 listenport=5000 listenaddress=0.0.0.0 connectport=5000 connectaddress=$(wsl hostname -I)
```

同學連 Windows 的 IP（`ipconfig` 看到的）即可。WSL 重開後內部 IP 會變，要重打一次。
清除：`netsh interface portproxy reset`。

**解法 C：互連時改用 MSYS2 版**

本機測試用 WSL，要和同學互連時改在 Windows 用 MSYS2 編譯 `.exe`。兩份程式碼相同。

不論哪一種，Windows 防火牆第一次都會詢問，按「允許」；沒跳出詢問但連不上時，
到「Windows 安全性 > 防火牆 > 允許應用程式」手動放行，或暫時關閉私人網路防火牆測試。

連不上的檢查順序：IP 給的是 172.x 內部 IP → 不同 Wi-Fi → 防火牆 → portproxy 沒設。

---

## WSL 與 Windows 指令對照

| 做什麼 | Windows PowerShell（MSYS2） | WSL Ubuntu |
|---|---|---|
| 執行程式 | `.\hello.exe` | `./hello` |
| 編譯網路程式 | `gcc chat.c -o chat.exe -lws2_32` | `gcc chat.c -o chat -lpthread` |
| make | `mingw32-make` | `make` |
| 查 IP | `ipconfig` | `hostname -I`（內部 IP，見 6-4） |
| 看檔案 bytes | `Format-Hex f.txt` | `hexdump -C f.txt` |
| 輸入重導向 | `cmd /c ".\a.exe < in.txt"` | `./a < in.txt` |
| Windows 檔案在哪 | `C:\Users\me\` | `/mnt/c/Users/me/`（別把專案放這） |
| Python | `python` | `python3` |

---

## 下一步

照 [vscode_c_starter.md](vscode_c_starter.md) 文末的順序讀其他教學。
WSL 裡的指令和 macOS／Linux 欄相同：`./` 執行、`make`、`hexdump -C`。
