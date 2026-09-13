# GitHub Actions 自動測試教學：讓 MP1–MP5 每次 push 都自動對答案

本課程五份個人作業（MP1–MP5）的驗收標準只有一條：
**你的 C 程式輸出，要和 [samples_2025-python/](../../samples_2025-python/) 的 Python 參考實作一樣。**
這份文件教你把這個「對答案」交給 GitHub 自動做：每次 `git push`，GitHub 會幫你編譯、跑測資、
和 Python 版比對，然後在你的 repo 顯示 ✅ 或 ❌。老師與助教看的也是同一個結果。

> 需要先會：[git_intro.md](git_intro.md)（commit、push）與一個 GitHub 帳號。
> 沒有 GitHub 帳號的同學先到 <https://github.com/signup> 註冊，用學校信箱可申請學生方案。
>
> **第一次接觸 GitHub Actions？先看助教梁博森寫的最小範例**：
> <https://github.com/Bensonlllll/build_on_github_test>
> 只有一個 `main.c` 和一個 workflow 檔，逐步解釋 checkout、gcc 編譯、執行、上傳 artifact 四個步驟，
> 10 分鐘看完就知道 Actions 在做什麼。本文的 workflow 是同一套概念，只是多了「和 Python 參考實作比對」。

---

## 0. 整體流程（先看懂這張圖）

```
你的電腦                          GitHub（雲端 Ubuntu 機器）
────────────────                  ──────────────────────────────────────────
寫 mp1/mp1.c                      1. 取得你的 repo
   │                              2. 取得課程 repo（測試腳本、測資、Python 參考）
git push ──────────────────────►  3. gcc 編譯你的 mp1
                                  4. 跑 mp1 與 Python 版，diff 輸出
   ◄──────────────────────────── 5. 結果顯示在 Actions 頁面：✅ 全對／❌ 哪個測資差在哪
```

重點：**測試腳本不在你的 repo 裡，在課程 repo。** 你只要放一個小小的設定檔告訴 GitHub「去跑課程的腳本」。
老師更新測資或修腳本，你什麼都不用改。

---

## 1. 個人 repo 的固定結構（template 已經幫你建好）

你的個人 repo 是從課程的 **作業 template** 建出來的（第 3 節教你怎麼建），一開始長這樣：

```
mmsp2026-hw-你的學號/
├── .github/workflows/mp-ci.yml   ← 已放好，不用動
├── mp1/README.md                 ← 把 mp1.c 放進這個資料夾
├── mp2/README.md
├── mp3/README.md
├── mp4/README.md
├── mp5/README.md
├── README.md                     ← 填姓名學號、把徽章的 OWNER/REPO 換成你的
├── .gitignore                    ← 已排除執行檔與 .ci_out/
└── .gitattributes                ← 強制 LF 換行，Windows 同學不會踩 CRLF
```

三條規則，違反任何一條 CI 就會紅：

1. **資料夾名稱** 固定 `mp1` … `mp5`，小寫。資料夾裡還沒有 `.c` 檔的作業腳本會自動略過，不算失敗。
2. **執行檔名稱** 與資料夾同名：`mp1/` 裡編出來的要叫 `mp1`。
   沒有 Makefile 時腳本會自動 `gcc -Wall -Wextra -std=c99 -o mp1 *.c -lm`；
   有 Makefile 時腳本會 `make mp1`，你的 Makefile 要產生名為 `mp1` 的執行檔。
3. **命令列參數與 Python 參考程式完全相同**（用檔名參數，不是 stdin／stdout）：

| 作業 | 你的程式被這樣呼叫 | 對應的 Python |
|---|---|---|
| MP1 | `./mp1 input.txt output.csv` | `symbol_stats.py input.txt output.csv` |
| MP2 | `./mp2 8000 16 1 sine 440 0.8 0.5 out.wav`，並在螢幕印 `SQNR = x dB` | `wavegen.py`（參數相同） |
| MP3 | `./mp3 encode in.txt codebook.csv out.bin`／`./mp3 decode out.bin codebook.csv out.txt` | `flc_codec.py` |
| MP4 | `./mp4 encode …`／`./mp4 decode …`（同 MP3） | `huffman_codec.py` |
| MP5 | `./mp5 gen 8000 out.wav`／`./mp5 spec 32 hamming 32 10 in.wav out.txt` | `spectrogram.py` |

> 去年的 MP1 樣本是 `< input > output` 從 stdin 讀，**今年改成檔名參數**，方便自動測試。
> 只差開頭幾行：用 `fopen(argv[1], "rb")` 取代 `stdin`。
> 另一個與去年不同：**輸入檔開頭若有 BOM（`EF BB BF`）要跳過、不計入**，MP1／MP3／MP4 都適用，見第 2 週講義 1.5。

---

## 2. 先在自己電腦跑一次（和 GitHub 完全相同的測試）

CI 不是黑盒子，同一支腳本你在本機就能跑。假設課程 repo 與個人 repo 並排：

```
~/mmsp2026/        ← 課程 repo（git clone https://github.com/cychiang-ntpu/mmsp2026）
~/mmsp2026-hw/     ← 你的個人 repo
```

| macOS / Linux / WSL | Windows PowerShell（MSYS2） |
|---|---|
| `cd ~/mmsp2026-hw` | `cd ~\mmsp2026-hw` |
| `bash ../mmsp2026/tools/ci/run_tests.sh mp1` | `C:\msys64\usr\bin\bash.exe ../mmsp2026/tools/ci/run_tests.sh mp1` |

MP2、MP5 需要 numpy：`pip install numpy`（Windows 用 `pip`，macOS 用 `pip3`）。

看到這樣就是全對：

```
=== MP1 symbol statistics ===
  ✅ mp1 建置成功
  ✅ mp1_basic
  ✅ mp1_skew
  ✅ text_mixed
  ✅ text_skew

================ 結果：通過 5、失敗 0、略過 0 ================
```

有 ❌ 時腳本會印出前幾行 diff，完整的輸出與參考檔都在 `.ci_out/`（已在 .gitignore，不會被 commit）：

```
  ❌ mp1_basic：CSV 與參考不同
     1c1
     < " ",10
     ---
     > " ",10,0.096153846153846
```

上面這個例子一眼就看出：少印了機率欄。**本機綠了再 push**，不要拿 GitHub 當編譯器。

> Windows 同學：腳本用 bash 寫，PowerShell 不能直接跑，所以上表用 MSYS2 的 bash。
> 或者直接在 MSYS2 終端機（開始選單的「MSYS2 UCRT64」）裡 `cd /c/Users/你/mmsp2026-hw` 再執行。

---

## 3. 建立個人 repo（只做一次，兩種方式擇一）

> **不要 fork 課程 repo。** 公開 repo 的 fork 一定是公開的、不能改成私有，你的作業會被全班看到。

### 方式 A：GitHub Classroom（課程公告有連結時用這個）

1. 點老師在 Slack 公告的 **Classroom 作業連結**，用 GitHub 帳號登入。
2. 在名單中選自己的學號（第一次會要你認領）。
3. 按 **Accept this assignment**，等幾秒，畫面會給你一個新 repo 的網址，
   例如 `https://github.com/ntpu-ce-mmsp-2026/mmsp2026-hw-你的帳號`。
   這個 repo 已經是 private，老師與助教自動有權限，不用再加 collaborator。
4. clone 到電腦：

   ```
   git clone https://github.com/ntpu-ce-mmsp-2026/mmsp2026-hw-你的帳號.git
   ```

### 方式 B：自己從 template 建（沒有 Classroom 連結時）

1. 打開課程公告的 template repo 網址（形如 `github.com/ntpu-ce-mmsp-2026/mmsp2026-hw-template`）。
2. 按右上角綠色的 **Use this template → Create a new repository**。
3. Repository name 填 `mmsp2026-hw-你的學號`，Visibility 選 **Private**，按 Create。
4. 到新 repo 的 Settings → Collaborators → Add people，加入老師與助教的 GitHub 帳號（Slack 公告）。
5. clone 到電腦。

### 兩種方式接下來都一樣

1. 打開 `README.md`，填姓名學號，把徽章那行的 `OWNER/REPO` 換成你的帳號與 repo 名。
2. 把 `mp1.c` 放進 `mp1/`，commit、push：

   ```
   git add -A
   git commit -m "MP1: first version"
   git push
   ```

3. 到 GitHub 上你的 repo，點上方 **Actions** 分頁。會看到一個正在跑的工作（黃色圈圈），
   約 1 分鐘後變成 ✅ 綠勾或 ❌ 紅叉。
   （Classroom 建的 repo 第一次可能要到 Actions 分頁按一下 **I understand my workflows, go ahead and enable them**。）

---

## 4. 看懂 Actions 頁面（紅了怎麼辦）

點進那次執行 → 點左邊的 **build-and-test** → 每個步驟可以展開。你只需要看兩個：

- **建置並與 Python 參考實作比對**：就是第 2 節你在本機看到的同一份輸出。找 ❌ 那行。
- **上傳輸出檔**：頁面最下方的 **Artifacts** 區有 `ci-outputs`，下載解壓就是 `.ci_out/`，
  裡面有你的輸出與參考輸出，可以用 VSCode 開兩個檔案比對。

常見的紅：

| 訊息 | 原因 |
|---|---|
| `mp1 建置失敗或沒有產生執行檔` | 編譯錯誤，或 Makefile 產生的檔名不是 `mp1` |
| `程式執行失敗（exit≠0）` | 程式當掉（segfault）或 `return 1`；先在本機跑同一組測資 |
| `CSV 與參考不同` | 演算法或格式錯，看 diff |
| `WAV 內容不同（… differ: byte 45）` | 檔頭或樣本值不同，用 hex 工具看第 45 byte |
| `數值個數不同` | MP5 列數或欄數錯，通常是 frame 數或 DFT 長度算錯 |

修好 → 本機再跑一次 → push → 自動再測。**每次 push 都是一次交作業前的預檢。**

---

## 5. 在 README 放狀態徽章（老師一眼看到你的狀態）

template 的 `README.md` 已經有這一行，只要把 `OWNER/REPO` 換成你的帳號與 repo 名：

```
![mp-ci](https://github.com/OWNER/REPO/actions/workflows/mp-ci.yml/badge.svg)
```

會顯示 ![passing](https://img.shields.io/badge/mp--ci-passing-brightgreen) 或 ![failing](https://img.shields.io/badge/mp--ci-failing-red)。

---

## 6. 繳交

截止日 **2026/10/23（五）18:00** 前，到課程指定的表單登錄：

1. 個人 repo 的 URL
2. 要被評分的 **完整 commit SHA**（40 碼；在 GitHub 上點 commits 頁面右邊的複製圖示）

助教會用**同一支腳本、不同測資**在那個 SHA 上評分，所以：

- 公開測資全綠不代表滿分，但公開測資紅的，私有測資一定也紅。
- 評分規則見 [course_plan.md](../course_plan.md)：repo 可存取 1 分、指定 SHA 能建置 1 分、測試群組 4 分。
- 不要在截止後 force push 改歷史，SHA 對不上視同未交。

---

## 延伸閱讀

- 助教梁博森的最小範例 <https://github.com/Bensonlllll/build_on_github_test>：想自己改 workflow（例如加 `-fsanitize=address`、多一個測資）時，先在這個範例上練。
- GitHub 官方 Actions 快速入門：<https://docs.github.com/actions/quickstart>
- 去年 MP4 滿分同學自己寫的 workflow：[samples_2025-C/mini_project_4/HIGH/mini_prj_4_100/.github/workflows/](../../samples_2025-C/mini_project_4/HIGH/mini_prj_4_100/.github/workflows/)，包含用 curl 抓大文本做壓力測試的寫法。

## 7. 常見問題

**Q1：Actions 分頁是空的？**
`.github/workflows/mp-ci.yml` 路徑或檔名打錯，或還沒 push。在 repo 頁面確認檔案在正確位置。

**Q2：顯示 `Workflow runs are disabled`？**
Private repo 有時要手動啟用：Settings → Actions → General → 選 Allow all actions。

**Q3：本機綠、GitHub 紅？**
GitHub 是 Linux + GCC，你的電腦可能是 macOS clang 或 Windows。九成是：
未初始化的變數、`\r\n` 換行、`int` 溢位、依賴未定義行為。用 `-Wall -Wextra` 看警告，
或在 WSL 裡跑一次。

**Q4：我可以改 mp-ci.yml 嗎？**
可以（例如加 `-fsanitize=address`），但評分不看你的 yml，只看課程腳本的結果。改壞了就重新複製一份。

**Q5：MP2 的 SQNR 差 0.0001 dB 過不了？**
容差是 0.001 dB。差更多通常是量化用了 `(int)` 截斷而不是四捨五入（Python 用 `rint`）。

**Q6：MP4 的 codebook 和 Python 不一樣？**
Huffman 樹平手時順序可以不同，所以 MP4 不比 codebook，只比 (1) bin 大小相同、
(2) 用 Python decoder 解你的 bin 能還原原文、(3) 用你的 decoder 解 Python 的 bin 能還原原文。

**Q7：Actions 額度會用完嗎？**
Public repo 免費無限；Private repo 每月 2,000 分鐘，一次測試約 1 分鐘，夠用。
Classroom 建立的 repo 在課程 organization 底下，額度算 organization 的，不占你的。

**Q8：我不小心 fork 了課程 repo？**
刪掉那個 fork（Settings 最下方 Delete this repository），照第 3 節重來。fork 是公開的，作業放上去等於公開。

**Q9：Classroom 名單裡沒有我的學號？**
在 Slack 私訊助教，不要隨便認領別人的。
