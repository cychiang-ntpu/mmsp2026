#!/usr/bin/env python3
"""udp_impair.py：搗蛋的 UDP 中繼。夾在 send 與 recv 中間，把網路「弄壞」：遺失、抖動、亂序、重複。

  python3 udp_impair.py <listen_port> <fwd_ip> <fwd_port> [--loss p] [--jitter ms] [--reorder p] [--dup p] [--seed n] [--bind ip] [--quiet]

  例：recv 聽 5004，send 改送到 5005，中間夾著它：
    python3 udp_impair.py 5005 127.0.0.1 5004 --loss 0.05 --jitter 30 --reorder 0.1 --dup 0.02
    voicelink recv 5004 out.wav
    voicelink send 127.0.0.1 5005 in.wav

  每個收到的 datagram 依序經過四關（都用同一個亂數產生器，給 --seed 就每次一樣）：
    --loss p      機率 p 直接丟掉
    --jitter ms   延遲 uniform(0, ms) 毫秒再轉送。後面的包抽到比較短的延遲就會「超車」——這本身就會造成亂序
    --reorder p   機率 p 額外「扣住」這一包，等「下一包」送出之後才送（保證至少和一包對調）
    --dup p       機率 p 多送一份複本（約 5 ms 之後）
  兩個方向都會搗蛋：記住 listen 這邊最後一個來源位址；從 fwd_ip:fwd_port 回來的封包就轉給它。
  所以 C 版的 call（雙向）也可以夾著它測。
  Ctrl-C 結束，印出 forwarded／dropped／reordered／duplicated 的數量。只用標準函式庫。

  做法：一個 UDP socket、一個 select 迴圈、一個以「該送出的時間」排序的 heap（heapq）。
  收到封包 → 決定它的命運、算出該送出的時刻、push 進 heap；每一輪把到期的 pop 出來送掉。
  不用多執行緒：select 的逾時就設成「離最早到期還有多久」。
"""
import argparse
import heapq
import random
import select
import socket
import sys
import time

DUP_DELAY = 0.005          # 複本在原本那一份之後 5 ms 送出
HOLD_GAP = 0.001           # 被扣住的包：在「下一包」之後 1 ms 送出
HOLD_FLUSH = 0.5           # 扣住之後 0.5 秒都沒有下一包（串流結束了）：直接送出，不要吞掉
MAX_IDLE = 0.2             # select 最多等這麼久就醒來一次：Windows 的 select 不會被 Ctrl-C 打斷，要醒來才看得到


def main():
    ap = argparse.ArgumentParser(description="搗蛋的 UDP 中繼：遺失、抖動、亂序、重複")
    ap.add_argument("listen_port", type=int, help="聽這個 port（send 改送到這裡）")
    ap.add_argument("fwd_ip", help="轉給誰（recv 的 ip）")
    ap.add_argument("fwd_port", type=int, help="轉給哪個 port（recv 的 port）")
    ap.add_argument("--loss", type=float, default=0.0, help="遺失機率 0–1（預設 0）")
    ap.add_argument("--jitter", type=float, default=0.0, help="每包延遲 uniform(0, ms)（預設 0）")
    ap.add_argument("--reorder", type=float, default=0.0, help="扣住、等下一包之後再送的機率（預設 0）")
    ap.add_argument("--dup", type=float, default=0.0, help="多送一份複本的機率（預設 0）")
    ap.add_argument("--seed", type=int, default=None, help="亂數種子；給了就每次一樣")
    ap.add_argument("--bind", default="0.0.0.0", help="聽哪個介面（預設 0.0.0.0）")
    ap.add_argument("--quiet", action="store_true", help="不要逐包印出發生了什麼")
    args = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    for name in ("loss", "reorder", "dup"):
        if not 0.0 <= getattr(args, name) <= 1.0:
            ap.error(f"--{name} 要在 0 到 1 之間")
    if args.jitter < 0 or args.listen_port not in range(1, 65536) or args.fwd_port not in range(1, 65536):
        ap.error("--jitter 不可以是負的；port 要是 1–65535")

    rng = random.Random(args.seed)
    fwd = (socket.gethostbyname(args.fwd_ip), args.fwd_port)      # 先解析成數字 ip，之後才能和 recvfrom 的來源比對
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SIO_UDP_CONNRESET"):                        # Windows：對方 port 沒開時 ICMP 會讓 recvfrom 丟 ConnectionResetError，關掉它
        sock.ioctl(socket.SIO_UDP_CONNRESET, False)
    sock.bind((args.bind, args.listen_port))
    sock.setblocking(False)
    print(f"udp_impair：聽 {args.bind}:{args.listen_port} → 轉給 {fwd[0]}:{fwd[1]}｜"
          f"loss={args.loss} jitter={args.jitter}ms reorder={args.reorder} dup={args.dup} seed={args.seed}（Ctrl-C 結束）", flush=True)

    # 狀態：
    #   pending   heap，元素 (該送出的時刻, 流水號, 目的位址, 資料)。流水號讓時刻相同時不會去比較 bytes，也保持先進先出
    #   held      每個方向最多扣住一包：方向 → (扣住的時刻, 目的位址, 資料)；方向用目的位址當 key
    #   client    listen 這邊最後一個來源位址（回程的封包要轉給它）
    #   counts    結束時印的四個數字。forwarded 是真的送出去的 datagram 數（含複本）
    pending, held, client, serial = [], {}, None, 0
    counts = dict(forwarded=0, dropped=0, reordered=0, duplicated=0)

    def schedule(when, dest, data):
        nonlocal serial
        heapq.heappush(pending, (when, serial, dest, data))
        serial += 1

    def log(msg):
        if not args.quiet:
            print(msg, flush=True)

    try:
        while True:
            now = time.perf_counter()
            # 1. 送出所有到期的
            while pending and pending[0][0] <= now:
                _, _, dest, data = heapq.heappop(pending)
                try:
                    sock.sendto(data, dest)
                    counts["forwarded"] += 1
                except OSError as why:                                  # 對方還沒開、或路不通：這一包就當作掉了
                    log(f"  送不出去（{why}），丟掉")
            # 2. 扣太久的（串流結束了）直接放行
            for dest in list(held):
                t_held, _, data = held[dest]
                if now - t_held > HOLD_FLUSH:
                    schedule(now, dest, data)
                    del held[dest]
            # 3. 等：最早到期的時刻、或 MAX_IDLE，看哪個先到。Windows 的 select 逾時解析度只有 ~15 ms，延遲會不準，
            #    所以用 select(0) 看一下有沒有封包，沒有就 sleep 最多 1 ms 再看
            wait = min(pending[0][0] - now if pending else MAX_IDLE, MAX_IDLE)
            readable, _, _ = select.select([sock], [], [], 0)
            if not readable:
                if wait > 0:
                    time.sleep(min(wait, 0.001))
                continue
            try:
                data, src = sock.recvfrom(65535)
            except OSError:
                continue
            # 4. 決定方向：從 fwd 回來的 → 轉給 client；其他的都當成 client → 轉給 fwd
            if src == fwd:
                if client is None:
                    continue                                            # 還不知道要回給誰
                dest = client
            else:
                client, dest = src, fwd
            # 5. 四關。每一包固定抽四次亂數，--seed 相同、封包順序相同，命運就相同
            r_loss, delay, r_reorder, r_dup = rng.random(), rng.uniform(0, args.jitter / 1000.0), rng.random(), rng.random()
            now = time.perf_counter()
            if r_loss < args.loss:
                counts["dropped"] += 1
                log(f"  {len(data):4d} bytes {src[0]}:{src[1]} → 丟掉")
                continue
            due = now + delay
            note = f"延遲 {delay * 1000:.1f} ms" if args.jitter else "直接轉"
            if r_dup < args.dup:
                schedule(due + DUP_DELAY, dest, data)
                counts["duplicated"] += 1
                note += "，+複本"
            # 扣住的包：等「下一包」排進 heap 之後，排在它後面 1 ms
            if dest in held:
                t_held, _, data_held = held.pop(dest)
                schedule(due + HOLD_GAP, dest, data_held)
                note += "，前一包補送"
            if r_reorder < args.reorder:
                held[dest] = (now, dest, data)
                counts["reordered"] += 1
                note += "，扣住等下一包"
            else:
                schedule(due, dest, data)
            log(f"  {len(data):4d} bytes {src[0]}:{src[1]} → {dest[0]}:{dest[1]}：{note}")
    except KeyboardInterrupt:
        pass
    finally:
        # 結束前把還排著的都送掉（例如最後幾個 BYE 還在等它們的延遲），再印統計
        for _, _, dest, data in sorted(pending):
            try:
                sock.sendto(data, dest)
                counts["forwarded"] += 1
            except OSError:
                pass
        for dest, (_, _, data) in held.items():
            try:
                sock.sendto(data, dest)
                counts["forwarded"] += 1
            except OSError:
                pass
        sock.close()
        print(f"udp_impair：forwarded={counts['forwarded']} dropped={counts['dropped']} reordered={counts['reordered']} duplicated={counts['duplicated']}",
              file=sys.stderr, flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
