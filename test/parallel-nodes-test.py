#!/usr/bin/env python3
"""
Node throughput of the parallel search, from the repository root:

    python3 test/parallel-nodes-test.py [movetime_seconds] [engine] [thread_option]

Defaults: 60 seconds, build/Release/Qapla, option Threads. Spike names its option CPUs.

Phase 1: two engines with Threads=1 side by side, so the machine carries the same load
as in phase 2. Only the first one's output is shown.
Phase 2: one engine with Threads=2.
Each engine searches the start position for movetime seconds and gets 'quit' five
seconds later. The search output goes to stdout, the last 'nodes' figure of each phase
is summed up at the end.
"""

import subprocess
import sys
import threading
import time

MOVETIME = int(sys.argv[1]) if len(sys.argv) > 1 else 60
ENGINE = sys.argv[2] if len(sys.argv) > 2 else "build/Release/Qapla"
THREAD_OPTION = sys.argv[3] if len(sys.argv) > 3 else "Threads"


def run_engine(threads, show, result):
    engine = subprocess.Popen(
        [ENGINE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL, text=True, bufsize=1)

    def reader():
        for line in engine.stdout:
            line = line.rstrip()
            tokens = line.split()
            # The engine's own search info only, not its 'info string' lines
            if tokens[:1] == ["info"] and tokens[1:2] != ["string"]:
                if "nodes" in tokens:
                    result["nodes"] = int(tokens[tokens.index("nodes") + 1])
                    result["last"] = line
                # Spike reports nps and time instead of a node count
                elif "nps" in tokens and "time" in tokens:
                    nps = int(tokens[tokens.index("nps") + 1])
                    ms = int(tokens[tokens.index("time") + 1])
                    result["nodes"] = nps * ms // 1000
                    result["last"] = line
            if show:
                print(line, flush=True)

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()

    def send(command):
        engine.stdin.write(command + "\n")
        engine.stdin.flush()

    send("uci")
    send(f"setoption name {THREAD_OPTION} value {threads}")
    send("isready")
    send("position startpos")
    send(f"go movetime {MOVETIME * 1000}")
    time.sleep(MOVETIME + 5)
    send("quit")
    engine.wait(timeout=10)
    thread.join(timeout=5)


def phase(title, engines):
    print(f"=== {title} ===", flush=True)
    results = [{"nodes": 0, "last": ""} for _ in engines]
    threads = [
        threading.Thread(target=run_engine, args=(count, show, results[i]))
        for i, (count, show) in enumerate(engines)
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    for r in results:
        print("last:", r["last"][:120], flush=True)
    return [r["nodes"] for r in results]


single = phase("Phase 1: two engines, Threads=1 each (output of the first)",
               [(1, True), (1, False)])
double = phase("Phase 2: one engine, Threads=2", [(2, True)])

print()
print(f"Threads=1: {single[0]:>12,} nodes (shown engine), {single[1]:>12,} nodes (silent engine)")
print(f"Threads=2: {double[0]:>12,} nodes")
if single[0]:
    print(f"ratio Threads=2 / Threads=1 (shown): {double[0] / single[0]:.2f}")
