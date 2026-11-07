#!/usr/bin/env python3
"""读取 COM3 导出数据: 收到完整 CSV(END 标记) 或完整 BMP(261054字节) 立即退出"""
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 460800
max_sec = float(sys.argv[3]) if len(sys.argv) > 3 else 120
out = sys.argv[4] if len(sys.argv) > 4 else "export_current.bin"

BMP_EXPECT = 54 + 348 * 250 * 3

ser = serial.Serial(port, baud, timeout=0.1)
print(f"{port} @ {baud} 已打开, 最长等待 {max_sec}s, 收到完整数据即结束")

buf = bytearray()
start = time.time()
mode = None

while time.time() - start < max_sec:
    chunk = ser.read(512)
    if not chunk:
        continue
    buf += chunk

    if mode is None and len(buf) >= 2:
        mode = "bmp" if buf[:2] == b"BM" else "csv"
        print(f"识别为 {mode.upper()}")

    if mode == "bmp" and len(buf) >= BMP_EXPECT:
        break
    if mode == "csv" and b"# END OF DATA" in buf:
        break

ser.close()
raw = bytes(buf)
with open(out, "wb") as f:
    f.write(raw)

print(f"收到 {len(raw)} 字节 -> {out}")
if mode == "bmp":
    print(f"BMP 期望 {BMP_EXPECT}, 实际 {len(raw)}")
