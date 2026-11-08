## 第 2 步｜step2_header.c：每包加 12 bytes 的 header，親眼看見 seq 跳號

[← 教學總覽](../TUTORIAL.md)｜上一個：[第 1 步 raw](../step1_raw/README.md)｜下一個：[第 3 步 jitter buffer](../step3_buffer/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/step2_header` | `cd team_projects\team2_voicelink\tutorial\step2_header` |
| `make`（編出 `./step2_header`（Windows `.\step2_header.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`step2_header.c`](step2_header.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


**目標**：送端每包前面加 `magic/type/seq/ts/ssrc`，最後送 3 次 BYE；收端解析、驗證、印出每包的 seq／ts，但還是照到達順序寫。

**用到的模組**：m6（`pack_header`／`parse_header` 就是它）＋第 1 步的 m1、m2、m3。

**要加的東西（為什麼）**

- **封包格式**（規格「封包格式」，RTP（Real-time Transport Protocol，即時傳輸協定） 的精簡版）：

  ```
  offset  0     1     2  3   4  5  6  7   8  9 10 11   12 ....................... 651
         +-----+-----+------+------------+------------+--------------------------------+
         |0x56 |type |  seq |     ts     |    ssrc    |   PCM 640 bytes（320 × int16）  |
         +-----+-----+------+------------+------------+--------------------------------+
          magic 0x01  16-bit   32-bit       32-bit      little-endian（WAV 的原樣）
                AUDIO  每包+1   每包+320     本次隨機
                0x02
                BYE  （BYE 只有 12 bytes，ts = 真正的 sample 總數）
  ```
  `seq` 回答「有沒有掉／亂序／重複」，`ts` 回答「什麼時候該播」，`ssrc` 回答「這是哪一次的串流」（對方重啟就換）。
- **big-endian**：網路慣例高位 byte 在前，CPU 卻多半是 little-endian，而且 struct 有對齊空隙——所以**不能把 struct 直接 memcpy 進封包**，
  要一個 byte 一個 byte 用位移放：`put_u16(p+2, seq)` 就是 `p[2] = seq >> 8; p[3] = seq & 0xff`（`udp.h` 已經寫好）。
- **先檢查長度再讀欄位**：長度不到 12 就去讀 `p[11]` 是讀到陣列外面（壞封包測試就是在測這個）。
- **BYE × 3**：UDP 沒有連線，「結束」要自己說；可能掉，所以說三次。BYE 的 `ts` 放真正的 sample 總數，收端用它去掉最後一包補的 0
  （這是 Python 版的約定，第 3 步會用到）。
- **STATS role=send**：規格要求的一行，印到 stderr（測試 8 要用 `bytes`）。

**程式碼**（`diff ../step1_raw/step1_raw.c step2_header.c`）

```c
typedef struct { uint8_t type; uint16_t seq; uint32_t ts; uint32_t ssrc; } hdr_t;   /* 解析好的 header */

static void pack_header(uint8_t *p, uint8_t type, uint16_t seq, uint32_t ts, uint32_t ssrc) {
    p[0] = MAGIC;  p[1] = type;
    put_u16(p + 2, seq);  put_u32(p + 4, ts);  put_u32(p + 8, ssrc);     /* big-endian，一個 byte 一個 byte 放 */
}
static int parse_header(const uint8_t *p, int len, hdr_t *h) {
    if (len < HDR_LEN || p[0] != MAGIC) return 0;                        /* 先看長度與 magic */
    h->type = p[1];  h->seq = get_u16(p + 2);  h->ts = get_u32(p + 4);  h->ssrc = get_u32(p + 8);
    if (h->type == T_AUDIO && len == PKT_AUDIO) return 1;                /* 長度要和 type 配：652 */
    if (h->type == T_BYE && len == HDR_LEN) return 1;                    /* 12 */
    return 0;
}
/* send：header 在前 12 bytes，PCM 從 offset 12 開始；seq 是 uint16_t，65535 之後自動繞回 0 */
pack_header(pkt, T_AUDIO, seq, (uint32_t)(k * FRAME_SAMPLES), ssrc);
pcm_to_bytes(w.pcm + start, n, pkt + HDR_LEN);
seq++;
/* 送完：3 次 BYE，ts = 真正的 sample 數 */
for (int i = 0; i < BYE_REPEAT; i++) { pack_header(bye, T_BYE, seq, (uint32_t)w.n_samples, ssrc); udp_send(...); seq++; }
/* recv */
if (!parse_header(buf, k, &h)) { n_bad++; continue; }                    /* 不是我們的封包：丟掉 */
if (h.type == T_BYE) break;
printf("seq=%u ts=%u ssrc=0x%08X\n", h.seq, h.ts, h.ssrc);
bytes_to_pcm(buf + HDR_LEN, FRAME_SAMPLES, out + out_len);               /* 還是到達順序！ */
```

**怎麼測**（三個視窗；這一步就開始和 Python 版互測）

| macOS / Linux | Windows PowerShell |
|---|---|
| `./step2_header recv 5004 out2.wav` | `.\step2_header.exe recv 5004 out2.wav` |
| `python3 ../../baseline/udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` | `python ..\..\baseline\udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --seed 1` |
| `./step2_header send 127.0.0.1 5005 $W` | `.\step2_header.exe send 127.0.0.1 5005 $W` |
| 反過來：`python3 ../../python_ref/voicelink.py recv 5004 out2b.wav --probe` 配 `./step2_header send 127.0.0.1 5004 $W` | `python ..\..\python_ref\voicelink.py recv 5004 out2b.wav --probe` 配 `.\step2_header.exe send 127.0.0.1 5004 $W` |

**看到什麼才算對**

```
seq=13 ts=4160 ssrc=0x152D469B
seq=15 ts=4800 ssrc=0x152D469B       ← 14 不見了（--loss 0.05）：現在「看得到」掉了哪一包
seq=16 ts=5120 ssrc=0x152D469B
...
BYE  seq=352 ts=112541（真正的 sample 數）
收到 335 包（丟掉 0 個壞封包），107200 samples，寫到 out2.wav
STATS role=send codec=pcm frames=352 bytes=229504 duration_ms=7040 pace=20      ← 送端（stderr）
```

Python 版收我們的封包時要印 `STATS role=recv ... frames_out=352 received=352 lost=0 ... concealed=0`，而且 `out2b.wav` 逐 byte 相同
（`--probe` 會把每一包的命運印出來，看得到它解析我們的 header）。這代表**封包格式對了**——11/9 討論會的「格式互測」就是測這個。

**常見錯誤**

- `put_u16`／`get_u16` 用錯邊（送端用 big-endian、收端卻 `buf[2] | buf[3] << 8`）→ Python 版印 `seq=256`、`seq=512`……跳 256 的倍數。
- 忘了 `pkt + HDR_LEN`：PCM 從 offset 0 開始寫，蓋掉 header → 對方說全是壞封包。
- `sizeof(pkt)` 還是 640 → 長度不是 652，對方丟掉。

---
