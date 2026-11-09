# 公告稿：Team 2 從 11/9 到 11/23 的安排（老師貼 Slack 用）

```
📢 Team 2（VoiceLink）11/9–11/23 的完整安排，請每位同學看完。

所有資訊都在這一頁，從這裡開始：
https://github.com/cychiang-ntpu/mmsp2026/blob/main/team_projects/team2_voicelink/README.md

■ 作業是什麼
tutorial 的九個模組（m1–m9）＋六步（step1–step6）做出一支能兩台電腦通話的 voicelink（raw PCM）。
真正要寫的是「加壓縮」：mulaw_system/（μ-law）、adpcm_huffman_system/（ADPCM 或／和 Huffman），兩個資料夾都給了框架，程式裡 ★ 標出要動手的地方。

■ 怎麼繳交
把 team_projects/team2_voicelink/ 整個複製成 team repo（mmsp2026-team2-gXX），資料夾名稱不要改。
repo 裡的 GitHub Actions（team2-check）每次 push 會自動檢查 m1–m9、step1–6：綠勾就對了。

■ 時程
11/9（一）不上課、助教不到場（我出國）。各組自己做 m1–m9，每個模組各自一個 commit。
　→ 11/11（三）18:00 前 m1–m9 全部 push。
11/16（一）照常到電腦教室，點名，助教現場主持：做 step4–6。
　→ 當天 16:00 下課前 step1–6 全部 push（含 step6_live）。第三節開始 mulaw_system。
　→ 助教會帶實驗室的 USB 麥克風，借給同學在課堂上使用；也可以自己帶 USB 麥克風，或帶自己的筆記型電腦、耳機、麥克風，方便現場實作與驗證通話。
11/22（日）18:00 登錄 repo URL ＋ commit SHA（mulaw_system 過測試 8；adpcm_huffman_system 做 ADPCM／Huffman；slides.pdf 含報告三題）。
11/23（一）demo 與口試日，流程同 Team 1：P 報告 4 分鐘 → D 兩台電腦現場通話 3 分鐘 → 問答 2 分鐘；第三節分組口試（以組為單位，照已公告的分組，全組一起回答）。帶兩台筆電、耳機、手機熱點。

■ 評分（加總超過 100 以 100 計）
40　m1–m9、step1–6 共 15 項都 push 到 team repo（各自一個資料夾、至少一個 commit、make 零警告）；缺一項扣 3。
最多 40　11/23 demo，一項一項加：兩台以 raw waveform 通話 +10、完成 μ-law +10、完成 ADPCM +10、完成 Huffman +10。
最多 30　助教程式碼實質審查（11/23 後一週）：正確性與互通 10、程式品質 10、個人貢獻 10（看 git log，沒有自己的 C commit 就是 0）。

問題貼 #team2-questions，助教線上回；11/16 現場可以問。
```
