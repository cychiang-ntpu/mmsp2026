# Team 2｜VoiceLink：即時語音管線

- 期間：10/19－11/9｜繳交截止：**11/8（日）18:00**｜評測：11/9（三節課內）
- 團隊 repo 必備：`src/`、`include/`、`tests/`、建置檔、`README.md`、
  `docs/interface.md`、`TEAM_LOG.md`、`CONTRIBUTIONS.md`、`AI_USAGE.md`、`slides.pdf`
- 繳交：登錄 repo URL＋完整 commit SHA

## 範圍

在官方文字 baseline 與音訊／UDP wrapper 上，完成：

- 20 ms PCM framing（固定 16 kHz、16-bit、單聲道；每 frame 320 samples＝640 bytes）
- sequence／timestamp
- 簡化 jitter buffer
- 遺失補償（依規格補零或重複前一 frame）

## 最低驗收

- 指定 WAV 經封包化後可重建
- 遇有限亂序或遺失不永久等待
- 量測 buffer 延遲
- 指定設備上展示雙向語音（使用耳機；不要求回音消除）

## baseline/

開題時放入官方文字 baseline 與音訊／UDP wrapper。
前導概念可參考 [../../docs/tutorials/nettcpudp_homework.md](../../docs/tutorials/nettcpudp_homework.md)
作業 5（UDP 傳聲音、掉封包觀察）。
