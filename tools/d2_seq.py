#!/usr/bin/env python3
"""把连续若干帧的 D2 行拼成一张图, 用于观察滚动时边沿是否中断"""
import sys

import cv2
import numpy as np

path = sys.argv[1]
out = sys.argv[2]
start = int(sys.argv[3]) if len(sys.argv) > 3 else 84
count = int(sys.argv[4]) if len(sys.argv) > 4 else 8

cap = cv2.VideoCapture(path)
print("opened:", cap.isOpened())
rows = []
idx = 0
while len(rows) < count:
    ok, img = cap.read()
    if not ok:
        print("read stop at", idx)
        break
    if start <= idx < start + count:
        c = img[538:588, 205:895]
        c = cv2.resize(c, (1380, 70), interpolation=cv2.INTER_NEAREST)
        rows.append(c)
    idx += 1

if rows:
    cv2.imwrite(out, np.vstack(rows))
    print("saved", len(rows), "frames, total", idx)
else:
    print("no frames")
