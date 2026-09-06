# samples_2025-python — 正確結果對照工具（Python 參考實作）

這裡是 MP1–MP5 的 **Python 正確實作**，給同學**對答案**用：
你的 C 程式跑出來的結果，應該要和這些 Python 程式的輸出一致。
（Python 用的是高階寫法——`Counter`、`heapq`、`numpy`、`wave` 模組——
和作業要求的 C 實作方式完全不同，所以看得懂它也不能拿來抄 C 的寫法。）

## 各程式用法

```
# MP1 符號統計
python3 mini_project_1/symbol_stats.py input.txt output.csv

# MP2 波形產生＋SQNR（需要 numpy）
python3 mini_project_2/wavegen.py 8000 16 1 sine 440 0.8 0.5 out.wav

# MP3 定長編碼
python3 mini_project_3/flc_codec.py encode input.txt codebook.csv encoded.bin
python3 mini_project_3/flc_codec.py decode encoded.bin codebook.csv output.txt

# MP4 Huffman
python3 mini_project_4/huffman_codec.py encode input.txt codebook.csv encoded.bin
python3 mini_project_4/huffman_codec.py decode encoded.bin codebook.csv output.txt

# MP5 訊號產生與 spectrogram（需要 numpy）
python3 mini_project_5/spectrogram.py gen 8000 out.wav
python3 mini_project_5/spectrogram.py spec 20 hamming 32 10 in.wav spec.txt
```

## 怎麼對答案

| 作業 | 對法 |
|---|---|
| MP1 | 輸出 CSV 應與你的 C 版**完全相同**（`diff` 比對） |
| MP2 | WAV 檔應**逐 byte 相同**（`cmp`）；SQNR 值應相同 |
| MP3 | codebook 與 encoded.bin 應**逐 byte 相同** |
| MP4 | Huffman 樹平手順序可能不同：改驗 (1) encoded.bin **大小相同**；(2) 用這支 decoder 解你的 bin＋codebook 應還原出原文；(3) 平均編碼長度相同 |
| MP5 | 產生的 WAV 應逐 byte 相同；spectrogram 數值應一致（浮點最後一位可能有微小差異，用數值比對而非文字 diff） |

安裝 numpy：`pip install numpy`
