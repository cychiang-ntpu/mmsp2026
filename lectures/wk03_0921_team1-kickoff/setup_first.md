# 一開機就先做：把電腦教室的電腦準備好（約 5 分鐘）

電腦教室的公用電腦沒有 Git、gcc、make，Python 也不一定有；而且可能**每次重開機就還原**，所以每次上課一坐下來就先做這一頁。
裝的過程不需要系統管理員權限，也不會動到電腦原有的設定；用自己筆電、而且第 1 週已經裝好的同學可以直接跳到[第 3 步](#3-確認)。

## 1. 開 PowerShell

按鍵盤的 **Windows 鍵**，打 `powershell`，按 Enter。（VSCode 裡的終端機也可以。）

## 2. 貼上這一行，按 Enter

```powershell
irm https://raw.githubusercontent.com/cychiang-ntpu/mmsp2026/main/tools/setup/lab_setup.ps1 | iex
```

它會自動做這些事，已經有的會跳過：

| 裝什麼 | 來源 | 裝到哪裡 |
|---|---|---|
| Git | Git for Windows 官方的 PortableGit | `C:\mmsp-tools\git` |
| gcc、make | MSYS2 官方的 base，再用 `pacman` 裝 ucrt64 的 gcc 與 make | `C:\mmsp-tools\msys64` |
| Python 3、numpy | python.org 官方的 Python 套件 | `C:\mmsp-tools\python` |
| 課程 repo | `git clone`（已經有就 `git pull`） | 桌面的 `mmsp2026` 資料夾 |

最後它會把本週的 C 範例編譯並執行一次當作檢查。總共下載約 200–250 MB；全班同時下載時請耐心等，**等的時候不要關視窗**。

> `C:\` 不能寫入的電腦會自動改裝到 `C:\Users\你的帳號\mmsp-tools`。腳本的內容在 [tools/setup/lab_setup.ps1](../../tools/setup/lab_setup.ps1)，歡迎先看再跑。

## 3. 確認

看到綠色的 **`ALL SET`** 就完成了。它下面會印一行 `cd "…\examples"`，把那一行複製貼上，接著照 [commands.md](commands.md) 跟著老師打指令。

想自己再確認一次，可以打這四行，每一行都要印出版本號：

```powershell
git --version
gcc --version
mingw32-make --version
python --version
```

之後**新開**的 PowerShell 視窗與 VSCode 也都找得到這些工具（裝之前就開著的視窗要關掉重開）。

## 出問題的時候

| 看到什麼 | 怎麼辦 |
|---|---|
| 紅字 `download failed` 或 `pacman could not install gcc` | 網路太擠或斷線：**同一行指令再貼一次**，已經裝好的部分會跳過，只補沒完成的 |
| 紅字 `… check(s) failed` | 同一行指令再跑一次；還是不行就舉手，把視窗留著給老師或助教看 |
| `irm : 無法連線…` | 電腦還沒連上網路：先開瀏覽器確認能上網（有些教室要先登入） |
| 防毒軟體或 SmartScreen 跳出來擋 | 選「仍要執行」／「允許」；這些都是官方網站下載的檔案 |
| 自己的筆電想照原本的方式裝 | 見 [VSCode C 語言開發環境設定教學](../../docs/tutorials/vscode_c_starter.md)（MSYS2 安裝版）；macOS 見 [macos_c_starter.md](../../docs/tutorials/macos_c_starter.md) |

想移除：把 `C:\mmsp-tools` 整個資料夾刪掉即可。
