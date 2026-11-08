# adpcm_huffman_system/：ADPCM 或／和 Huffman 系統（你們寫）——`--codec adpcm`、`--codec huff-mulaw`

[← 教學總覽](../tutorial/TUTORIAL.md)｜[← 規格](../README.md)｜上一個：[mulaw_system](../mulaw_system/README.md)

> 這個資料夾給的 [`voicelink.c`](voicelink.c) 和 `mulaw_system/` 一開始是同一個框架（只有 pcm）。**建議先把你們做好的 `../mulaw_system/voicelink.c` 複製過來覆蓋它**，
> 這樣 μ-law 已經在表裡，Huffman 才有東西可以疊。程式裡搜尋 **`★ 第 9 步`** 就是要動手的地方。
> 兩個 codec 各 10 分（11/23 demo），做一個或兩個都可以；演算法的零件在 [m9_adpcm](../tutorial/m9_adpcm/README.md)（ADPCM）與 Team 1 的 Huffman。
> 繳交：team repo 照 `team2_voicelink/` 的結構，這個資料夾就叫 `adpcm_huffman_system/`，程式就叫 `voicelink.c`。

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/adpcm_huffman_system` | `cd team_projects\team2_voicelink\adpcm_huffman_system` |
| `cp ../mulaw_system/voicelink.c voicelink.c`（覆蓋框架） | `copy ..\mulaw_system\voicelink.c voicelink.c` |
| `make` | `mingw32-make` |
| `S=../../lectures/wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `$S = "..\..\lectures\wk07_1019_audio-pcm\data\speech_osr_16k.wav"` |

---

### 9a. ADPCM（demo +10）：表裡加第三列

**目標**：`--codec adpcm`：每包 320 個 sample → 160 bytes（type 0x05）；SQNR ≥ 20 dB、`bytes` ≈ 四分之一（含 header 0.264）；**過測試 9**，和 `python_ref` 互通。

**用到的模組**：m9（兩張表、`adpcm_state_t`、`adpcm_reconstruct`、`adpcm_encode_one` 原封不動搬進來）。

**要加的東西**（m9 README 第 6 節寫得更細）

1. 搬 m9 的表與三個函式。
2. 寫 frame 版（每包**獨立**：開頭歸零，這是規格，也是 `python_ref` 的做法）：

   ```c
   static void adpcm_encode_frame(const int16_t *pcm, uint8_t *out) {
       adpcm_state_t st; state_reset(&st);                       /* pred = 0, index = ADPCM_INDEX_FIRST */
       for (int i = 0; i < FRAME_SAMPLES; i++) {
           if (i == 1) st.index = ADPCM_INDEX0;                  /* 第 2 個 sample 起換成中等 step（python_ref 也是） */
           int nib = adpcm_encode_one(&st, pcm[i]);
           if (i % 2 == 0) out[i / 2] = (uint8_t)(nib << 4);     /* 先到的在高 4 bits */
           else            out[i / 2] |= (uint8_t)nib;
       }
   }
   static void adpcm_decode_frame(const uint8_t *in, int16_t *pcm) {
       adpcm_state_t st; state_reset(&st);
       for (int i = 0; i < FRAME_SAMPLES; i++) {
           if (i == 1) st.index = ADPCM_INDEX0;
           int nib = (i % 2 == 0) ? (in[i / 2] >> 4) : (in[i / 2] & 0x0F);
           adpcm_reconstruct(&st, nib);
           pcm[i] = (int16_t)st.pred;
       }
   }
   ```
3. 表裡加一列：`{ "adpcm", 0x05, FRAME_SAMPLES / 2, adpcm_encode_frame, adpcm_decode_frame },`。
4. 其他不用改。

**怎麼測**：和 mulaw_system 一樣的三種組合，`--only 9`；全套 `--only 1,2,3,4,5,6,7,8,9`。

**看到什麼才算對**：`SQNR 2x dB（≥ 20.0）`、`bytes … = 0.264`、`STATS codec=adpcm`、`--loss 0.05` 時 `concealed ≈ lost`。老師的語音檔約 25 dB，7 秒母音檔只有 21 dB（SQNR 和內容有關）。

**常見錯誤**：忘了第 2 個 sample 換 index（互通時 SQNR 崩）；nibble 順序反了（全是雜音）；`clampi` 被刪（爆音）；最後一包不足 320 要先補 0 再編（框架的 `frame` 已經做了 `memset`）。

---

### 9b. Huffman（demo +10）：疊在 μ-law 的碼字上，payload 長度不固定

**目標**：`--codec huff-mulaw`：先 μ-law（320 個 8-bit 碼字），再用一張**固定的 Huffman 表**把 320 個碼字編成位元串；type 0x06；
SQNR 和 μ-law 一樣、`bytes` 比 μ-law 少；**過測試 10**（你們的 send 對你們的 recv；`python_ref` 不會解 0x06）＋現場兩台互通。

**規格的 payload**（封包格式表的 0x06）：`1 byte 底層 codec 的 type（0x04）` ＋ `2 bytes 有效位元數 n（big-endian）` ＋ `⌈n/8⌉ bytes 位元串（MSB 先、最後補 0）`。

**要做的事**

1. **建表（離線，一次）**：拿老師的語音檔（`speech_osr_16k.wav`）跑一次 μ-law，統計 256 個碼字各出現幾次（出現 0 次的當 1，每個符號都要有碼），
   用 Team 1 的 Huffman 建樹，得到 256 個 (code, length)。寫成 C 的兩個陣列 `huff_code[256]`、`huff_len[256]`，**寫死在程式裡**，另存一份 `docs/huffman_table.txt` 一起交。
   兩端用同一張表，所以不用把表送過去——這張表就是期中考講的「事先約好的 side information」。
2. **表的介面要改一點**：Huffman 的 payload 長度不固定，所以 `encode` 要能回傳實際長度。做法：

   ```c
   size_t (*encode)(const int16_t *pcm, uint8_t *out);     /* 回傳寫了幾個 bytes */
   size_t      payload_max;                                 /* 表裡改存「最大長度」，parse_header 改成 len <= HDR_LEN + payload_max 且 len > HDR_LEN */
   ```
   pcm／mulaw／adpcm 的 encode 改成回傳固定長度即可（改四個函式各一行）。`decode` 多一個參數 `size_t n_bytes`。
3. **encode**：μ-law 320 bytes → 依表寫位元（一個 bit buffer，從 MSB 開始填，滿 8 bits 輸出一個 byte）→ 前面補 3 bytes 的標頭（0x04、n）。
4. **decode**：讀 3 bytes 標頭 → 逐 bit 走表（最簡單：對每個 bit 累積 code 與 length，在 256 個 (code,len) 裡找相符的；320 個符號為止）→ 320 個 μ-law byte → `mulaw_decode`。
5. 表裡加一列：`{ "huff-mulaw", 0x06, 3 + 320 /* 最差情況不會比原來大很多，給 3 + 400 */, huff_mulaw_encode, huff_mulaw_decode }`。

**怎麼測**：`python3 ../tests/run_mock_tests.py "./voicelink send" "./voicelink recv" $S --only 10 --with-huffman`（預設疊在 mulaw；`--huff-base adpcm` 疊在 ADPCM）。
看 `bytes … = 0.8x（要比 mulaw 少）`、`SQNR ≥ 30`、`STATS codec=huff-mulaw`。

**報告要回答**：省了幾 %？為什麼疊在 μ-law 上省得多（安靜段的碼字很集中）、疊在 ADPCM 上省得少（碼字分布平）？表為什麼可以不送？

**常見錯誤**：
- 用訓練檔以外的 WAV 出現了訓練時沒看過的碼字 → 表裡沒有碼 → 解不出來。所以「出現 0 次的當 1」一定要做。
- 位元順序：規格是 MSB 先；最後一個 byte 補 0 的位元不要被解成符號（用 n 控制，解滿 320 個就停）。
- 一包內自成一段（每包獨立編碼），掉一包不影響下一包；不要跨包延續 bit buffer。

---

### 這一步在評分裡

- 11/23 demo：ADPCM 過測試 9 ＋ 現場切 `--codec adpcm` → **+10**；Huffman 過測試 10 ＋ 現場切 `--codec huff-mulaw` 看 `bytes` 變少 → **+10**。（加上 raw 10、μ-law 10，demo 最多 40；三部分加總超過 100 以 100 計。）
- 程式碼實質審查（30）：正確性與互通、程式品質、個人貢獻（`git log` 要看得出誰寫了哪個 codec）。
