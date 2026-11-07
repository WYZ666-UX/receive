#!/usr/bin/env python3
"""打印 CSV 波形细节, 判断波形是否连续、周期数是否正常"""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "export_tb0.bin"
text = open(path, encoding="utf-8", errors="ignore").read()
rows = []
for line in text.splitlines():
    p = line.split(",")
    if len(p) >= 7 and p[0].strip().isdigit():
        rows.append([int(x) for x in p[:7]])

ch1 = [r[2] for r in rows]
t = [r[1] for r in rows]
lo, hi = min(ch1), max(ch1)
mid = (lo + hi) // 2

print(f"中点 = {mid} mV")
print("每 20 个采样打印一次 CH1:")
for i in range(0, len(ch1), 20):
    bar = "#" * (ch1[i] // 50)
    print(f"  i={i:3d} t={t[i]:6d}us ch1={ch1[i]:4d} {bar}")

print("所有跨越中点的位置:")
for i in range(1, len(ch1)):
    if ch1[i - 1] < mid <= ch1[i]:
        print(f"  上升 i={i:3d} t={t[i]:6d}us")
    elif ch1[i - 1] > mid >= ch1[i]:
        print(f"  下降 i={i:3d} t={t[i]:6d}us")

# PART0 与 PART1 交界处检查
print("PART 交界 (i=248..252):", [ch1[i] for i in range(248, 253)])
