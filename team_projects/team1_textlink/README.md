# Team 1｜TextLink：文字與封包

- 期間：9/21－10/12｜繳交截止：**10/11（日）18:00**｜評測：10/12（三節課內）
- 團隊 repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、
  `docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`
- 繳交：登錄 repo URL＋完整 commit SHA

## 範圍

完成 UTF-8 文字封裝／解析、長度欄位、半包／合併資料處理與
錯誤輸入測試；以課程提供的 TCP 傳輸框架完成兩端文字交換。

## 最低驗收

- 中文訊息 round-trip
- 長度檢查；切段輸入（半包／黏包）仍可還原
- 不越界（buffer 邊界安全）
- Huffman 僅作小型壓縮比較實驗（計入 codebook 成本），
  不要求串入即時聊天主路徑

## baseline/

`chat.c`＋`Makefile`：課程 TCP／UDP 聊天教學範例，作為 TCP 傳輸
框架的概念起點（正式官方 baseline 與固定 API 於開題時發布取代）。
延伸提示可參考 [../../docs/tutorials/nettcpudp_homework.md](../../docs/tutorials/nettcpudp_homework.md)
作業 1、3（暱稱時間戳記、長度前綴切包）——與本輪範圍直接相關。
