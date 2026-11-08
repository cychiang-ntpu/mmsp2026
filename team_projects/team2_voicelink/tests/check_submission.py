#!/usr/bin/env python3
"""check_submission.py：檢查 team repo 的 tutorial 繳交（規格「驗收與評分」①）——m1–m9 與 step1–step6 各自一個資料夾、
有 .c、make 零警告、該過的檢查有過。給 GitHub Actions（.github/workflows/team2-check.yml）跑，助教與同學也可以在本機跑。只用標準函式庫。

  python tests/check_submission.py [--root <team repo 根目錄>] [--wav <16k mono wav>] [--only m|step|systems] [--port N] [--deadline-report]

每一項印一行 [PASS]／[FAIL]／[SKIP]，最後印總表與「缺幾項、扣幾分」（15 項，缺一項扣 3，40 分封頂）。有任何 FAIL 結束碼 1。

檢查內容：
  m1–m9      資料夾存在、.c 存在、make 成功且沒有 "warning"；不需要互動的模組順便跑一次：
             m3（跑得完、有印誤差；數值只參考）、m6（印 OK）、m7（REPEAT 剛好 1 次）、m8（SQNR ≥ 30）、m9（SQNR ≥ 20）
  step1–6    資料夾存在、.c 存在、make 零警告；step3 跑 run_mock_tests --only 1,2,5、step4 --only 1,2,3,5,7、
             step5／step6 --only 1,2,3,4,5,6,7（自己對自己；CI 上 --buffer 200）
  systems    （--only systems 或預設都跑）mulaw_system／adpcm_huffman_system：make 零警告、voicelink devices 結束碼 0；
             有 mulaw 就跑 --only 8；有 adpcm 就跑 --only 9（看 voicelink.c 裡 CODECS 表有沒有那個名字）。這部分只報告、不計入「缺項」。
  --deadline-report  印每個資料夾最後一次 commit 的時間（git log），方便對照 11/11（m）與 11/16 16:00（step）的截止。
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

MODULES = {
    "m1_wav": ["m1_wav"], "m2_udp": ["m2_udp_send", "m2_udp_recv"], "m3_clock": ["m3_clock"],
    "m4_mic_to_wav": ["m4_mic_to_wav"], "m5_play_wav": ["m5_play_wav"], "m6_header": ["m6_header"],
    "m7_ringbuffer": ["m7_ringbuffer"], "m8_mulaw": ["m8_mulaw"], "m9_adpcm": ["m9_adpcm"],
}
STEPS = {
    "step1_raw": None, "step2_header": None, "step3_buffer": "1,2,5", "step4_counters": "1,2,3,5,7",
    "step5_robust": "1,2,3,4,5,6,7", "step6_live": "1,2,3,4,5,6,7",
}
SYSTEMS = {"mulaw_system": {"mulaw": "8"}, "adpcm_huffman_system": {"adpcm": "9"}}
EXE = ".exe" if os.name == "nt" else ""
MAKE = shutil.which("make") or shutil.which("mingw32-make") or "make"
PY = sys.executable


def run(cmd, cwd=None, timeout=600):
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
        return p.returncode, (p.stdout or "") + (p.stderr or "")
    except subprocess.TimeoutExpired:
        return 124, "timeout"
    except OSError as e:
        return 127, str(e)


def build(folder):
    """make -B；回傳 (ok, why)。zero-warning 是規格要求。"""
    rc, out = run([MAKE, "-B"], cwd=folder)
    if rc != 0:
        return False, "make 失敗：" + out.strip().splitlines()[-1][:120] if out.strip() else "make 失敗"
    if re.search(r"\bwarning\b", out, re.I):
        return False, "有編譯警告：" + next(l for l in out.splitlines() if re.search(r"warning", l, re.I))[:120]
    return True, "make 零警告"


def check_module(root, name, exes, wav):
    folder = os.path.join(root, "tutorial", name)
    if not os.path.isdir(folder):
        return False, "資料夾不存在"
    missing = [e + ".c" for e in exes if not os.path.exists(os.path.join(folder, e + ".c"))]
    if missing:
        return False, "缺 " + "、".join(missing)
    ok, why = build(folder)
    if not ok:
        return False, why
    # 不用互動的模組跑一次
    exe = os.path.join(folder, exes[0] + EXE)
    if name == "m3_clock":
        rc, out = run([exe], timeout=30)
        m = re.search(r"最大誤差\s*([\d.]+)\s*ms", out)
        # 只看跑得完、有印出誤差；數值不設門檻（CI 與忙碌的電腦常常 >5 ms，不該算同學的錯）
        return (rc == 0 and m is not None), why + f"；m3 跑完，最大誤差 {m.group(1) if m else '?'} ms（參考值，不計分）"
    if name == "m6_header":
        rc, out = run([exe], timeout=30)
        return (rc == 0 and "OK" in out), why + "；m6 印出 OK"
    if name == "m7_ringbuffer":
        rc, out = run([exe], timeout=30)
        n = len(re.findall(r"tick .* REPEAT", out))
        return (rc == 0 and n == 1), why + f"；m7 REPEAT {n} 次（要 1）"
    if name in ("m8_mulaw", "m9_adpcm") and wav:
        out_wav = os.path.join(folder, "_check_out.wav")
        rc, out = run([exe, wav, out_wav], timeout=120)
        m = re.search(r"SQNR\s*([\d.]+)\s*dB", out)
        thr = 30.0 if name == "m8_mulaw" else 20.0
        try:
            os.remove(out_wav)
        except OSError:
            pass
        return (rc == 0 and m is not None and float(m.group(1)) >= thr), why + f"；SQNR {m.group(1) if m else '?'} dB（≥ {thr:g}）"
    return True, why


def check_step(root, name, only, wav, port, buffer_ms):
    folder = os.path.join(root, "tutorial", name)
    if not os.path.isdir(folder):
        return False, "資料夾不存在"
    if not os.path.exists(os.path.join(folder, name + ".c")):
        return False, f"缺 {name}.c"
    ok, why = build(folder)
    if not ok or not only or not wav:
        return ok, why
    exe = os.path.join(folder, name + EXE)
    if name == "step3_buffer":
        buffer_ms = min(buffer_ms, 60)             # 第 3 步的環狀陣列只有 16 格（cap = 4 × buffer frames）：--buffer 最多 80
    rc, out = run([PY, os.path.join(root, "tests", "run_mock_tests.py"), f"{exe} send", f"{exe} recv", wav,
                   "--only", only, "--port", str(port), "--buffer", str(buffer_ms)], timeout=900)
    summ = next((l for l in out.splitlines() if l.startswith("summary:")), "（沒有 summary）")
    return rc == 0, why + f"；run_mock_tests --only {only}：{summ}"


def check_system(root, name, codecs, wav, port, buffer_ms):
    folder = os.path.join(root, name)
    if not os.path.isdir(folder):
        return False, "資料夾不存在"
    src = os.path.join(folder, "voicelink.c")
    if not os.path.exists(src):
        return False, "缺 voicelink.c"
    ok, why = build(folder)
    if not ok:
        return False, why
    exe = os.path.join(folder, "voicelink" + EXE)
    rc, out = run([exe, "devices"], timeout=30)
    if rc != 0:
        return False, why + "；voicelink devices 結束碼 %d" % rc
    text = open(src, encoding="utf-8", errors="replace").read()
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)             # 先拿掉註解：框架的 ★ 提示裡也寫著 { "mulaw", … }
    text = re.sub(r"//[^\n]*", "", text)
    notes = []
    all_ok = True
    for codec, test in codecs.items():
        if re.search(r'\{\s*"%s"' % codec, text) is None:
            notes.append(f"codec 表裡還沒有 {codec}（測試 {test} 跳過）")
            continue
        if not wav:
            continue
        rc, out = run([PY, os.path.join(root, "tests", "run_mock_tests.py"), f"{exe} send", f"{exe} recv", wav,
                       "--only", test, "--port", str(port), "--buffer", str(buffer_ms)], timeout=900)
        summ = next((l for l in out.splitlines() if l.startswith("summary:")), "（沒有 summary）")
        notes.append(f"{codec}：--only {test} {summ}")
        all_ok = all_ok and rc == 0
        port += 10
    return all_ok, why + "；" + "；".join(notes) if notes else why


def last_commit(root, rel):
    rc, out = run(["git", "log", "-1", "--format=%ci", "--", rel], cwd=root, timeout=30)
    return out.strip() if rc == 0 and out.strip() else "（沒有 commit）"


def main():
    ap = argparse.ArgumentParser(description="Team 2 tutorial 繳交檢查")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))), help="team repo 根目錄（預設：這個腳本的上兩層）")
    ap.add_argument("--wav", default=None, help="16 kHz 單聲道 16-bit WAV（m8／m9／step 測試用）；沒有就跳過那些測試")
    ap.add_argument("--only", choices=["m", "step", "systems"], default=None)
    ap.add_argument("--port", type=int, default=5604)
    ap.add_argument("--buffer", type=int, default=200, help="run_mock_tests 的 --buffer（CI 的機器慢，預設 200）")
    ap.add_argument("--deadline-report", action="store_true")
    cfg = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    root = os.path.abspath(cfg.root)
    wav = os.path.abspath(cfg.wav) if cfg.wav else None
    if cfg.wav and not os.path.exists(wav):
        print(f"錯誤: 找不到 {wav}", file=sys.stderr)
        return 2
    print(f"check_submission：root={root} wav={wav or '（無；跳過需要 WAV 的測試）'} make={MAKE}")

    results = {}
    port = cfg.port
    if cfg.only in (None, "m"):
        for name, exes in MODULES.items():
            ok, why = check_module(root, name, exes, wav)
            results[name] = ok
            print(f"[{'PASS' if ok else 'FAIL'}] {name:22s} {why}", flush=True)
    if cfg.only in (None, "step"):
        for name, only in STEPS.items():
            ok, why = check_step(root, name, only, wav, port, cfg.buffer)
            port += 10
            results[name] = ok
            print(f"[{'PASS' if ok else 'FAIL'}] {name:22s} {why}", flush=True)
    sys_results = {}
    if cfg.only in (None, "systems"):
        for name, codecs in SYSTEMS.items():
            ok, why = check_system(root, name, codecs, wav, port, cfg.buffer)
            port += 30
            sys_results[name] = ok
            print(f"[{'PASS' if ok else 'FAIL'}] {name:22s} {why}", flush=True)

    if cfg.deadline_report:
        print("\n最後一次 commit（對照截止：m1–m9 → 11/11 18:00；step1–6 → 11/16 16:00）")
        for name in list(MODULES) + list(STEPS):
            print(f"  {name:22s} {last_commit(root, os.path.join('tutorial', name))}")
        for name in SYSTEMS:
            print(f"  {name:22s} {last_commit(root, name)}")

    if results:
        n_total = len(results)
        n_fail = sum(1 for v in results.values() if not v)
        score = max(0, 40 - 3 * n_fail) if n_total == 15 else None
        print(f"\nsummary: tutorial {n_total - n_fail}/{n_total} 項 PASS" + (f"；缺 {n_fail} 項 → 繳交分 {score}/40" if score is not None else ""))
    if sys_results:
        print("summary: systems " + "、".join(f"{k}={'PASS' if v else 'FAIL'}" for k, v in sys_results.items()) + "（不計入缺項；demo 與審查看）")
    return 1 if any(not v for v in results.values()) else 0


if __name__ == "__main__":
    sys.exit(main())
