#!/usr/bin/env python3
"""
采集端模拟器: 按协议向显示端发送波形数据帧, 并接收显示端下发的命令。

接线(USB-TTL):
    模块 TX  -> 显示板 PC6 (USART6_TX)
    模块 RX  -> 显示板 PC7 (USART6_RX)
    GND      -> GND
显示端 USART6 = 921600 8N1。USART1(ST-Link 虚拟串口) 仍用于 CSV/BMP 导出。

帧格式(1706 字节):
    A5 5A | 01 | seq | timebase | flags | len(2) | tick_ms(4) | payload | crc16(2)
    payload = CH1 750B + CH2 750B + D0 64B + D1 64B + D2 64B

用法:
    python link_sim.py COM5
    python link_sim.py COM5 --tb 1 --fps 20
"""

import argparse
import math
import struct
import sys
import time

import serial

SAMPLES = 500
TB_US = [10000, 5000, 1000, 500, 100]
DIG_CHANNELS = 3
ANALOG_BYTES = 750
DIGITAL_BYTES = 64
PAYLOAD_SIZE = ANALOG_BYTES * 2 + DIGITAL_BYTES * DIG_CHANNELS


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc


def pack_analog(values):
    """500 个 12bit 采样 -> 750 字节, 每 3 字节装 2 个点"""
    out = bytearray(ANALOG_BYTES)
    for i in range(0, SAMPLES, 2):
        s0 = values[i] & 0x0FFF
        s1 = values[i + 1] & 0x0FFF
        out[(i // 2) * 3 + 0] = s0 & 0xFF
        out[(i // 2) * 3 + 1] = ((s0 >> 8) & 0x0F) | ((s1 & 0x0F) << 4)
        out[(i // 2) * 3 + 2] = (s1 >> 4) & 0xFF
    return bytes(out)


def pack_digital(values):
    """500 个 0/1 -> 64 字节, 低 500 bit 有效"""
    out = bytearray(DIGITAL_BYTES)
    for i, v in enumerate(values):
        if v:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def build_frame(seq, timebase, flags, samples, tick_ms):
    header = bytes([0xA5, 0x5A, 0x01, seq & 0xFF, timebase & 0xFF, flags & 0xFF]) + \
             struct.pack("<H", PAYLOAD_SIZE) + struct.pack("<I", tick_ms & 0xFFFFFFFF)
    payload = b"".join(samples)
    body = header[2:] + payload
    crc = crc16(body)
    return header + payload + struct.pack("<H", crc)


def generate(timebase, t0, active):
    """按当前时基生成一屏数据, t0 为本帧起始时刻(秒)"""
    window = TB_US[timebase] * 10 / 1e6  # 一屏覆盖的时间(秒)
    ch1 = []
    ch2 = []
    dig = [[], [], []]

    for i in range(SAMPLES):
        t = t0 + window * i / (SAMPLES - 1)

        # CH1: 50Hz 正弦, 2048 ± 900, 与示波器假数据一致
        v1 = 2048 + 900 * math.sin(2 * math.pi * 50 * t)
        ch1.append(max(0, min(4095, int(v1))))

        # CH2: 20Hz 方波, 高 3300 / 低 600
        ch2.append(3300 if (t * 20) % 1.0 < 0.5 else 600)

        # 数字通道: 125Hz / 40Hz / 10Hz, 占空比 50% / 48% / 20%
        dig[0].append(1 if (t * 125) % 1.0 < 0.5 else 0)
        dig[1].append(1 if (t * 40) % 1.0 < 0.48 else 0)
        dig[2].append(1 if (t * 10) % 1.0 < 0.20 else 0)

    if not active[0]:
        ch1 = [2048] * SAMPLES
    if not active[1]:
        ch2 = [2048] * SAMPLES

    payload = [pack_analog(ch1), pack_analog(ch2)]
    for c in range(DIG_CHANNELS):
        payload.append(pack_digital(dig[c]))
    return payload


def parse_commands(buf):
    """从接收缓冲里解析显示端命令, 返回 (命令列表, 剩余缓冲)"""
    cmds = []
    while True:
        start = buf.find(b"\xA5\x5A\x02")
        if start < 0:
            return cmds, buf[-2:]
        if len(buf) - start < 8:
            return cmds, buf[start:]
        frame = buf[start:start + 8]
        if crc16(frame[2:6]) != struct.unpack("<H", frame[6:8])[0]:
            buf = buf[start + 2:]
            continue
        cmds.append((frame[3], frame[4], frame[5]))
        buf = buf[start + 8:]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="串口名, 例如 COM5")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--tb", type=int, default=0, choices=range(5), help="0=10ms 1=5ms 2=1ms 3=500us 4=100us")
    parser.add_argument("--fps", type=int, default=20)
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0)
    print(f"已打开 {args.port} @ {args.baud}, 帧率 {args.fps}fps, 时基 {TB_US[args.tb] / 1000}ms/格")

    timebase = args.tb
    active = [True, True, True, True, True]   # CH1 CH2 D0 D1 D2
    period = 1.0 / args.fps
    start = time.perf_counter()
    seq = 0
    next_frame = start
    rx = b""
    last_report = start

    try:
        while True:
            now = time.perf_counter()
            if now < next_frame:
                # 空闲时收命令
                data = ser.read(256)
                if data:
                    rx += data
                    cmds, rx = parse_commands(rx)
                    for cmd, a0, a1 in cmds:
                        if cmd == 1:
                            timebase = a0 if a0 < 5 else timebase
                            print(f"  <- 时基 -> {TB_US[timebase] / 1000}ms/格")
                        elif cmd == 2:
                            active = [bool(a0 & (1 << i)) for i in range(5)]
                            print(f"  <- 通道使能 mask=0x{a0:02X} {active}")
                        elif cmd == 3:
                            src = a0 & 0x07
                            edge = (a0 >> 3) & 0x01
                            mode = (a0 >> 5) & 0x03
                            print(f"  <- 触发 源={src} 边沿={'RIS' if edge == 0 else 'FAL'} "
                                  f"模式={['AUTO', 'NORM', 'SING'][mode]} 电平={a1 * 16}")
                        elif cmd == 4:
                            print(f"  <- 运行控制 {a0}")
                time.sleep(0.001)
                continue

            t0 = now - start
            flags = 0
            if active[0]:
                flags |= 0x01
            if active[1]:
                flags |= 0x02
            for c in range(DIG_CHANNELS):
                if active[2 + c]:
                    flags |= 0x04 << c

            payload = generate(timebase, t0, active)
            frame = build_frame(seq, timebase, flags, payload, int(t0 * 1000))
            ser.write(frame)
            seq = (seq + 1) & 0xFF

            next_frame += period
            if next_frame < now:            # 落后太多就丢掉积压
                next_frame = now + period

            if now - last_report >= 1.0:
                print(f"  发送 {seq} 帧, 时基 {TB_US[timebase] / 1000}ms/格")
                last_report = now
    except KeyboardInterrupt:
        print("\n退出")
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
