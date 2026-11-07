#!/usr/bin/env python3
"""读取 ST-Link 虚拟串口 (COM3) 的导出数据并保存到文件"""
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 460800
sec = float(sys.argv[3]) if len(sys.argv) > 3 else 30
out = sys.argv[4] if len(sys.argv) > 4 else "export.bin"

ser = serial.Serial(port, baud, timeout=0.1)
print(f"{port} @ {baud} 已打开, 读取 {sec}s -> {out}")

chunks = []
end = time.time() + sec
while time.time() < end:
    data = ser.read(512)
    if data:
        chunks.append(data)

ser.close()
raw = b"".join(chunks)
with open(out, "wb") as f:
    f.write(raw)

print(f"收到 {len(raw)} 字节")
print("前 160 字节:", raw[:160])
