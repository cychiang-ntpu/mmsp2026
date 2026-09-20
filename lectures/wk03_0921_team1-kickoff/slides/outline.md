# 第 3 週投影片大綱（2026/9/21）

> 沒有另外製作投影片；上課以講義 [../README.md](../README.md) 為主，搭配四支互動網頁與終端機示範。
> 數學式與例子整理自 J. Burg《The Science of Digital Media》第 1 章、2023 講義 Chapter 2 與 DSP 2026 講義第三部分；素材來源與授權見講義末。

## 互動教材（瀏覽器直接開，不需要網路）

| 檔案 | 用在 | 操作 |
|---|---|---|
| [media_size.html](media_size.html) | 第一節 1.1（第二節 2.5 再回來一次） | 上方六個分頁；每個計算機上方的圓角按鈕是範例，按了就填好數字並展開算式 |
| [huffman_steps.html](huffman_steps.html) | 第一節 1.6 | 「下一步」或鍵盤 →；上方選單換例子，輸入框可打任何文字或 `名稱:次數,…` |
| [a2d_steps.html](a2d_steps.html) | 第二節 2.1–2.4 | 上方四個方塊切換階段；右邊拉桿與四個預設按鈕（正常、aliasing、2 bits、音量太小） |
| [wav_bmp_viewer.html](wav_bmp_viewer.html) | 第二節 2.2、2.3、2.6 | 真實的語音、音樂、照片：播放／顯示、檔頭逐 byte、換取樣率與 bit depth 再聽；右上可選自己的檔案。需要同資料夾的 samples.js |

## 第一節：Huffman coding

1. **沒壓縮有多大**〔切到 media_size.html〕：樣本數 × 每個樣本的 bits；四種媒體的公式；按範例：小說 1.4 MB、一首歌 40 MB、一張照片 34 MB、一分鐘 Full HD 10 GB
1a. **為什麼要壓縮**：lossless／lossy；dictionary、entropy、arithmetic、adaptive、differential 五種標籤；壓縮率 a:b 與 b÷a
1b. **常見格式與管線**〔「常見格式」「壓縮方法」分頁〕：codec ≠ 容器；ZIP、PNG、JPEG、MP3 的最後一步都是 Huffman；失真都發生在量化；視訊靠移動補償
2. **RLE**：20 個 pixel → (255,6)(242,4)(238,6)(255,4)；b = ⌈log2(r+1)/8⌉；20 bytes → 8 bytes；對聲音沒用
3. **熵**：H(S) = Σ pᵢ·log2(1/pᵢ)；「平均每個符號至少要幾 bits」
4. **例 1**：256 色各一次 → H = 8 bits，沒得省
5. **例 2**：8 色頻率表（100、100、20、20、5、5、3、3）→ H ≈ 2.006；black 1.356 bits、yellow 3.678 bits；**長度必須是整數**
6. **prefix code**：A=0、B=01、C=1 為什麼不行；符號都放在葉上就自動滿足
7. **Shannon-Fano**：由上往下切；8 色例子 → 長度 1、2、3、4、6、6、6、6 → L = 2.094
8. **Huffman**：由下往上合併；四個步驟 →〔切到動畫：8 色影像，一步一步走〕
9. **三個數字**：H = 2.0063、L = 2.0938（效率 95.8%）、定長 3 bits；H ≤ L < H+1；768 → 536 bits（69.8%）
10. **動畫換例子**：機率相同 → 沒得省；2 的負次方 → L = H；只有一種符號 → 長度 1（定理的例外）
11. **程式裡長什麼樣子**〔切到終端機：`huffman_trace --step`〕：node[] ＋ queue[]；insertion；沿 parent 走出 code；K² 與 K log K
12. **符號是什麼比演算法重要**〔`entropy`〕：同一個檔 byte 75.15% vs 字元 45.65%；Team 1 的符號定義
13. **codebook 的成本**：ABRACADABRA 11 bytes → 3 bytes ＋ codebook；MP3、MP4、Team 1、第 5 週的關係

## 第二節：WAV 與 PCM

13a. **整條路**：說話 → 麥克風（振膜；動圈式／電容式）→ 類比訊號 → ADC → 計算機 → DAC → 擴大器 → 喇叭；analog＝「比擬」、digital 來自 digitus（手指）
14. **A/D 四步驟**〔切到動畫 a2d_steps.html，階段 ①→④〕：連續 → 取樣 → 量化 → 編碼；PCM；4-bit [0111]=+7、[0110]=+6、[1010]=−2
15. **常見規格**：CD 44.1k／16、語音辨識 16k／16、電話 8k／16
16. **取樣**：x[n] = x_c(nT)；T = 1/f、ω = 2πf；Nyquist：f_s > 2f_max；Nyquist frequency vs Nyquist rate
17. **aliasing**〔動畫按「aliasing：f=5000」＋ `alias_demo.py` 播兩個 WAV〕：sin(2π·5000n/8000) = −sin(2π·3000n/8000)；折回來的規律；頻譜複製（修過訊號與系統的）
18. **量化**：L = 2^m；Q(x) = Δ⌊x/Δ + ½⌋；e[n] = x − Q(x)；|e| ≤ Δ/2〔動畫階段 ③ 拉 bit depth〕
19. **clipping**〔動畫按「2 bits」＋ `quantize_demo.py` 的 3-bit 表〕：0.98 → 0.75，誤差 0.23 > Δ/2；去年 MP2 95 分的評語
19a. **用真的聲音聽**〔切到 wav_bmp_viewer.html〕：音樂 ÷4 不濾波 → aliasing；語音 16 → 8 → 4 bits → 沙沙聲、小聲的地方消失；放大 5 ms 看一根一根的 sample
20. **dB**：10log10(P/P₀) = 20log10(V/V₀)，由 P = V²/R 推導；功率 ×2 ≈ +3 dB、振幅 ×2 ≈ +6 dB
21. **SQNR (a)**：20log10(2^(n−1) ÷ ½) = 20log10(2^n) ≈ 6.02n；dynamic range
22. **SQNR (b)**：10log10(Σx²/Σe²)；滿刻度弦波 6.02n + 1.76 + 20log10(A)；實測表；音量減半 −6 dB〔動畫按「音量太小」〕
23. **資料量**：CD 1,411,200 bits/s、一分鐘 10.09 MB；影像 2.25 MB；影片 1.7 GB
24. **WAV 檔頭**：RIFF／chunk；44 bytes 欄位表〔hexdump speech_osr_8k.wav 逐欄讀〕
25. **三個陷阱**：little-endian（對照 Team 1 frame 的 big-endian）；16-bit 二補數 vs 8-bit 無號；data 不一定在第 44 byte
25a. **檢視器的 WAV／BMP 分頁**：欄位上色、公式驗算＝檔案實際大小；BMP 預告（B G R、由下往上、每列補到 4 的倍數）；每個顏色降到 3 bits 看色帶
26. **sample 的 histogram**〔`wav_info.py`〕：集中在 0 附近；sample 73.5% vs byte 83.2%；12,343 種符號的 codebook；差值編碼是 Team 2 的伏筆
27. **MP2 ＝ 用程式模擬 A/D**：對照表（數學式＝連續訊號、`t = n/fs` 的迴圈＝取樣、`rint(x × full_scale)`＝量化、寫 little-endian 整數＝編碼）；模擬的好處是 x 與 Q(x) 都在手上，SQNR 可以實際量；五個建議實驗
27a. **MP2 細節**：命令列、自動測試比什麼（WAV 逐 byte ＋ SQNR）、九個重點（檔頭、rint、8-bit +128、double、"wb"…）、去年的扣分原因

## 第三節：分組與 Team 1 說明

28. **分組公告**〔投影私有 repo 的 team1_announce.md〕：11 組、P／D／V、換角色今天登記、每個人都要有 commit
29. **建 team repo**：private、加 collaborators、複製 starter、必備檔案
30. **Team 1 一句話＋三個功能**：對應今天第一、二節的哪一段
31. **固定的部分**：四個指令、IP 與 port 不可寫死、跨機器評測、聊天中 /files 與 /send、STATS
32. **報告要回答的問題**：N、K、H、L、codebook、壓縮率；符號定義的影響；壓縮有沒有比較快；損益平衡頻寬
33. **最低驗收與時程**：10/11 18:00 登錄 SHA、10/12 評測；9/28 停課
34. **長度前綴 frame**：格式、`00 00 00 04 01 61 62 63`、先收標頭再收 payload、recv_all；big-endian、length 不可相信、send_all
35. **chunk_send.py**〔現場跑〕：5 則黏包＋逐 byte 半包
36. **starter**〔現場 `make test` → 46 TODO；開聊天看黃字〕：五個 TODO、建議順序、三週時程
37. **收尾**：跨機器的坑（防火牆、校園 Wi-Fi 隔離 → 手機熱點、WSL）；MP4 獨立完成；下次上課 10/5
