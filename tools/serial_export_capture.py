#!/usr/bin/env python3
"""抓取显示端导出的 CSV / BMP（USART1，导出期间 460800）。

用法：
    先运行：  python tools/serial_export_capture.py --port COM3
    再在板上：顶栏 SAVE → SAVE CSV / SAVE BMP
    脚本自动识别格式并保存为 export.csv / export.bmp（可加 --out 指定）

说明：
- 显示端空闲时 USART1 是 115200，点 SAVE 后切到 460800；本脚本固定开 460800，
  空闲期间线上没有数据，不会混入乱码。
- CSV 以 "# Remote Data Scope CSV" 开头，以 "# END OF DATA" 结束；
- BMP 为 54 字节头 + 480×272×24bpp = 391734 字节，收满即停。
"""

import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("缺少 pyserial：pip install pyserial")

BMP_SIZE = 54 + 480 * 272 * 3          # 391734（整屏 480×272，24bpp）
CSV_END = b"# END OF DATA"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM3", help="串口号，默认 COM3（ST-Link 虚拟串口）")
    ap.add_argument("--baud", type=int, default=460800, help="导出波特率，默认 460800")
    ap.add_argument("--out", default="", help="输出文件名（省略则按格式自动命名）")
    ap.add_argument("--wait", type=float, default=60.0, help="等待按下 SAVE 的最长时间（秒）")
    args = ap.parse_args()

    try:
        port = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as e:
        sys.exit("打开 %s 失败：%s" % (args.port, e))

    print("已打开 %s @ %d，等你在板上点 SAVE ..." % (args.port, args.baud))

    # 等第一段数据，用前两个字节判断格式
    data = bytearray()
    t0 = time.time()
    first = b""
    while time.time() - t0 < args.wait:
        chunk = port.read(64)
        if chunk:
            first = chunk
            data.extend(chunk)
            break

    if not first:
        port.close()
        sys.exit("没收到数据：确认线接在 PA9/ST-Link 虚拟串口，且已点 SAVE")

    if first[:2] == b"BM":
        mode = "BMP"
        print("识别为 BMP，应收 %d 字节" % BMP_SIZE)
        while len(data) < BMP_SIZE:
            chunk = port.read(BMP_SIZE - len(data))
            if chunk:
                data.extend(chunk)
        data = data[:BMP_SIZE]
        out = args.out or "export.bmp"
    else:
        mode = "CSV"
        print("识别为 CSV（文本）")
        limit = time.time() + 20.0
        while CSV_END not in data and time.time() < limit:
            chunk = port.read(4096)
            if chunk:
                data.extend(chunk)
                limit = time.time() + 3.0
        idx = data.find(CSV_END)
        if idx >= 0:
            data = data[:idx + len(CSV_END) + 2]      # 带上结尾的 \r\n
        out = args.out or "export.csv"

    port.close()

    if os.path.dirname(out):
        os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(data)

    print("完成：%s  %s  %d 字节" % (mode, out, len(data)))
    if mode == "CSV":
        print("用 Excel / WPS / 记事本打开；共 500 行数据")
    else:
        print("用任意看图软件打开；图像为整屏 480×272")


if __name__ == "__main__":
    main()
