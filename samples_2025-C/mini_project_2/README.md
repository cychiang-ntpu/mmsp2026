# mini project-2: Function Generator

> **注意**：本次作業的程式碼樣本暫不公開（將配合課程進度釋出），程式碼中與演算法核心相關的關鍵行已移除或以亂碼取代（無法直接編譯／抄用），整體結構與其餘內容保留，供同學參考寫法與評語。


### 1. 作業規定
請看：[**作業規定**](https://hackmd.io/@ntpu-ce-mmsp/mmsp-2025-mini-project-2)

### 2. 作業樣本和評分

依照作業成績高、中、低排序，各取兩個樣本；經匿名處理。

|檔案|評分|評語|
|-|-|-|
|[`HIGH/mini_prj_2_100.c`](HIGH/mini_prj_2_100.c)|100|sqnr 正確，波形正確|
|[`HIGH/mini_prj_2_95.c`](HIGH/mini_prj_2_95.c)|95|整體來說已經很好。建議觀察在極值 (e.g. sine wave最高點) 是否發生 overflow，超出了你設定的資料型態能儲存的範圍|
|[`MEDIUM/mini_prj_2_80.c`](MEDIUM/mini_prj_2_80.c)|80|sqnr 計算錯誤，波形正確|
|[`MEDIUM/mini_prj_2_70.c`](MEDIUM/mini_prj_2_70.c)|70|sqnr 計算錯誤，波形振幅大小錯誤|
|[`LOW/mini_prj_2_50.c`](LOW/mini_prj_2_50.c)|50|SQNR 計算錯誤、wavetype有缺少 (題目的表格中提到的都需要有)|
|[`LOW/mini_prj_2_50(1).c`](LOW/mini_prj_2_50%281%29.c)|50|SQNR 結果有誤 (量化過程有錯)，SQNR 結果沒有輸出到 stderr|


