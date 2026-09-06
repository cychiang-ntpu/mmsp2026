#!/usr/bin/env bash
set -euo pipefail

mkdir -p out/wav

BASE_RAW="https://raw.githubusercontent.com/cychiang-ntpu/mini-project-5-spectrogram/main"

download_one () {
  local out_name="$1"
  shift
  local urls=("$@")

  if [[ -f "out/wav/${out_name}" ]]; then
    echo "[fetch] out/wav/${out_name} already exists."
    return 0
  fi

  for u in "${urls[@]}"; do
    echo "[fetch] trying: $u"
    if curl -L --fail -o "out/wav/${out_name}" "$u" ; then
      echo "[fetch] OK -> out/wav/${out_name}"
      return 0
    fi
  done

  echo "[fetch] ERROR: cannot download ${out_name}" >&2
  return 1
}

#  expected filename
download_one "aeueo-16kHz.wav" \
  "${BASE_RAW}/aeueo-16kHz.wav"

# try both 
download_one "aeueo-8kHz.wav" \
  "${BASE_RAW}/aeueo-8kHz.wav" \
  "${BASE_RAW}/aeueo-8Hz.wav"
