## m2_udp_send.c／m2_udp_recv.c：一次 sendto = 一個 datagram

[← 教學總覽](../TUTORIAL.md)｜上一個：[m1 讀寫 WAV](../m1_wav/README.md)｜下一個：[m3 20 ms 時鐘](../m3_clock/README.md)

### 在這個資料夾

| macOS / Linux | Windows PowerShell |
|---|---|
| `cd team_projects/team2_voicelink/tutorial/m2_udp` | `cd team_projects\team2_voicelink\tutorial\m2_udp` |
| `make`（編出 `./m2_udp_send`（Windows `.\m2_udp_send.exe`） `./m2_udp_recv`（Windows `.\m2_udp_recv.exe`）） | `mingw32-make` |
| `W=../../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav` | `$W = "..\..\..\..\lectures\wk07_1019_audio-pcm\data\aeueo-16kHz.wav"` |

程式：[`m2_udp_send.c`](m2_udp_send.c)、[`m2_udp_recv.c`](m2_udp_recv.c)（每一行都有註解，先讀檔頭的「用法／看到什麼／試著改」）。


### 這個模組的目的

- **做什麼**：兩個程式用 UDP 互寄一行字：一次 `sendto` 就是一個 datagram，對方一次 `recvfrom` 拿到整張。
- **為什麼要先做它**：Team 2 不用 TCP、改用 UDP，整個作業的「掉包、亂序、重複」都從這裡來。先親手看到「一張明信片就是一包、不會半行、不會黏行、寄丟了送端也不知道」，step1 的 send／recv 迴圈就只是把這兩支程式的「一行字」換成「640 bytes 的聲音」。
- **在 Part B 的哪裡用到**：step1 的 `cmd_send`／`cmd_recv`；`udp_recv` 的逾時參數在 step3 變成播放時鐘。
- **做完要能回答**：UDP 和 TCP 差在哪三件事？送到沒人在聽的位址，`udp_send` 會報錯嗎？

**看什麼**：UDP 沒有 connect／accept。送端 `udp_open(&u, NULL, 0)`（隨便一個 port）＋ `udp_set_peer`（記下寄給誰）＋ `udp_send`；
收端 `udp_open(&u, NULL, port)` ＋ `udp_recv`（有逾時）。打一行寄一張明信片，對面一次收一整張——不會半行、不會黏行（TCP 才會）。

| macOS / Linux | Windows PowerShell |
|---|---|
| 視窗 1：`./m2_udp_recv 5004` | `.\m2_udp_recv.exe 5004` |
| 視窗 2：`./m2_udp_send 127.0.0.1 5004`，然後打字 | `.\m2_udp_send.exe 127.0.0.1 5004` |

```
12 bytes from 127.0.0.1:54372: hello 你好
11 bytes from 127.0.0.1:54372: second line
收到空字串（對方結束了）
```

**改一個地方**：收端的 `buf` 改成 8 bytes，寄一行長的——多出來的被作業系統直接丟掉（一個 datagram 一次收完）。
把送端的 ip 改成沒人在聽的位址：`udp_send` 照樣回成功（UDP 不知道對方在不在，這就是為什麼 Team 2 要自己送 BYE）。
