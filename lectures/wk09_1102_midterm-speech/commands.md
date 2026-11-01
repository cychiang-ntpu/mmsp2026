# 第 9 週｜上課跟著打的指令（一頁版）

左欄 macOS／Linux／WSL，右欄 Windows PowerShell（`python3`→`python`）。從本週資料夾出發：`cd lectures/wk09_1102_midterm-speech`。

| 做什麼（講義章節） | macOS / Linux / WSL | Windows PowerShell |
|---|---|---|
| 更新 | `cd mmsp2026 && git pull && cd lectures/wk09_1102_midterm-speech` | `cd mmsp2026; git pull; cd lectures\wk09_1102_midterm-speech` |
| μ-law vs 線性 8-bit：弦波（2.2） | `python3 examples/mulaw_demo.py` | `python examples\mulaw_demo.py` |
| 同上：真實語音 | `python3 examples/mulaw_demo.py ../wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `python examples\mulaw_demo.py ..\wk07_1019_audio-pcm\data\speech_osr_16k.wav` |
| 預測編碼的 prediction gain 與 4-bit DPCM（2.3） | `python3 examples/dpcm_demo.py ../wk07_1019_audio-pcm/data/speech_osr_16k.wav` | `python examples\dpcm_demo.py ..\wk07_1019_audio-pcm\data\speech_osr_16k.wav` |
| 回顧 F₀ 與 frame（3.1） | `python3 ../wk07_1019_audio-pcm/examples/frame_demo.py ../wk07_1019_audio-pcm/data/aeueo-16kHz.wav 1.0` | `python ..\wk07_1019_audio-pcm\examplesrame_demo.py ..\wk07_1019_audio-pcm\dataeueo-16kHz.wav 1.0` |
| jitter buffer 動畫（3.3） | `open ../wk07_1019_audio-pcm/slides/jitter_buffer.html`（Linux：`xdg-open`） | `start ..\wk07_1019_audio-pcm\slides\jitter_buffer.html` |

Team 2 工作坊的指令（baseline、對手端、自動驗收、buffer 掃描、live）在 [lab.md](lab.md)。
