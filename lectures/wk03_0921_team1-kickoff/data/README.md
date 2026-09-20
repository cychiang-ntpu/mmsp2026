# data/ — 第 3 週用的真實檔案

| 檔案 | 規格 | 用途 |
|---|---|---|
| `speech_osr_8k.wav` | 8 kHz、16-bit、單聲道 PCM，33.6 秒，538,014 bytes | hexdump 讀檔頭（講義 2.6）、`wav_info.py` 的 sample histogram（2.7）、檢視器內建的語音（前 4.5 秒） |
| `music_sousa_44k.wav` | 44.1 kHz、16-bit、單聲道 PCM，6 秒，529,244 bytes | 檢視器內建的音樂：降取樣與 aliasing 要寬頻的聲音才聽得出來 |
| `earth_256.bmp` | 256×256、24-bit 未壓縮 BMP（由下往上存），196,662 bytes | 檢視器內建的圖像；也可以用 hex 工具直接看 54 bytes 的檔頭 |

`../slides/samples.js` 是這三個檔案的 base64，由 `../examples/make_samples_js.py` 產生；換了這裡的檔案之後要重跑一次。

## 來源與授權

- **speech_osr_8k.wav**：Open Speech Repository（<https://www.voiptroubleshooter.com/open_speech/>），American English 的 `OSR_us_000_0010_8k.wav`，未經修改。
  網站的使用聲明（2026/9/21 查閱）："The material on this site is freely available for use in VoIP testing, research, development, marketing and any other
  reasonable application. The material may be copied, downloaded, broadcast, modified, incorporated into web sites or test equipment.
  We do require that you identify the source of the speech materials as 'Open Speech Repository'."
- **music_sousa_44k.wav**：John Philip Sousa 的進行曲〈Comrades of the Legion〉（1920 年作，作曲者 1932 年逝世），
  由 "The President's Own" United States Marine Band 演奏與錄音。來源檔：Wikimedia Commons
  [File:John Philip Sousa - Comrades of the Legion.ogg](https://commons.wikimedia.org/wiki/File:John_Philip_Sousa_-_Comrades_of_the_Legion.ogg)，標示為 Public domain
  （美國聯邦政府機構執行公務所產生的作品）。本檔截取第 20–26 秒、混成單聲道、頭尾各加 0.05 秒淡入淡出，存成 44.1 kHz／16-bit PCM。
- **earth_256.bmp**：The Blue Marble，Apollo 17 任務組員於 1972/12/7 拍攝，NASA 影像編號 AS17-148-22727。來源檔：Wikimedia Commons
  [File:The Earth seen from Apollo 17.jpg](https://commons.wikimedia.org/wiki/File:The_Earth_seen_from_Apollo_17.jpg)，標示為 Public domain（NASA 的作品）。
  本檔由其縮圖縮成 256×256，存成 24-bit BMP。

轉檔用的指令（ffmpeg）：

```
ffmpeg -ss 20 -t 6 -i sousa.ogg -ac 1 -ar 44100 -af "afade=t=in:d=0.05,afade=t=out:st=5.95:d=0.05" -c:a pcm_s16le -map_metadata -1 music_sousa_44k.wav
ffmpeg -i earth.jpg -vf scale=256:256:flags=lanczos -pix_fmt bgr24 earth_256.bmp
```
