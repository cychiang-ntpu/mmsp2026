#!/usr/bin/env python3
"""jitter_sim.py — 離線模擬 Team 2 VoiceLink 的接收端：jitter buffer 要多大？只用標準函式庫。

用法：python3 jitter_sim.py [--frames 1500] [--jitter 30] [--loss 0.05] [--reorder 0.05] [--dup 0.02] [--seed 1]

送端每 20 ms 送一個 frame（seq = 0, 1, 2, …），網路給每一包加上 0..jitter ms 的隨機延遲、以機率 loss 弄丟、
以機率 reorder 多壓 20..60 ms（讓它排到後面的包之後）、以機率 dup 多送一份。
接收端用 team2_voicelink/README.md「jitter buffer 與播放時鐘」的規則，對同一份到達紀錄、
把 --buffer 從 0 掃到 200 ms 各跑一次，印出 late／concealed／overflow／avg_delay／max_delay 的表，
計數規則與 STATS 欄位的定義相同：先看懂這張表的取捨，再去寫 C。
"""
# ── 給第一次讀這支程式的同學 ─────────────────────────────────────────────────────────────
# 對應講義：2.x Team 2（UDP 語音、20 ms frame、jitter buffer、PLC、STATS）。
# 怎麼跑：  python3 jitter_sim.py                     （Windows 打 python；預設 1500 個 frame = 30 秒的語音）
#           python3 jitter_sim.py --jitter 80         抖動變大，late 要到更大的 buffer 才會降到 0
#           python3 jitter_sim.py --loss 0.2 --jitter 0   只有遺失：buffer 再大也救不了 concealed
# 會看到：  buffer 變大 → late 與 concealed 下降（降到只剩真正遺失的那些），但 avg_delay_ms 跟著線性上升；
#           這就是報告第 1 題要討論的取捨。預設參數下 60–100 ms 左右 late 已經是 0。
# 注意：    這是「理想時鐘」的模擬：播放時鐘每 20 ms 準時跳一次、收封包不花時間；真正的程式還有 OS 排程的誤差。
#           reorder 的做法是自己定的（spec 的 udp_impair.py 可能略有不同），uniform 抖動本身就會造成少量亂序。
# ──────────────────────────────────────────────────────────────────────────────────────
import argparse
import random
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

FRAME_MS = 20
BUFFERS = [0, 20, 40, 60, 80, 100, 140, 200]      # 要掃的 --buffer 值（ms）


def make_trace(frames, jitter, loss, reorder, dup, seed):
    """產生網路的到達紀錄：list of (到達時間 ms, seq)，照到達時間排序。同一組 seed 每次都一樣，所以各種 buffer 比的是同一份資料。"""
    rng = random.Random(seed)
    arrivals = []
    for seq in range(frames):
        t_send = seq * FRAME_MS                           # 送端照真實時間：第 seq 包在 20·seq ms 送出
        if rng.random() < loss:                           # 遺失：這包永遠不會到
            continue
        delay = rng.uniform(0, jitter)                    # 網路延遲：0 到 jitter ms 之間均勻分布
        if rng.random() < reorder:
            delay += rng.uniform(FRAME_MS, 3 * FRAME_MS)  # 亂序：再壓 1..3 個 frame 的時間，讓後面的包先到
        arrivals.append((t_send + delay, seq))
        if rng.random() < dup:                            # 重複：同一包再到一次（晚一點）
            arrivals.append((t_send + delay + rng.uniform(0, jitter), seq))
    arrivals.sort()                                       # tuple 的比較先比第一個元素（時間），所以就是照到達時間排
    return arrivals


def simulate(arrivals, frames, buffer_ms):
    """照 spec 的規則跑一次接收端。回傳 STATS 的各欄位（dict）。"""
    # spec：「buffer 要有上限（建議 --buffer 的 4 倍），滿了就丟最舊的（計 overflow）」。buffer = 0 時上限至少留 1 個 frame。
    cap = max(1, 4 * buffer_ms // FRAME_MS)
    stats = dict(late=0, dup=0, reordered=0, overflow=0, concealed=0, played=0)
    buf = {}             # seq → 到達時間（還沒播的包）
    seen = set()         # 收過的 seq（含已播、已丟的），用來判斷重複
    highest = -1         # 目前收過最大的 seq，用來判斷亂序
    delays = []          # 每個真的播出去的包：播放時間 − 到達時間
    next_seq = None      # 「下一個該播的 seq」；開始播放前還不知道
    i = 0                # arrivals 的讀取位置

    def receive(t, seq):
        """一個封包到達時怎麼處理（spec：遲到丟掉、重複丟掉、亂序排回去、滿了丟最舊的）。"""
        nonlocal highest
        if seq in seen:
            stats["dup"] += 1                             # 重複：同一個 seq 已經收過
            return
        seen.add(seq)
        if next_seq is not None and seq < next_seq:
            stats["late"] += 1                            # 遲到：它的拍子已經過了（已經用 PLC 補掉了）
            return
        if seq < highest:
            stats["reordered"] += 1                       # 亂序但還來得及：放進 buffer，dict 用 seq 當 key 自然就排好了
        highest = max(highest, seq)
        buf[seq] = t
        if len(buf) > cap:
            buf.pop(min(buf))                             # 滿了：丟最舊（seq 最小）的那一個
            stats["overflow"] += 1

    # 播放時鐘：第一包到達後，先等 buffer_ms（累積資料），然後每 20 ms 準時交出一個 frame，直到最後一個 seq 播完。
    t_start = arrivals[0][0] + buffer_ms
    t = t_start
    while True:
        while i < len(arrivals) and arrivals[i][0] <= t:  # 這一拍之前到的包先收進來
            receive(*arrivals[i])
            i += 1
        if next_seq is None:
            next_seq = min(buf) if buf else 0             # 開始播放：從 buffer 裡最小的 seq 播起
        if next_seq >= frames:
            break
        if next_seq in buf:                               # buffer 裡有「下一個該播的 seq」→ 播它
            delays.append(t - buf.pop(next_seq))
            stats["played"] += 1
        else:                                             # 沒有 → PLC 補一個 frame，seq 照樣前進
            stats["concealed"] += 1
        next_seq += 1
        t += FRAME_MS
    while i < len(arrivals):                              # 播完之後才到的包：全都是遲到（或重複）
        receive(*arrivals[i])
        i += 1
    stats["frames_out"] = stats["played"] + stats["concealed"]
    stats["avg_delay"] = sum(delays) / len(delays) if delays else 0.0
    stats["max_delay"] = max(delays) if delays else 0.0
    return stats


def main():
    ap = argparse.ArgumentParser(description="VoiceLink 接收端的 jitter buffer 模擬")
    ap.add_argument("--frames", type=int, default=1500, help="送幾個 frame（1500 = 30 秒）")
    ap.add_argument("--jitter", type=float, default=30, help="網路延遲 0..jitter ms 均勻分布")
    ap.add_argument("--loss", type=float, default=0.05, help="遺失機率")
    ap.add_argument("--reorder", type=float, default=0.05, help="被多壓 20..60 ms 的機率（製造亂序）")
    ap.add_argument("--dup", type=float, default=0.02, help="重複的機率")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    arrivals = make_trace(args.frames, args.jitter, args.loss, args.reorder, args.dup, args.seed)
    received = len(arrivals)
    lost = args.frames - len({seq for _, seq in arrivals})   # 從 seq 判斷：一份都沒到的才算 lost（亂序後仍沒到）
    print(f"送 {args.frames} 個 frame（{args.frames * FRAME_MS / 1000:.0f} 秒），每 {FRAME_MS} ms 一包；"
          f"jitter 0..{args.jitter:g} ms、loss {args.loss:g}、reorder {args.reorder:g}、dup {args.dup:g}、seed {args.seed}")
    print(f"網路之後：received = {received}（含重複）、lost = {lost}（這兩個和 buffer 大小無關）\n")
    print(f"{'buffer_ms':>9} {'late':>6} {'concealed':>9} {'overflow':>8} {'avg_delay_ms':>12} {'max_delay_ms':>12}"
          f"   {'reordered':>9} {'dup':>5} {'frames_out':>10}")
    for b in BUFFERS:
        s = simulate(arrivals, args.frames, b)
        print(f"{b:>9d} {s['late']:>6d} {s['concealed']:>9d} {s['overflow']:>8d} "
              f"{s['avg_delay']:>12.1f} {s['max_delay']:>12.1f}"
              f"   {s['reordered']:>9d} {s['dup']:>5d} {s['frames_out']:>10d}")
    print("\n讀法：concealed = 遺失 + 遲到 + overflow 丟掉的；buffer 越大 late 越少、但每一包都要多等（avg_delay_ms 上升）。")
    print("      frames_out = 播出的 + concealed，應該等於送出的 frame 數（驗收第 7 項）。")


main()
