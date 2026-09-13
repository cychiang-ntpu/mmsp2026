# 文字編碼簡史：從電報到 UTF-8（補充閱讀）

> 給大三同學的背景閱讀，約 15 分鐘。上課只講其中的重點，細節與出處在文末參考文獻。
> 目的不是背年份，而是理解**每一代編碼都在解決上一代的什麼問題**，這樣 UTF-8 的規則就不用死背。

## 0. 為什麼要「編碼」

電腦只存 bit。要存「文字」，必須先約定「哪個數字代表哪個字」，這份約定就是**字元集**（character set）；
再約定「這個數字怎麼排成 bytes」，這叫**編碼**（encoding）。早期兩者是同一件事（一個字＝一個 byte），
到 Unicode 才明確分開：**Unicode 給字編號，UTF-8 決定編號怎麼存**。本週講義 1.2 的表格就是這個分工。

## 1. 電報時代：先有「碼」才有電腦（1840s–1930s）

- **摩斯電碼**（1844）：長短音代表字母，長度不固定，常用的 E 最短。這是**變長編碼**與「常用符號用短碼」的
  祖先，第 5 週（10/5）的 Huffman 編碼就是把這個直覺做成最佳化演算法。
- **Baudot 碼**（1874）：5 bits 固定長度，32 種組合，靠「切換字母／數字模式」擴充。
  固定長度的好處是機器好做，壞處是不夠用，這個矛盾會一路延續到 Big5 與 Unicode。

## 2. ASCII：美國人的 128 個字（1963）

- ASA X3.4-1963，1967 年修訂加入小寫字母，成為今天的 ASCII（American Standard Code for Information Interchange）。
- **7 bits、128 個碼位**：0–31 是控制字元（`\n`=10、`\r`=13、`\t`=9），32–126 是可見字元，127 是 DEL。
- 為什麼是 7 bits 而不是 8？當時傳輸成本高，第 8 個 bit 留給同位檢查（parity）。
  這個「最高位元是 0」的特性，30 年後成為 UTF-8 能與 ASCII 相容的關鍵。
- IBM 同期有自己的 8-bit **EBCDIC**（1963–64，隨 System/360 推出），至今大型主機仍在用，提醒我們「標準」不只一套。

## 3. 8-bit 時代與「code page」地獄（1980s）

- 個人電腦普及後第 8 個 bit 被拿來放各國字元：128–255 這 128 個位置，每個國家、每家公司各填各的。
  西歐用 ISO 8859-1（Latin-1，1987）、IBM PC 用 CP437、Windows 用 CP1252……
- 同一個 byte 0xE9 在 Latin-1 是「é」，在希臘語頁是「ι」，在西里爾頁是「й」。
  收到檔案卻不知道它用哪個 code page，就會看到**亂碼**（日文稱 mojibake，文字化け）。
- 這就是 Joel Spolsky 那篇名文的核心結論：**「沒有編碼資訊的純文字是不存在的」**（There ain't no such thing as plain text）。

## 4. 中日韓：一個 byte 不夠用（1978–1990s）

一個 byte 最多 256 個位置，中文常用字就有五千以上，所以東亞編碼一律用**2 bytes 表示一個漢字**，
並且和 ASCII 混用：byte 值小於 128 是 ASCII，大於等於 128 的當作 2-byte 字的第一個 byte。

| 編碼 | 年 | 地區 | 備註 |
|---|---|---|---|
| JIS X 0208 / Shift_JIS | 1978 / 1982 | 日本 | 最早的雙位元組實用方案 |
| GB 2312 | 1980 | 中國大陸 | 簡體字，6,763 字；後擴為 GBK、GB 18030 |
| **Big5（大五碼）** | 1984 | 台灣 | 資策會為「五大」中文套裝軟體計畫制定，1983 年底公告、1984 年推行；常用 5,401＋次常用 7,652＝13,053 字 |
| CNS 11643 | 1986 | 台灣 | 國家標準，1986 首版只有兩個字面（源自 Big5），1992 年第二版擴為七個字面；今日收字超過 10 萬，戶政系統用 |
| KS X 1001 | 1987 | 韓國 | 韓文與漢字 |

### Big5 的規則與它的毛病

- 第一個 byte 0x81–0xFE（實際使用 0xA1–0xF9），第二個 byte 0x40–0x7E 或 0xA1–0xFE。
  去年 MP1 滿分程式 [mini_prj_1_100.c 第 16–24 行](../../samples_2025-C/mini_project_1/HIGH/mini_prj_1_100.c#L16-L24) 判斷的就是這兩個範圍。
- **第二個 byte 可能落在 ASCII 範圍**（0x40–0x7E），這是 Big5 最出名的設計缺陷。
  「功」（0xA55C）「許」（0xB35C）「蓋」（0xBB5C）等字的第二個 byte 是 0x5C，也就是反斜線 `\`，
  在 C 字串、路徑、正規表示式裡都會被當成跳脫字元，台灣工程師稱之為**「許功蓋問題」**。
  同理，從字串中間往回找不到字元邊界，因為第二個 byte 和第一個 byte 的範圍重疊；UTF-8 特別設計成沒有這個問題。
- Big5 收字不足：「堃」「喆」「煊」等姓名用字不在其中，早年戶政與銀行系統要靠「造字」補，
  各家造字碼位不同，資料交換再度亂碼。這是台灣最早體會到「需要一個全球統一字元集」的痛點。
- 同時期的**倚天中文系統**（1985 年 11 月推出）在 Big5 的 0xF9D6–0xF9DC 自行加了 7 個常用異體字
  （碁、銹、裏、墻、恒、粧、嫺），後來被微軟 CP950 吸收，成為「Big5 有好幾種」的來源之一。

## 5. Unicode：把全世界的字放進一張表（1988–1991）

- 1987–1988 年 Xerox 的 Joe Becker 與 Apple 的 Lee Collins、Mark Davis 開始設計「Unicode 88」，
  目標是**一個固定 16 bits 的萬國碼**，當時估計 65,536 個位置夠用（後來證明不夠）。
- Unicode 1.0 於 1991 年 10 月發布（24 種文字、7,129 個字元），ISO 同步制定 ISO/IEC 10646（1993）。中日韓漢字被「認同」為同一批碼位
  （**Han unification**，中日韓統一表意文字），節省空間但也引發字形爭議。
- 1996 年 7 月 Unicode 2.0 引入代理對（surrogate pair），碼位擴充到 17 個平面共 1,114,112 個，
  這就是為什麼今天的 emoji（U+1F600 等）需要 4 bytes。最新的 Unicode 17.0（2025 年 9 月）收錄 159,801 個字元、172 種文字。
- 每個字元有一個編號叫 **code point**，寫成 U+XXXX。「多」是 U+591A。注意這只是編號，還沒說要怎麼存。

## 6. UTF-8：在餐廳紙巾上設計出來的存法（1992）

- 早期 Unicode 假設「每個字 2 bytes」（UCS-2），但這對 Unix 是災難：
  ASCII 檔案全部要重存、字串裡會出現 0x00 byte 讓 C 的 `strlen` 失效、`/` 這種路徑分隔符會出現在其他字的一半裡。
- 1992 年 9 月 2 日，貝爾實驗室的 **Ken Thompson**（Unix 與 C 的共同發明人）和 **Rob Pike** 在紐澤西一家餐廳，
  把 X/Open 提出的 FSS-UTF 草案改成今天的 UTF-8，據 Pike 的回憶是「寫在餐廳的墊紙上」，
  一週內就把整個 Plan 9 作業系統改成 UTF-8。
- 設計目標與達成方式，正是講義 1.2 那張表的每一列：

  | 目標 | 做法 |
  |---|---|
  | 與 ASCII 完全相容 | U+0000–U+007F 就是原本的 1 byte，最高位元為 0 |
  | 字串裡不出現 0x00 與 ASCII 符號 | 所有多位元組序列的每個 byte 都 ≥ 0x80 |
  | 從任何位置都能找到字元邊界 | 續位元組固定 `10xxxxxx`，前導 byte 固定 `110`/`1110`/`11110` 開頭 |
  | 前導 byte 自己說明長度 | 前導 byte 開頭有幾個 1 就是幾 bytes |
  | 保持 code point 排序 | byte 序列的大小順序與 code point 一致 |

- 1993 年 1 月在聖地牙哥 USENIX 冬季會議發表的論文標題就叫 *Hello World or Καλημέρα κόσμε or こんにちは 世界*。
- 標準化：RFC 2044（1996）→ RFC 2279（1998）→ **RFC 3629（2003）**，最後一版把長度限制在 4 bytes、碼位上限 U+10FFFF。
- 今天約 99% 的網頁用 UTF-8（W3Techs 2026 年統計為 99.1%），Linux、macOS、Git、Python 3、Go、Rust 都以 UTF-8 為預設。
  Windows 內部仍用 UTF-16，所以 Windows 同學會遇到本週講義提到的 BOM 與主控台代碼頁問題。

## 7. BOM：一個歷史遺留的記號

- U+FEFF 原本是 UTF-16 用來標示位元組順序（big-endian 或 little-endian）的「位元組順序記號」。
- UTF-8 沒有位元組順序問題，理論上不需要它，但微軟的工具習慣在 UTF-8 檔頭也寫入 `EF BB BF` 當作「這是 UTF-8」的標記。
- Unicode 標準明說 UTF-8 的 BOM「既不要求也不建議」。Unix 世界的工具多半不認它，會把它當成內容的一部分。
  這就是為什麼本週 data/sample_bom.txt 用 utf8_dump 看會多出第 0 個「字元」。
- 實務上最常把 BOM 帶進你資料的是 **Excel 的「CSV UTF-8」**（它靠 BOM 認編碼）與舊版 Windows 工具；
  Python 的 `utf-8-sig` 編碼、.NET 的 `StreamReader`、Java 的多數 JSON 函式庫會自動剝掉，
  而 C 標準函式庫、`gcc`、`bash`、`JSON.parse` 不會。寫 C 的人要自己處理，這是本課程明訂「跳過不計」的原因。

## 8. 回到這門課

| 本週講義／作業 | 對應的歷史 |
|---|---|
| utf8_dump.c 的 `utf8_len` | 第 6 節：前導 byte 自述長度 |
| chat.c 折行時往回找 `10xxxxxx` | 第 6 節：自同步；Big5 做不到（第 4 節） |
| MP1 要同時處理 Big5 與 UTF-8 | 第 4、6 節：台灣真實檔案常混用 |
| MP1 的 `"\n"` `"\r"` `"\t"` | 第 2 節：ASCII 控制字元 |
| BOM 算不算符號 | 第 7 節 |
| 第 5 週（10/5）的 Huffman | 第 1 節：摩斯電碼的直覺 |

---

## 參考文獻

依「先讀哪個」排序。前三篇是必讀，其餘查證用。

1. **Joel Spolsky, “The Absolute Minimum Every Software Developer Absolutely, Positively Must Know About Unicode and Character Sets (No Excuses!)”, 2003.**
   <https://www.joelonsoftware.com/2003/10/08/the-absolute-minimum-every-software-developer-absolutely-positively-must-know-about-unicode-and-character-sets-no-excuses/>
   二十年來最多人推薦的入門文，講清楚字元集與編碼的差別、code page 亂碼、以及「純文字不存在」。
2. **Rob Pike, “UTF-8 history”, 2003.** <https://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt>
   Pike 本人回憶 1992 年 9 月那個晚上的 email，含 Thompson 當時手寫的設計表，一頁就看完。
3. **Rob Pike and Ken Thompson, “Hello World or Καλημέρα κόσμε or こんにちは 世界”, Proc. USENIX Winter 1993.**
   <https://doc.cat-v.org/plan_9/4th_edition/papers/utf>
   UTF-8 的原始論文，解釋為何 UCS-2 對 Unix 行不通、以及 Plan 9 如何一週內全面轉換。
4. **F. Yergeau, “UTF-8, a transformation format of ISO 10646”, RFC 3629, IETF, 2003.** <https://www.rfc-editor.org/rfc/rfc3629>
   現行標準，第 3 節的表格就是講義 1.2 的表；第 6 節談 BOM。
5. **The Unicode Consortium, *The Unicode Standard*, Version 17.0, 2025.** <https://www.unicode.org/versions/Unicode17.0.0/>
   第 2 章「General Structure」講 code point、平面與編碼形式；第 23.8 節講 BOM（Specials）。
6. **Markus Kuhn, “UTF-8 and Unicode FAQ for Unix/Linux”.** <https://www.cl.cam.ac.uk/~mgk25/unicode.html>
   Unix 環境下的實務 FAQ，含 UTF-8 的合法序列定義與各種邊界案例，寫 MP1 時可查。
7. **Ken Lunde, *CJKV Information Processing*, 2nd ed., O'Reilly, 2009.**
   中日韓越編碼的權威專書，第 3 章 Character Set Standards 與第 4 章 Encoding Methods 詳述 Big5、CNS 11643、GB、Shift_JIS 的結構與歷史。
8. **Charles E. Mackenzie, *Coded Character Sets, History and Development*, Addison-Wesley, 1980.**
   ASCII 與 EBCDIC 誕生過程的第一手記錄，作者是 IBM 標準委員會成員。
   全文掃描檔收錄於 bitsavers.org 的 `pdf/ibm/history/` 目錄（該站擋自動連線，請用瀏覽器開）。
9. **Bob Bemer, ASCII history essays（個人網站，經 Internet Archive 保存）.** <https://web.archive.org/web/2020/https://www.bobbemer.com/>
   被稱為「ASCII 之父」的 Bemer 自述 7-bit 與跳脫字元的設計理由；原站已下線，請用存檔版。
10. **國家發展委員會，CNS 11643 中文全字庫。** <https://www.cns11643.gov.tw/>
    台灣官方字集，可查每個字在 Big5、CNS、Unicode 的對應，以及「Big5 沒有的姓名用字」如何處理。
11. **Wikipedia：[Big5](https://zh.wikipedia.org/wiki/大五碼)、[UTF-8](https://en.wikipedia.org/wiki/UTF-8)、[ASCII](https://en.wikipedia.org/wiki/ASCII)、[Mojibake](https://en.wikipedia.org/wiki/Mojibake)。**
    查年份與碼位範圍方便，但引用時請回到上列一手來源。
12. **Jennifer Burg, *The Science of Digital Media*, Pearson, 2009, Ch. 1.**（本課程參考書）
    數位資料表示的總覽，把文字、聲音、影像放在同一框架下。
