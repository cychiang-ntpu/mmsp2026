# 第 5 週投影片大綱（2026/10/5）

> 沒有另外製作投影片；上課以講義 [../README.md](../README.md) 為主，搭配終端機示範與第 3 週的建樹動畫。
> 這一週是第 3 週的「怎麼寫成 C」：每個觀念都接在第 3 週的章節後面，講的時候要回指。

## 教材（終端機為主）

| 檔案 | 用在 | 操作 |
|---|---|---|
| [../examples/bitio_trace.c](../examples/bitio_trace.c) | 第一節 1.2、1.3 | `./bitio_trace`；`--no-stop`；`--codes` 換一組 code |
| [../examples/huffman_build.c](../examples/huffman_build.c) | 第二節 2.1、2.2 | `./huffman_build`；`make trace`；`--file … --eof` |
| [../examples/codebook_check.py](../examples/codebook_check.py) | 第一節 1.4、第二節 2.5 | `python3 codebook_check.py 某.csv 某.bin --bits`；現場把 CSV 改壞一個 code 再跑 |
| [../examples/block_dump.py](../examples/block_dump.py) | 第三節 3.1 | `python3 block_dump.py`（44 bytes 逐欄）；`--file 語音.wav`（s16：codebook 12,343 組、head 44） |
| [../examples/canon_decode_trace.c](../examples/canon_decode_trace.c) | 第三節 3.2 | `./canon_decode_trace`；`--lens A:1,B:1,C:1`（拒絕）；`--n 20`（拒絕） |
| `huffman_build --sym s16／byte` | 第三節 3.3 | `make report`：報告表兩列，和 python_ref inspect 的數字相同 |
| 第 3 週 [huffman_steps.html](../../wk03_0921_team1-kickoff/slides/huffman_steps.html) | 第二節 2.1 對照 | 8 色影像走一遍，和兩個佇列的輸出並排 |
| 第 3 週 `huffman_trace` | 第一節 1.1 | `huffman_trace "MISSISSIPPI RIVER"` 對回家作業 |

## 第一節：從作業到 bytes

1. **回家作業對答案**〔`huffman_trace "MISSISSIPPI RIVER"`〕：H = 2.6987、L = 46 ÷ 17 = 2.7059；你的 code 和它不一樣是正常的；**Σ 次數 × 長度 = 46 不會變**
1a. 這件事今天出現三次：MP4 的 CI 只比 bin 大小＋交叉解碼；Team 1 兩端要重建同一張表；MP3 定長沒有平手問題所以要逐 byte 相同
2. **從 code 到 bytes**：ABRACADABRA 的 23 bits → `01101110 10001010 1101110[0]` → `6E 8A DC`；MSB first；DEFLATE 相反；兩端一致
3. **bit writer**：`acc`、`nbits`；`acc = (acc << len) | code`；滿 8 吐最高 8 bits；`& mask`；flush 補 0；`uint64_t` 與 56 bits 上限
4. **bit reader**：`(cur >> (7 - bit)) & 1`；讀完 8 個換 byte；資料用完回 −1〔`./bitio_trace`：逐行看 acc 與吐出的 byte，再逐 bit 走樹讀回〕
5. **解碼端怎麼知道該停**〔`./bitio_trace --no-stop`：第 12 個 A〕：(a) 記 N（Team 1）、(b) EOF 符號（MP3、MP4）、(c) 記有效 bits；停止條件來自區塊裡的數字
6. **MP3 定長編碼**〔`flc_codec.py encode` 回家作業的字串，`cat` 看 CSV〕：K 種 → 7 bits；排序規則（少→多、bytes 長度、bytes 值）；EOF 次數 0 排最後；每一欄
7. **數字**：17 bytes → 16 bytes（94%）；中英文混合檔 162 → 93（57%）是符號定義的功勞，Huffman 76（47%）才是 code 指派的功勞
8. **CI 怎麼測 MP3**：codebook 與 bin 逐 byte 相同；decode 還原；執行檔叫 `mp3`；去年的評語都不是演算法（Tab、`.c` 名稱、沒真的打包）
9. **codebook_check.py**〔`--bits` 看位元流、EOF、補位〕
10. **FLC → Huffman 只換一步**：同一條管線，共用的函式；Team 1 再加兩種切法

## 第二節：Huffman 的 C 實作

11. **K 很大**：第 3 週的插回佇列是 K²；K = 12,343（語音檔的 sample）、65,536；要換做法
12. **兩個佇列**〔`./huffman_build`、`make trace`〕：葉排序一次 → Q1；新節點只會越來越大 → Q2 不排序；比兩個頭；O(K log K)；L 仍是 2.0938、yellow 與 blue 對調
13. **型別**：根的 weight 用 `uint64_t`；統計表以符號值為索引，`calloc` 不放 stack
14. **canonical**：只留長度；規則（依長度、符號排序；0；+1；變長就左移）；8 色影像的表；是 prefix code、L 不變
15. **為什麼要 canonical**：codebook 只傳「符號＋長度」（s16：3 bytes × K ≈ 37 KB）；DEFLATE 也這樣；`huffman_demo.py` 第 3 段印的就是它
16. **解碼三種寫法**：逐 bit 走樹（MP4）；canonical 區間查表（Team 1）；一次讀 8–16 bits 查表（JPEG、ZIP；不要求）；兩個一定要防的壞輸入
17. **MP4 規格**〔`huffman_codec.py encode`，`cat` 看 CSV〕：五欄；依（長度，code）排序；EOF 次數 1；N = 18、7 bytes；EOF 的代價 2.7059 → 2.8889
18. **CI 怎麼測 MP4**：bin 大小相同（為什麼可以）；Python 解你的；你解 Python 的；「encode 與 decode 只能靠 codebook 溝通」
19. **常見 bug 清單**：bit order、EOF、`\r`、BOM、K = 1 長度 0、補位被解成符號、不穩定的平手
20. **codebook_check.py 檢查的每一條對應哪個觀念**；`make check`：樹不同、code 不同、數字必須相同

## 第三節：Team 1 收尾

21. **路線圖**：五個 TODO ↔ 今天的工具（表）；這一週每天做什麼；上一週的 `make test` round-trip 過了嗎
22. **interface.md 的表**〔`python3 block_dump.py`：44 bytes 一欄一欄讀；再 `--file 語音.wav` 看 12,343 組 codebook 與 head 44〕：符號種類、原始長度、N、K、codebook（符號＋長度）、s16 保留的 bytes、bitstream；K = 0／1、長度 > 56 的答案
23. **壞輸入**〔`./canon_decode_trace`：first／count 表；`--lens A:1,B:1,C:1` 與 `--n 20` 被拒絕〕：先檢查再 `malloc`；Kraft > 1；截半、`max_out − 1`；V 的四種破壞；評測第 6 項
24. **接上 `--huff`**：transfer.c 的流程；退回 byte；今天就接、先 byte 再 s16；`block_dump --out` 的區塊可以餵自己的 decoder
25. **壓縮率分析表**〔`make report`〕：每一欄從哪裡來；C 版算出的 K = 12,343、80.55% 和 python_ref 相同；`127.0.0.1` 上 `--huff` 比較慢是正常的；損益平衡頻寬
26. **10/11 18:00 登錄、10/12 評測流程**：repo 必備；每組 12 分鐘 P → D → V → 口試；口試題目就是今天的內容；今天回去就兩台電腦傳一次
27. **收尾**：MP 截止 10/23；10/12 評測、10/19 Team 2＋MP5；期中考 11/2 範圍到第 7 週
