#!/usr/bin/env python3
"""统计画面中三行数字通道的高低电平占比, 判断绘制是否正确"""
import sys

import cv2
import numpy as np

path = sys.argv[1]
img = cv2.imread(path)
hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
# 数字波形是青绿色
mask = cv2.inRange(hsv, (35, 60, 60), (95, 255, 255))

x0, x1 = 177, 700
rows = {"D0": (383, 407), "D1": (410, 434), "D2": (437, 463)}

for name, (y0, y1) in rows.items():
    sub = mask[y0:y1, x0:x1]
    cols = []
    for x in range(sub.shape[1]):
        ys = np.where(sub[:, x])[0]
        cols.append(np.median(ys) if len(ys) else np.nan)
    cols = np.array(cols, dtype=float)
    valid = ~np.isnan(cols)
    if not valid.any():
        print(f"{name}: 未找到波形")
        continue
    mid = (y1 - y0) / 2.0
    high = (cols < mid) & valid
    low = (cols >= mid) & valid
    n = valid.sum()
    print(f"{name}: 有效列 {n}/{len(cols)}, 高 {high.sum() / n:.1%}, 低 {low.sum() / n:.1%}")

    d = np.diff(np.where(high, 1, 0))
    edges = np.where(np.abs(d) > 0)[0]
    if len(edges):
        print(f"   跳变 {len(edges)} 处, 首/末位置 {edges[0]} / {edges[-1]}")
