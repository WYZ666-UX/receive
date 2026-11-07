#!/usr/bin/env python3
"""常驻后台，自动保存显示端导出的 CSV / BMP。

启动一次即可（双击 tools\\启动导出监听.bat）：
之后只要在板上点 SAVE，脚本自己收完并落盘到 exports\\ 下，按时间戳命名。

- 串口固定 460800；板子空闲时是 115200，但空闲期间线上没有数据，不会混入乱码。
- BMP 按文件头里的 bfSize 收整屏（当前 480×272 → 391734 字节）；
- CSV 收到 "# END OF DATA" 即停。
- 端口被占用（比如串口助手开着）时每 3 秒重试一次，不退出。
"""

import argparse
import datetime
import os
import struct
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("缺少 pyserial：pip install pyserial")

CSV_END = b"# END OF DATA"
DEFAULT_BMP_SIZE = 54 + 480 * 272 * 3      # 391734

# 保存目录跟随脚本所在工程：tools 的上一级 + exports
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DIR = os.path.join(BASE_DIR, "exports")
DEFAULT_LOG = os.path.join(DEFAULT_DIR, "watcher.log")


def log(path, msg):
    line = time.strftime("%Y-%m-%d %H:%M:%S ") + msg
    print(line)
    if path:
        try:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "a", encoding="utf-8") as f:
                f.write(line + "\n")
        except OSError:
            pass


def save_bytes(data, folder, prefix, ext):
    os.makedirs(folder, exist_ok=True)
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    name = os.path.join(folder, "%s_%s.%s" % (prefix, ts, ext))
    with open(name, "wb") as f:
        f.write(data)
    return name


def capture_one(port, idle_timeout):
    """收一段完整数据；长时间没数据返回 None。"""
    t0 = time.time()
    head = b""
    while time.time() - t0 < idle_timeout:
        b = port.read(1)
        if b:
            head = b
            break

    if not head:
        return None

    if head == b"B":                      # 可能是 BMP
        buf = bytearray(head)
        while len(buf) < 54:              # 先把 14+40 字节头读全
            c = port.read(54 - len(buf))
            if c:
                buf.extend(c)
            elif time.time() - t0 > 5.0:
                return ("DROP", bytes(buf))
        size = struct.unpack("<I", bytes(buf[2:6]))[0]
        if not (1000 <= size <= 4 * 1024 * 1024):
            size = DEFAULT_BMP_SIZE
        while len(buf) < size:
            c = port.read(min(8192, size - len(buf)))
            if c:
                buf.extend(c)
                t0 = time.time()
            elif time.time() - t0 > 5.0:
                return ("DROP", bytes(buf))
        return ("BMP", bytes(buf[:size]))

    if head == b"#":                      # CSV 文本
        buf = bytearray(head)
        t0 = time.time()
        while CSV_END not in buf and time.time() - t0 < 20.0:
            c = port.read(4096)
            if c:
                buf.extend(c)
                t0 = time.time()
        idx = buf.find(CSV_END)
        if idx >= 0:
            return ("CSV", bytes(buf[:idx + len(CSV_END) + 2]))
        return ("DROP", bytes(buf))

    return ("DROP", bytes(head))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--dir", default=DEFAULT_DIR)
    ap.add_argument("--log", default=DEFAULT_LOG)
    args = ap.parse_args()

    os.makedirs(args.dir, exist_ok=True)
    log(args.log, "监听启动：%s @ %d，保存到 %s" % (args.port, args.baud, args.dir))

    while True:
        try:
            port = serial.Serial(args.port, args.baud, timeout=0.2)
        except Exception as e:
            log(args.log, "打开 %s 失败（%s），3 秒后重试" % (args.port, e))
            time.sleep(3)
            continue

        log(args.log, "串口已打开，等待板上点 SAVE ...")
        try:
            while True:
                r = capture_one(port, 120.0)
                if r is None:
                    continue
                kind, data = r
                if kind == "BMP":
                    path = save_bytes(data, args.dir, "屏幕", "bmp")
                    log(args.log, "已保存 BMP：%s（%d 字节）" % (path, len(data)))
                elif kind == "CSV":
                    path = save_bytes(data, args.dir, "数据", "csv")
                    log(args.log, "已保存 CSV：%s（%d 字节）" % (path, len(data)))
                else:
                    log(args.log, "收到一段不完整/未知数据（%d 字节），已丢弃" % len(data))
                log(args.log, "继续等待下一次 SAVE ...")
        except Exception as e:
            log(args.log, "串口异常：%s，重新打开" % e)
            try:
                port.close()
            except Exception:
                pass
            time.sleep(2)


if __name__ == "__main__":
    main()
