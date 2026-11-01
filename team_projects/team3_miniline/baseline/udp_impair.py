#!/usr/bin/env python3
"""udp_impair.py（Team 3）：搗蛋的 UDP 中繼，和 Team 2 的是同一支程式。
本檔只是轉呼叫 ../../team2_voicelink/baseline/udp_impair.py（參數完全相同），讓你在這個資料夾也能打同樣的指令：
  python udp_impair.py <listen_port> <fwd_ip> <fwd_port> [--loss p] [--jitter ms] [--reorder p] [--dup p] [--seed n] [--bind ip] [--quiet]
"""
import os
import runpy
import sys

TARGET = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "team2_voicelink", "baseline", "udp_impair.py"))
sys.argv[0] = TARGET
runpy.run_path(TARGET, run_name="__main__")
