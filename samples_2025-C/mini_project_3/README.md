# mini project-3: 簡易文字編碼解碼器製作

> **注意**：本次作業的程式碼樣本暫不公開（將配合課程進度釋出），程式碼中與演算法核心相關的關鍵行已移除或以亂碼取代（無法直接編譯／抄用），整體結構與其餘內容保留，供同學參考寫法與評語。


### 1. 作業規定
請看：[**作業規定**](https://hackmd.io/@ntpu-ce-mmsp/mmsp-2025-mini-project-3)

### 2. 作業樣本和評分

依照作業成績高、中、低排序，各取兩個樣本；經匿名處理。

|檔案|評分|評語|
|-|-|-|
|[`HIGH/mini_prj_3_100`](HIGH/mini_prj_3_100)|100|全對！|
|[`HIGH/mini_prj_3_90`](HIGH/mini_prj_3_90)|90|沒有達到壓縮的效果 (encoded.bin 和 input.txt 大小相同)|
|[`MEDIUM/mini_prj_3_80`](MEDIUM/mini_prj_3_80)|80|MakeFile 格式可能有錯，再檢查一下！Makefile:7: *** missing separator.  Stop.|
|[`MEDIUM/mini_prj_3_70`](MEDIUM/mini_prj_3_70)|70|要能夠使用 `make` 指令來執行 Makefile 全部的流程，且 Makefile 功能不完整，也要包含執行編譯後的 `.exe` 檔的功能。`encoder.exe` 執行後有誤，只有 `codebook.csv`缺少 `encoded.bin`。|
|[`LOW/mini_prj_3_60`](LOW/mini_prj_3_60)|60|MakeFile 沒有指定正確的 `.c` 檔名稱。decoder 未完成。|
|[`LOW/mini_prj_3_50`](LOW/mini_prj_3_50)|50|要能夠使用 `make` 指令來執行Makefile全部的流程。從 encoder 無法 build，後續無法驗證。|