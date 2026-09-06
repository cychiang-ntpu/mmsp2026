# Team 3｜MiniLINE：視訊與整合

- 期間：11/16－12/7｜繳交截止：**12/6（日）18:00**｜評測：12/7（三節課內）
- 團隊 repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、
  `docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`
- 繳交：登錄 repo URL＋完整 commit SHA

## 範圍

在官方文字／語音 baseline 與 camera／JPEG／display wrapper 上，完成：

- JPEG 分片／重組、frame ID
- frame timeout（缺片逾時丟棄）
- bounded queue（queue 上限）
- 簡化掛斷／重連狀態
- 整合文字、語音與視訊

## 最低驗收

- 320×240、目標 5 fps 的 MJPEG
- 缺片 frame 逾時丟棄
- 不因視訊壅塞永久阻塞音訊／文字
- 掛斷後可再連線
- 記錄實際 fps、遺失／丟棄與 buffer 指標（不以設備差異直接扣分）

## baseline/

開題時放入官方文字／語音 baseline 與 camera／JPEG／display wrapper。
Mock mode 以 WAV、JPEG 序列及固定測試軌跡評分；live mode 展示真實通訊。
範圍限受控 LAN、一對一；不要求 NAT 穿透、TLS、Opus、H.264。
