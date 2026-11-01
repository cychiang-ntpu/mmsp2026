# 第 9 週｜上課跟著打的指令（一頁版）

左欄 macOS／Linux／WSL，右欄 Windows PowerShell（`python3`→`python`）。從本週資料夾出發：`cd lectures/wk09_1102_midterm-speech`。

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 更新 | `cd mmsp2026 && git pull && cd lectures/wk09_1102_midterm-speech` | `cd mmsp2026; git pull; cd lectures\wk09_1102_midterm-speech` |
| A/D 四步驟動畫（2.1） | `open ../wk03_0921_team1-kickoff/slides/a2d_steps.html`（Linux：`xdg-open`） | `start ..\wk03_0921_team1-kickoff\slides\a2d_steps.html` |
| aliasing：3 kHz 與 5 kHz（2.3） | `python3 ../wk03_0921_team1-kickoff/examples/alias_demo.py` | `python ..\wk03_0921_team1-kickoff\examples\alias_demo.py` |
| 量化與 SQNR 的表（2.4） | `python3 ../wk03_0921_team1-kickoff/examples/quantize_demo.py` | `python ..\wk03_0921_team1-kickoff\examples\quantize_demo.py` |
| 振幅 0.1 時的 SQNR（回家作業 3） | `python3 ../wk03_0921_team1-kickoff/examples/quantize_demo.py 0.1` | `python ..\wk03_0921_team1-kickoff\examples\quantize_demo.py 0.1` |
| 播放、檔頭、換取樣率（2.5） | `open ../wk03_0921_team1-kickoff/slides/wav_bmp_viewer.html` | `start ..\wk03_0921_team1-kickoff\slides\wav_bmp_viewer.html` |
| 波形量週期與 F₀（3.3） | `python3 ../wk07_1019_audio-pcm/examples/frame_demo.py ../wk07_1019_audio-pcm/data/aeueo-16kHz.wav 1.0` | `python ..\wk07_1019_audio-pcm\examples\frame_demo.py ..\wk07_1019_audio-pcm\data\aeueo-16kHz.wav 1.0` |
| 氣音的地方沒有週期 | 同上，最後改 `0.2` | 同上，最後改 `0.2` |
| 三種窗與 leakage（3.5） | `python3 ../wk07_1019_audio-pcm/examples/window_demo.py` | `python ..\wk07_1019_audio-pcm\examples\window_demo.py` |
| STFT 四步驟動畫（3.5） | `open ../wk07_1019_audio-pcm/slides/stft_steps.html` | `start ..\wk07_1019_audio-pcm\slides\stft_steps.html` |
| 文字版 spectrogram（3.5） | `python3 ../wk07_1019_audio-pcm/examples/stft_demo.py ../wk07_1019_audio-pcm/data/aeueo-16kHz.wav --ascii` | `python ..\wk07_1019_audio-pcm\examples\stft_demo.py ..\wk07_1019_audio-pcm\data\aeueo-16kHz.wav --ascii` |
| μ-law（課後，audio_coding.md） | `python3 examples/mulaw_demo.py` | `python examples\mulaw_demo.py` |

Team 2 工作坊與 tutorial 的指令在 [lab.md](lab.md) 與 [tutorial/TUTORIAL.md](../../team_projects/team2_voicelink/tutorial/TUTORIAL.md)。
