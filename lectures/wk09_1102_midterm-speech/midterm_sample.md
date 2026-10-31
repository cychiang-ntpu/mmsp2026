# 期中考範例考題（練習用）

多媒體訊號處理｜2026 秋｜期中考：11/2（一）第一節，13:10–14:00，**50 分鐘、閉卷**，可帶計算機（不可帶手機）。
範圍：MP1–MP5 與 Team 1（第 2、3、7 週講義、各 MP 的規格與參考答案、Team 1 規格；第 7 週第二節 Team 2 的部分只考觀念）。

本卷是**範例**，題型、難度與配分比例與正式考題相同，題目不同；**正式考題比本卷短**（50 分鐘寫得完的量：選擇 10 題、計算 3 題、簡答 3 題、系統 1 題且小題較少），本卷多出來的題目是給你練習的。正式考題也是 100 分：選擇 20、計算 40、簡答 25、系統題 15。
建議先不看講義寫一遍，再對講義；每一題後面括號裡是對應的講義章節。

**考前要會的名詞**（上課講過；定義已補進第 3 週講義 1.1 與 1.7，這裡摘要）：

| 名詞 | 意思 |
|---|---|
| redundancy（冗餘） | 資料裡「不講也猜得到」的部分：常見的符號、重複的 pattern。壓縮就是把它拿掉；通道編碼是故意加回去 |
| source coding（信源編碼） | 依資料的統計特性（機率、熵）拿掉 redundancy，用最少 bits 表示：RLE、Huffman、MP3、JPEG 都是 |
| channel coding（通道編碼） | 依通道的錯誤特性加上受控的 redundancy，讓錯誤可以被偵測（checksum、CRC）或更正（FEC）；在 TCP 以下的層，TextLink 自己沒做 |
| codebook（碼簿） | 符號 ↔ code 的對照表；Huffman 的 codebook 由資料算出來，每個檔案不同，所以必須跟著資料送給解碼端 |
| side information（附帶資訊） | 解碼端還原資料**必須知道、但不是資料內容本身**的一切：codebook、符號種類、原始長度、補 0 的位元數、WAV 的其他 chunk……codebook 是其中最大的一項。壓縮率要把它算進去 |

計算題要**寫出算式**；只有答案沒有過程不給分。log₂ 的值可用 log₂3 ≈ 1.585、log₂5 ≈ 2.322、log₂7 ≈ 2.807；log₁₀2 ≈ 0.301。

---

## 一、選擇題（每題 2 分，共 20 分；單選）

1. 下列哪一個 byte 序列**不是**合法的 UTF-8？（wk02 1.2b）
   (A) `E5 A4 9A`　(B) `C0 AF`　(C) `F0 9F 98 80`　(D) `41 0A`

2. 一個 UTF-8 文字檔開頭有 `EF BB BF` 三個 bytes，它是：（wk02 1.5）
   (A) 第一個字元　(B) BOM，不算內容　(C) 檔案損毀　(D) 換行符號

3. 8 kHz 取樣、16-bit、單聲道、10 秒的 PCM，不含檔頭的資料量是：（wk03 2.5）
   (A) 80 000 bytes　(B) 160 000 bytes　(C) 320 000 bytes　(D) 1 280 000 bytes

4. 以 8 kHz 取樣一個 5 000 Hz 的弦波，取樣後聽起來像幾 Hz？（wk03 2.2）
   (A) 5 000　(B) 4 000　(C) 3 000　(D) 1 000

5. 關於熵 H 與 Huffman 平均碼長 L，下列何者**正確**？（wk03 1.3、1.6）
   (A) L < H 是可能的　(B) H ≤ L < H + 1（符號種類 ≥ 2 時）　(C) L = H 永遠成立　(D) L 與 H 無關

6. prefix code 的意思是：（wk03 1.4）
   (A) 每個 code 一樣長　(B) 沒有任何 code 是另一個 code 的開頭　(C) code 都以 0 開頭　(D) code 依字母順序排列

7. Team 1 的 frame 用 4 bytes 的長度前綴，主要是為了解決 TCP 的什麼問題？（wk02 3.3、wk03 3.3）
   (A) 封包遺失　(B) 沒有訊息邊界（黏包／半包）　(C) 傳輸太慢　(D) 無法傳中文

8. WAV 檔頭裡的多 byte 整數與 Team 1 frame 的長度欄位，位元組順序分別是：（wk03 2.6、3.3）
   (A) 都是 big-endian　(B) 都是 little-endian　(C) WAV little-endian、frame big-endian　(D) WAV big-endian、frame little-endian

9. STFT 中，決定**能不能分辨兩個相近的頻率**（主瓣寬度）的是：（wk07 1.6）
   (A) 視窗長度 P　(B) DFT 長度 N　(C) hop M　(D) 窗的種類

10. 對 frame 乘上 Hamming window 而不是直接切（rectangular）的主要目的是：（wk07 1.5）
    (A) 提高頻率解析度　(B) 減少 frame 兩端不連續造成的 leakage　(C) 減少計算量　(D) 讓 N 變成 2 的次方

---

## 二、計算題（共 40 分）

### 題 1｜UTF-8 與符號統計（MP1；10 分）

檔案內容為 `多媒體 OK`（「體」與「O」之間有一個空白）後接一個換行（`\n`），存成 UTF-8，開頭有 BOM。已知「多」= U+591A、「媒」= U+5A92、「體」= U+9AD4。

(a) 這個檔案共幾個 bytes？（含 BOM）（3 分）
(b) 依 MP1 的規則統計，這個檔案有幾個符號（字元）？幾**種**符號？（3 分）
(c) 把 U+591A 編成 UTF-8，寫出三個 bytes（十六進位），並寫出每個 byte 的二進位拆法。（4 分）

### 題 2｜取樣與量化（MP2；10 分）

用 MP2 的方法產生 2 秒、440 Hz、振幅 1.0 的弦波，f_s = 8 000 Hz、16-bit、單聲道。

(a) 這個 WAV 檔多大？（44 bytes 檔頭＋資料）（3 分）
(b) 量到的 SQNR 大約幾 dB？寫出用的公式（6.02n 或 6.02n + 1.76 都可以，但要說明是哪一種定義）。（3 分）
(c) 振幅改成 0.5，SQNR 大約變成幾 dB？為什麼？（3 分）
(d) MP2 以 8-bit 量化（full scale = 127，四捨五入）、x = 0.30 這個 sample：量化後的整數值是多少？量化誤差（x − x̂）大約是多少？（1 分）

### 題 3｜定長編碼與 bit packing（MP3；8 分）

一個文字檔有 1 000 個字元、90 種不同符號。MP3 的 codebook 要多放一個 EOF 符號。

(a) 定長編碼每個符號至少要幾 bits？為什麼？（3 分）
(b) 編碼後（含 EOF、補 0 到整數 byte）的 `encoded.bin` 幾 bytes？（3 分）
(c) 若原檔是 1 800 bytes 的 UTF-8，壓縮率（壓縮後 ÷ 原始）是多少？（2 分）

### 題 4｜熵與 Huffman（MP4、Team 1；8 分）

一段訊息的符號次數：A 4 次、B 2 次、C 1 次、D 1 次（共 8 個符號）。

(a) 算熵 H（bits／符號）。（3 分）
(b) 畫 Huffman tree（寫出每一輪合併哪兩個），寫出四個符號的 code。（3 分）
(c) 算平均碼長 L 與編碼後總 bits；驗證 H ≤ L < H + 1。（2 分）

### 題 5｜STFT 參數（MP5；4 分）

f_s = 16 000 Hz，視窗 25 ms、DFT 32 ms、hop 10 ms，Hamming 窗。

(a) P、N、M 各是幾個 sample？（1 分）
(b) DFT 相鄰兩根（bin）的頻率間距是幾 Hz？第 k = 100 根對應幾 Hz？（1 分）
(c) 一個 frame 直接算 DFT 需要幾次複數乘法？用 FFT 大約幾次？（依第 7 週講義 1.4 的算法）（2 分）

---

## 三、簡答題（共 25 分；每題 3–5 句即可）

1. **（5 分）** 什麼是 **side information**？什麼是 **codebook**？兩者是什麼關係？以 Team 1 的 Huffman 區塊（`kind | orig_len | n_syms | K | codebook | bitstream`）為例，指出哪些欄位是 side information、哪些是真正的資料，並說明為什麼壓縮率一定要把 side information 算進去。（wk03 1.7、Team 1「壓縮率」）

2. **（4 分）** Team 1 送一則文字「嗨」（UTF-8 = `E5 97 A8`）、type = TEXT_RAW（0x01）。寫出整個 frame 的 bytes（十六進位），並說明接收端怎麼知道這則訊息在哪裡結束。（wk03 3.3）

3. **（4 分）** 同一個檔案，Huffman 以「byte」為符號和以「UTF-8 字元」為符號，壓縮率可能差很多。用中文文字檔為例說明為什麼。（wk03 1.7、Team 1）

4. **（4 分）** Team 1 量測發現：小檔案用 Huffman 傳**比不壓縮還慢**。列出至少兩個可能的原因，並說明要怎麼量才能分清楚是哪一個。（Team 1「量測與報告」）

5. **（4 分）** 解釋「zero padding 不會提高頻率解析度」：N 從 400 補到 512，頻譜上多出來的那些根代表什麼？要提高解析度該改哪個參數，代價是什麼？（wk07 1.6）

6. **（4 分）** 用 rectangular window 切一段 170 Hz 的弦波（f_s = 8 000、256 點）做 DFT，結果不是一根而是「一座山加很多小山」。說明原因，並說明 Hamming window 改善了什麼、犧牲了什麼（給出大約的 side lobe dB 值）。（wk07 1.5）

---

## 四、系統題（共 15 分）

### TextLink 端對端的壓縮傳輸 block diagram

Team 1 的 `textlink send <ip> <port> big.txt --huff`，對方跑 `textlink recv <port> <outdir>`，中間經過 TCP。
請畫出**從送端的檔案到收端存下的檔案**完整的 block diagram，並回答下列問題。

(a) **畫圖（6 分）**：以方塊代表處理步驟、箭頭代表資料流，送端與收端各一排，中間是網路。至少要包含（名稱可以自己取，但功能要對）：
　　讀檔 → 決定符號種類 → 統計次數（histogram）→ 建 Huffman tree／codebook → 位元打包 → 組 frame（長度前綴＋type）→ TCP 送出 ──網路──▶
　　TCP 收 → 拆 frame → 解析 header／codebook → 位元解碼 → 還原 bytes → 寫檔；以及最後收端回送的 ack。
　　每個箭頭旁邊**標出那一段資料是什麼**（例如「UTF-8 bytes」、「符號序列」、「codebook＋bitstream」、「frame bytes」）。

(b) **（2 分）** 在圖上圈出：壓縮率 = wire_bytes ÷ file_bytes 的兩個量各是在**哪個箭頭**量的；encode_ms、send_ms、decode_ms 各涵蓋圖上的哪幾個方塊。

(c) **（2 分）** 這張圖裡哪些方塊是**有失真**的？（提示：Huffman 是無失真；想一想 UTF-8 不合法的 bytes、以及 `.wav` 以 16-bit sample 為符號時 `data` 以外的 chunk 怎麼處理。）

(d) **（2 分）** 如果把中間的 TCP 換成 Team 2 的 UDP，圖上哪幾個方塊要改或新增？寫出方塊名稱即可。

(e) **（3 分）** 什麼是 **source coding**？什麼是 **channel coding**？在你的圖上標出兩者各在哪個位置（TextLink 自己的程式裡有沒有 channel coding？如果沒有，它發生在圖的哪一層？），並用一句話說明兩者的目的為什麼**相反**（一個拿掉 redundancy、一個加上 redundancy）卻可以分開設計。（wk03 1.1）

---

## 考試須知

- 閉卷；可帶一台**不能上網**的計算機。
- 答案寫在答案卷上，計算題要有過程。
- 考完接著第二、三節的語音實驗，請帶筆電（或用教室電腦）。
