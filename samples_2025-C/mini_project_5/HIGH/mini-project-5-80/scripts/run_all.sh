#!/usr/bin/env bash
set -euo pipefail

mkdir -p out/wav out/txt out/pdf

echo "== 1) Generate s-8kHz.wav / s-16kHz.wav =="
./signal_gen
mv -f s-8kHz.wav  out/wav/s-8kHz.wav
mv -f s-16kHz.wav out/wav/s-16kHz.wav

echo "== 2) Fetch aeueo wavs =="
bash scripts/fetch_aeueo.sh

# WAV list (input) + base name (for output naming)
# HackMD 建議 txt 命名：s-8k.{Set1~4}.txt, s-16k.{Set1~4}.txt, aeueo-8kHz.{Set1~4}.txt, aeueo-16kHz.{Set1~4}.txt :contentReference[oaicite:2]{index=2}
declare -a WAVS=(
  "out/wav/s-8kHz.wav:s-8k"
  "out/wav/s-16kHz.wav:s-16k"
  "out/wav/aeueo-8kHz.wav:aeueo-8kHz"
  "out/wav/aeueo-16kHz.wav:aeueo-16kHz"
)

# Four settings (w_size(ms), w_type, dft_size(ms), f_itv(ms)) :contentReference[oaicite:3]{index=3}
declare -a SETS=(
  "32 rectangular 32 10:Set1"
  "32 hamming     32 10:Set2"
  "30 rectangular 32 10:Set3"
  "30 hamming     32 10:Set4"
)

echo "== 3) Generate 16 spectrogram txt =="
for pair in "${WAVS[@]}"; do
  IFS=":" read -r wav base <<< "$pair"
  for s in "${SETS[@]}"; do
    IFS=":" read -r params setname <<< "$s"
    txt="out/txt/${base}.${setname}.txt"
    echo "[spec] $wav -> $txt  ($params)"
    # shellcheck disable=SC2086
    ./spectrogram $params "$wav" "$txt"
  done
done

echo "== 4) Generate 16 pdf (waveform + spectrogram) =="
python3 -m pip -q install --disable-pip-version-check numpy matplotlib >/dev/null || true

for pair in "${WAVS[@]}"; do
  IFS=":" read -r wav base <<< "$pair"
  for s in "${SETS[@]}"; do
    IFS=":" read -r _ setname <<< "$s"
    txt="out/txt/${base}.${setname}.txt"
    pdf="out/pdf/${base}.${setname}.pdf"
    echo "[pdf] $wav + $txt -> $pdf"
    python spectshow.py "$wav" "$txt" "$pdf"
  done
done

echo "DONE. Outputs:"
echo "  out/wav  (4 wav)"
echo "  out/txt  (16 txt)"
echo "  out/pdf  (16 pdf)"
