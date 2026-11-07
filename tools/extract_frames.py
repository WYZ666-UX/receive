#!/usr/bin/env python3
"""从视频中抽取几帧, 并裁出数字通道区域放大, 便于检查"""
import os
import sys

import cv2

path = sys.argv[1]
outdir = sys.argv[2] if len(sys.argv) > 2 else "frames"
os.makedirs(outdir, exist_ok=True)

cap = cv2.VideoCapture(path)
total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
fps = cap.get(cv2.CAP_PROP_FPS) or 30
print(f"总帧数 {total}, fps {fps:.1f}, 时长 {total / fps:.1f}s, 尺寸 {int(cap.get(3))}x{int(cap.get(4))}")

ratios = [0.15, 0.35, 0.55, 0.75, 0.95]
for r in ratios:
    cap.set(cv2.CAP_PROP_POS_FRAMES, int(total * r))
    ok, frame = cap.read()
    if not ok:
        continue
    h, w = frame.shape[:2]
    cv2.imwrite(os.path.join(outdir, f"full_{int(r * 100):02d}.png"), frame)

    # 数字通道在屏幕下部, 按比例裁切后放大
    y0, y1 = int(h * 0.86), h
    x0, x1 = 0, int(w * 0.75)
    crop = frame[y0:y1, x0:x1]
    crop = cv2.resize(crop, None, fx=3, fy=3, interpolation=cv2.INTER_NEAREST)
    cv2.imwrite(os.path.join(outdir, f"dig_{int(r * 100):02d}.png"), crop)
    print(f"已导出帧 {int(r * 100)}%")

cap.release()
print("完成")
