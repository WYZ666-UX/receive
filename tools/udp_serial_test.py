#!/usr/bin/env python3
"""串口调试: 发 UDP 数据帧, 同时读取板子 USART1(COM3) 打印的包长度"""

import math
import socket
import struct
import time

TARGET = ("192.168.9.11", 5000)


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc


def build_part(seq, part, ch1, ch2, dig):
    h = bytearray(20)
    h[0] = 0xA5
    h[1] = 0x5A
    h[2] = 1
    h[3] = 0x20
    h[4] = seq & 0xFF
    h[5] = (seq >> 8) & 0xFF
    h[6] = part
    h[7] = 2
    h[8] = 0x1F
    h[10] = 0xFF
    h[11] = 0xFF
    h[12] = 250
    h[13] = 0
    tick = int(time.time() * 1000) & 0xFFFFFFFF
    h[16] = tick & 0xFF
    h[17] = (tick >> 8) & 0xFF
    h[18] = (tick >> 16) & 0xFF
    h[19] = (tick >> 24) & 0xFF

    payload = bytearray(1250)
    base = part * 250
    for i in range(250):
        idx = base + i
        o = i * 4
        payload[o] = ch1[idx] & 0xFF
        payload[o + 1] = (ch1[idx] >> 8) & 0xFF
        payload[o + 2] = ch2[idx] & 0xFF
        payload[o + 3] = (ch2[idx] >> 8) & 0xFF
        payload[1000 + i] = dig[idx] & 7

    frame = bytes(h) + bytes(payload)
    return frame + struct.pack("<H", crc16(frame))


def make_wave():
    ch1, ch2, dig = [], [], []
    for i in range(500):
        t = i / 499.0
        ch1.append(int(2048 + 900 * math.sin(2 * math.pi * t * 5)))
        ch2.append(3300 if (t * 2.5) % 1.0 < 0.5 else 600)
        dig.append(1 if (t * 12.5) % 1.0 < 0.5 else 0)
    return ch1, ch2, dig


def main():
    import serial

    ser = serial.Serial("COM3", 115200, timeout=0)
    print("COM3 已打开")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ch1, ch2, dig = make_wave()

    for seq in range(5):
        sock.sendto(build_part(seq, 0, ch1, ch2, dig), TARGET)
        sock.sendto(build_part(seq, 1, ch1, ch2, dig), TARGET)
        time.sleep(0.05)
    print("已发送 10 包 (5 帧)")

    end = time.time() + 2.5
    while time.time() < end:
        line = ser.readline()
        if line:
            print("板子:", line.decode(errors="ignore").strip())

    ser.close()


if __name__ == "__main__":
    main()
