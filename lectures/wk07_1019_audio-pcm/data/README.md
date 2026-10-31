# 第 7 週的素材

| 檔案 | 格式 | 用途 |
|---|---|---|
| `aeueo-16kHz.wav` | 16 kHz、16-bit、單聲道 PCM，7.03 秒，112,541 個 sample（351 個 20 ms frame） | 1.3 的 frame 與 F₀、1.6 的 spectrogram、`stft_steps.html` 內建、Team 2 自動測試的預設輸入 |
| `speech_osr_16k.wav` | 16 kHz、16-bit、單聲道 PCM，33.62 秒，537,970 個 sample（1681 個 20 ms frame） | Team 2 mock 模式的測試音檔（句子） |

## 來源與授權

- **aeueo-16kHz.wav**：本課程 Mini-Project 5 的測試音檔（授課教師錄製的五個母音 a-e-u-e-o，四種聲調），
  2025 年起隨 MP5 題目發給同學；課程教材。
- **speech_osr_16k.wav**：Open Speech Repository（<https://www.voiptroubleshooter.com/open_speech/>）American English 的
  `OSR_us_000_0010_8k.wav`（即第 3 週的 `speech_osr_8k.wav`），用 ffmpeg 升取樣：
  `ffmpeg -i speech_osr_8k.wav -ar 16000 -ac 1 -sample_fmt s16 speech_osr_16k.wav`。
  **注意**：原始錄音是 8 kHz，升取樣後頻寬仍是 0–4 kHz（spectrogram 的 4–8 kHz 會是空的）；它的用途是 Team 2 的傳輸測試，不是頻譜示範。
  網站的使用聲明（2026/9/21 查閱）："The material on this site is freely available for use in VoIP testing, research, development,
  marketing and any other reasonable application. The material may be copied, downloaded, broadcast, modified, incorporated into web
  sites or test equipment. We do require that you identify the source of the speech materials as 'Open Speech Repository'."
