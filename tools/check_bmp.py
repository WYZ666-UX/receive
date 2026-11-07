#!/usr/bin/env python3
"""检查导出的 BMP 文件头与像素内容"""
import struct
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "export_bmp.bin"

data = open(path, "rb").read()

magic = data[:2]
file_size = struct.unpack("<I", data[2:6])[0]
data_offset = struct.unpack("<I", data[10:14])[0]
header_size = struct.unpack("<I", data[14:18])[0]
width, height = struct.unpack("<ii", data[18:26])
bits = struct.unpack("<H", data[28:30])[0]
image_size = struct.unpack("<I", data[34:38])[0]

expected = data_offset + width * abs(height) * bits // 8

print(f"文件: {path}")
print(f"实际字节数 : {len(data)}")
print(f"理论字节数 : {expected}")
print(f"魔数       : {magic}")
print(f"头声明大小 : {file_size}")
print(f"数据偏移   : {data_offset}")
print(f"信息头大小 : {header_size}")
print(f"宽 × 高    : {width} × {height}")
print(f"位深       : {bits} bit")
print(f"图像数据   : {image_size}")

if len(data) == expected and magic == b"BM":
    print("结论: 完整 BMP")
else:
    print("结论: 不完整或有误")

# 统计非背景像素, 确认确实画了东西
px = data[data_offset:data_offset + image_size]
bg = px[0:3]
nonbg = sum(1 for i in range(0, len(px), 3) if px[i:i + 3] != bg)
total = width * abs(height)
print(f"背景色(BGR) : {tuple(bg)}")
print(f"非背景像素 : {nonbg} / {total} = {nonbg / total * 100:.1f}%")
