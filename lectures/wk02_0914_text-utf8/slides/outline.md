# 第 2 週投影片大綱（2026/9/14）

> 尚未製作正式投影片；本檔為每張投影片的標題與要點，可直接用 Marp 或 HackMD 轉出。

## 第一節

1. **本學期地圖**：三條線（文字／聲音／影像）× 四步（表示／壓縮／傳輸／整合）→ MiniLINE
2. **多媒體 = 訊號與系統**：輸入裝置 → 數位資料 → 系統 → 輸出裝置（2023 Ch.1 圖）
3. **資料就是 bytes**：硬碟上沒有「文字」，只有 0x00–0xFF
4. **時間軸一張表**：1963 ASCII → 1980s code page → 1984 Big5 → 1991 Unicode → 1992 UTF-8，每代解決上代的問題
4a. **ASCII**：7 bits、128 符號、`'A'`=0x41；第 8 bit 留給 parity，30 年後救了 UTF-8
5. **Big5**：2 bytes、13,053 字；許功蓋問題（第二 byte = `\`）、缺字造字
5a. **Unicode**：1991，全世界一張表，Han unification；原以為 16 bits 夠，1996 擴到 17 個平面
6. **UTF-8**：1992 Thompson & Pike 在餐廳墊紙上設計；編號（U+591A）與存法（E5 A4 9A）是兩件事
7. **UTF-8 一張表**：0xxx／110x／1110／11110 與 10xx
8. **為什麼能從中間找回字元邊界**：續位元組永遠 10xxxxxx
8x. **RFC 3629 三條禁令**：overlong（C0 80）、代理區 D800–DFFF、超過 10FFFF；overlong 的 `/` 是經典漏洞
8a. **BOM 是什麼**：U+FEFF → `EF BB BF`；UTF-16 的遺物，Unicode 說 UTF-8「不要求也不建議」
8b. **哪裡會碰到**：記事本、Excel CSV UTF-8（一定有）、PowerShell 5、開放資料平台下載、部分 HTTP JSON
8c. **會咬人的地方**：CSV 第一欄名壞掉、JSON.parse 失敗、gcc stray '\357'、diff 看起來一樣卻不同
8d. **怎麼處理**：讀入偵測前 3 bytes 跳過、寫出不加；Python utf-8-sig；本課程規則：跳過不計
9. **demo**：`hexdump -C`／`Format-Hex`、bytes 數 vs 字元數、`utf8_dump`
9a. **工具箱**：hexdump／xxd／od（Unix）、Format-Hex（PowerShell）、Python 一行、VSCode Hex Editor（三平台）、HxD／Hex Fiend；怎麼讀 offset／hex／ASCII 三欄
10. **與 Team 1 的關係**：「多媒體」= 9 bytes，收到 7 bytes 會怎樣

## 第二節

11. **MP1 題意**：stdin → stdout、CSV 三欄、機率 15 位小數
12. **排序三條件**：次數↓、byte 長度↑、byte 值↑
13. **CSV 轉義**：`"\n"` `"\r"` `"\t"` `""""`
14. **100 分程式的四個區塊**：結構、utf8_len、讀符號迴圈、qsort+輸出
15. **`ungetc`**：讀錯了放回去
16. **猜分數 1**：0 分——`fgetwc` 與 locale
17. **猜分數 2**：75 分——機率分母、漏 `\n` `\r`、雙引號
18. **猜分數 3**：90 分——`unsigned char < 65536` 永遠為真
19. **-Wall 的警告要看**：警告 = 你以為的檢查沒發生
20. **diff 就是 CI**：沒輸出就是全對；對答案工具也會有 bug（Python `\r`）

## 第三節

21. **兩人互連**：server 先開，client 後連，查 IP
22. **五個 socket 函式**：socket／bind+listen+accept／connect／send／recv
23. **一句話**：recv 的 bytes 數 ≠ send 的 bytes 數
24. **黏包實驗**：間隔 0 ms 泡泡數少於 5（一顆或兩顆，每次不同），間隔 500 ms 五顆
25. **半包**：長訊息被拆成兩次 recv
26. **解法：長度前綴**：4 bytes 長度 + `recv_all`（作業 3）
27. **UDP 為什麼沒這問題**：保留封包邊界
28. **收尾**：10/23 MP 截止、9/21 分組與 Team 1 開題、回家作業 1 與 3

## 附錄：2026 產業與會議（更新自 2023 Ch.1；課前請確認日期與地點）

- 會議：IEEE ICASSP 2026、IEEE ICME 2026、IEEE MMSP 2026、ACM Multimedia 2026、Interspeech 2026
- 台灣產業：聯發科（音訊／影像 codec IC）、瑞昱（音訊 codec、網通）、
  Skymizer 等 AI 編譯器、KKBOX／LINE Taiwan（串流與 IM）
- 開源專案：FFmpeg、Opus、libjpeg-turbo、WebRTC
