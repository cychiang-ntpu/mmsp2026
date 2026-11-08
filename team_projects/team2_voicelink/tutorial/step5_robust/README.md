## 第 5 步｜step5_robust.c：壞封包不當掉、ssrc 換了、seq 繞回、--bind、結束碼

[← 教學總覽](../TUTORIAL.md)｜上一個：[第 4 步 counters](../step4_counters/README.md)｜下一個：[第 6 步 live](../step6_live/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/step5_robust` | `cd team_projects\team2_voicelink\tutorial\step5_robust` |
| `make`（編出 `./step5_robust`（Windows `.\step5_robust.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`step5_robust.c`](step5_robust.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


**目標**：對 port 丟 100 包亂碼不當掉；對方重啟（ssrc 換了）就重置；seq 從 65535 繞到 0 照樣對；過測試 1–7（含 4、6）。

**用到的模組**：m6（`parse_header` 先看長度再讀欄位；`m6_header 65535` 的繞回）、m2（`--bind`：`udp_open` 的第二個參數）。

**要加的東西（為什麼）**

- **壞封包**：`parse_header` 從第 2 步起就「先檢查長度再讀欄位」，本來就不會讀到陣列外面；這一步把它計數、印出來，並把「type 不認識」也算進去。
  規則只有一條：收到什麼都不能 crash（測試 6 丟的長度包括 1、11、12、13、651、652、653、1400）。
- **ssrc 換了**（名詞解釋「ssrc」）：對方重啟後 seq 從 0 開始、ts 也從 0 開始，不重置的話全部變 late。`jb_reset` 清 buffer、位元表、播放進度，
  **計數器保留**（STATS 是整次執行的）。別的 ssrc 的 BYE 不理。
- **seq 繞回與位元表**：`seq_diff` 已經處理「比大小」，但 `seen` 位元表 22 分鐘後繞回來會把新包當 dup。做法：每一拍把「正對面」
  （`expected + 32768`）那個 bit 清掉；拍子連續，所以一圈回來那些 bit 早就是 0。`send --seq0 65500` 可以從快繞回的地方開始送，親眼看它繞過去。
- **BYE 被亂序超車**：BYE 之後才到的更後面的包 → 把結尾往後推；BYE 的 seq 太離譜（比看過最大的多超過 cap）就不信它。
- **`--bind <ip>`**：只收某張網卡（規格的參數）；第一包的等待上限 = max(10 × timeout, 30 s)。
- **結束碼**：0 正常；1 網路或裝置錯誤（`udp_open` 失敗、`udp_recv` 回 −1）；2 參數或檔案格式錯誤。前幾步已經這樣做，這裡整理成一致。

**程式碼**（`diff ../step4_counters/step4_counters.c step5_robust.c`）

```c
static void seen_clear(jb_t *jb, uint16_t seq) { jb->seen[seq >> 3] &= (uint8_t)~(1u << (seq & 7)); }
static void jb_reset(jb_t *jb) {                       /* 新的串流：不能 memset 整個（cap、plc、計數器要留） */
    for (int i = 0; i < RING_SLOTS; i++) jb->slot[i].valid = 0;
    memset(jb->seen, 0, sizeof(jb->seen));
    jb->count = 0;  jb->started = 0;  jb->have_first = jb->have_highest = jb->have_end = 0;  jb->trailing_concealed = 0;
}
/* jb_tick 最後 */
seen_clear(jb, (uint16_t)(jb->expected + 32768));      /* 繞回的準備：每一拍清掉對面那一個 bit */
/* cmd_recv 收到一包 */
if (!parse_header(buf, k, &h)) { n_bad++; if (n_bad <= 3) printf("壞封包 %d bytes ...\n", k); }
else if (h.type == T_AUDIO) {
    if (have_ssrc && h.ssrc != ssrc) { printf("ssrc 變了：重置 buffer\n"); jb_reset(&jb); have_bye = 0; }
    ssrc = h.ssrc; have_ssrc = 1;
    ...
} else if (!jb.have_end && (!have_ssrc || h.ssrc == ssrc)) { /* BYE：同一個 ssrc 的才算 */ }
```

**怎麼測**

| macOS / Linux | Windows PowerShell |
|---|---|
| `python3 ../../tests/run_mock_tests.py "python3 ../../python_ref/voicelink.py send" "./step5_robust recv" $W --only 1,2,3,4,5,6,7` | `python ..\..\tests\run_mock_tests.py "python ..\..\python_ref\voicelink.py send" ".\step5_robust.exe recv" $W --only 1,2,3,4,5,6,7` |
| `python3 ../../tests/run_mock_tests.py "./step5_robust send" "python3 ../../python_ref/voicelink.py recv" $W --only 1,2,3,4,5,6,7` | `python ..\..\tests\run_mock_tests.py ".\step5_robust.exe send" "python ..\..\python_ref\voicelink.py recv" $W --only 1,2,3,4,5,6,7` |
| 繞回：`./step5_robust recv 5004 w.wav` ＋ `./step5_robust send 127.0.0.1 5004 $W --seq0 65500` | `.\step5_robust.exe recv 5004 w.wav` ＋ `.\step5_robust.exe send 127.0.0.1 5004 $W --seq0 65500` |
| 重啟：recv 開著，`send` 跑兩次（第二次 ssrc 不同） | 同左 |

**看到什麼才算對**

```
[PASS] 4 dup      225082 bytes 逐 byte 相同；dup=38
[PASS] 6 garbage  225082 bytes 逐 byte 相同；received=352（壞封包沒有被算進去）
summary: 7/7 passed
```

繞回那次：`第一包到了（seq=65500）`、`BYE（seq=316 ...）：結尾 = seq 316`、STATS `lost=0 concealed=0`，`w.wav` 逐 byte相同
（用測試腳本的 `same_pcm` 或 Python 版 `--probe` 看）。重啟那次：`ssrc 從 0x... 變成 0x...：重置 buffer`，第二段聲音接在第一段後面播。

**常見錯誤**

- `jb_reset` 用 `memset(jb, 0, sizeof *jb)` → cap 變 0，之後每一包都 overflow。
- 壞封包也更新 `last_rx` → 有人一直丟垃圾時 `--timeout` 永遠不會到。只有合法封包才算「有活動」。
- 位元表沒清對面那一格 → 跑超過 22 分鐘的 live 通話後突然全部變 dup。

---
