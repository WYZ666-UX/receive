#!/usr/bin/env python3
"""分析导出的 CSV: 时间轴刻度、电压范围、过中点次数、反算频率"""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "export_tb0.bin"
text = open(path, encoding="utf-8", errors="ignore").read()
lines = text.splitlines()

rows = []
for line in lines:
    parts = line.split(",")
    if len(parts) >= 7 and parts[0].strip().isdigit():
        rows.append([int(p) for p in parts[:7]])

if not rows:
    print("没有解析到数据行")
    sys.exit(1)

t = [r[1] for r in rows]
ch1 = [r[2] for r in rows]
ch2 = [r[3] for r in rows]

step = t[1] - t[0]
window_us = t[-1] - t[0]

lo1, hi1 = min(ch1), max(ch1)
mid1 = (lo1 + hi1) // 2

rises = 0
rises_t = []
for i in range(1, len(ch1)):
    if ch1[i - 1] < mid1 <= ch1[i]:
        rises += 1
        rises_t.append(t[i])

print(f"数据行数      : {len(rows)}")
print(f"time_us       : {t[0]} .. {t[-1]}  (步进 {step} us)")
print(f"一屏窗口      : {window_us / 1000:.1f} ms")
print(f"CH1 范围      : {lo1} .. {hi1} mV  (峰峰 {hi1 - lo1} mV, 中点 {mid1})")
print(f"CH1 上升沿数  : {rises}  位置: {rises_t}")

if rises >= 2:
    period_us = (rises_t[-1] - rises_t[0]) / (rises - 1)
    freq = 1e6 / period_us
    print(f"周期(上升沿间隔): {period_us:.1f} us")
    print(f"反算频率      : {freq:.2f} Hz")
else:
    print("上升沿不足 2 个, 无法算频率")
    if rises == 1 and len(ch1) > 1:
        print("只有 1 个上升沿: 窗口内不足一个完整周期")

lo2, hi2 = min(ch2), max(ch2)
print(f"CH2 范围      : {lo2} .. {hi2} mV")
print(f"数字通道首行  : d0={rows[0][4]} d1={rows[0][5]} d2={rows[0][6]}")
