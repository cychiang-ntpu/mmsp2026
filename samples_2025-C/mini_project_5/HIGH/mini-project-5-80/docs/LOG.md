# 開發日誌 (Mini-Project 5: Spectrogram/STFT)(411186008)

## 2025-12-20
### 今日目標
- 釐清專案整體流程、產出物與 GitHub Actions 方向
- 先把 C 端 pipeline 跑通：產生 wav、輸出 16 份 spectrogram txt
- 建立輸出驗證方法（frames/bins/第一行數據等）

---

### 完成事項
- [x] 整理專案流程表（wav/txt/pdf + workflow artifacts）
- [x] 建立 Makefile + scripts/run_all.sh + scripts/fetch_aeueo.sh + workflow.yml 的一鍵流程概念
- [x] 於 Git Bash 環境成功跑 `make all`，完成：
  - out/wav：4 個 wav
  - out/txt：16 個 txt

---

### 今日遇到的問題與解法

#### 1) 專案流程
**現象**
- 想要一個明確的製作流程與檢查點

**解法**
- 先建立完整流程：環境 → signal_gen → 4 wav → spectrogram 16 txt → spectshow 16 pdf → workflow artifacts → README 討論

---

#### 2) 想驗證輸出是否正確
**建立的驗證方法**
- 驗證 wav header/採樣率/長度（Python wave 讀取）
- 驗證 txt 的 frames 行數與 bins 欄數：
  - s-8k Set1：P=256, N=256, M=80 → frames=397, bins=129
- 用 peak bin 對照已知頻率（Δf=fs/N=31.25Hz）檢查 DFT 正確性：
  - 31.25Hz → k=1
  - 500Hz → k=16
  - 2000Hz → k=64

---

### 今日結果摘要
- pipeline 已成功產生：
  - out/wav：4 個 wav
  - out/txt：16 個 txt
- out/pdf 目前尚未產生（原因：spectshow.py 還沒完成）
- Set1(s-8kHz) 的參數/frames/bins 計算與 header 描述一致：
  - fs=8000, P=256 (32ms), N=256 (32ms), M=80 (10ms), frames=397, bins=129

---

### 待辦事項 (Next)
- [ ] 完成 spectshow.py，確保可批次輸出 16 個 pdf（waveform + spectrogram）
- [ ] README：補 Set1~Set4 比較、每 frame 乘加次數、心得與分工
- [ ] 整體運行速度偏慢，可以檢查一下

2025-12-21
###今日目標
完成整個 pipeline 最後一段：spectshow.py → 16 份 pdf
解決 Windows / Git Bash / Python 環境造成的繪圖失敗問題
確保 scripts/run_all.sh 在本機可完整跑完

###完成事項
 完成 spectshow.py（waveform + spectrogram 合併輸出為 pdf）
 成功在 Windows + Git Bash (MINGW64) 環境下產生：
out/pdf：16 個 pdf
 修正 scripts/run_all.sh，統一使用 python 呼叫繪圖程式
 完整跑通流程：
signal_gen → 4 wav
spectrogram → 16 txt
spectshow → 16 pdf

###今日結果摘要
專案 pipeline 已完全完成並驗證成功：
out/wav：4 個 wav
out/txt：16 個 txt
out/pdf：16 個 pdf
