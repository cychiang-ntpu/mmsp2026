# 第 5 週（2026/10/5）｜定長編碼、Huffman 實作

對應：MP3、MP4、Team 1 第 3 週

本週沒有另外的講義；上課用的是第 3 週的材料與 MP 的規格：

- 觀念：[第 3 週講義 1.3–1.7](../wk03_0921_team1-kickoff/README.md#第一節huffman-coding)（熵、prefix code、Huffman 建樹、符號的定義與 codebook 成本）。
- 建樹的資料結構與逐步追蹤：[huffman_trace.c](../wk03_0921_team1-kickoff/examples/huffman_trace.c)、[huffman_steps.html](../wk03_0921_team1-kickoff/slides/huffman_steps.html)。
- 定長編碼與 bit packing（MP3）：[samples_2025-C/mini_project_3](../../samples_2025-C/mini_project_3/README.md)、參考答案 [flc_codec.py](../../samples_2025-python/mini_project_3/flc_codec.py)。
- Huffman 編解碼（MP4）：[samples_2025-C/mini_project_4](../../samples_2025-C/mini_project_4/README.md)、參考答案 [huffman_codec.py](../../samples_2025-python/mini_project_4/huffman_codec.py)。
- Team 1 的高階對照：[python_ref/textlink.py](../../team_projects/team1_textlink/python_ref/textlink.py)（`--probe` 看每一步）。

## 回家作業／下週前要做的事

- Team 1：10/11（日）18:00 登錄 SHA；10/12 評測。
- MP3、MP4 用 `run_tests.sh mp3 mp4` 對答案；10/23 18:00 與 MP1–MP5 一起截止。
