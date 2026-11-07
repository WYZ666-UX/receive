#!/usr/bin/env python3
"""
显示端 UDP 接收测试: 向 192.168.9.11:5000 发送符合协议 v1.0 的数据帧。

用法:
    python udp_test.py            # 默认 5 秒
    python udp_test.py --sec 10
"""

import argparse
import math
import socket
import struct
import time

TARGET_IP = "192.168.9.11"
TARGET_PORT = 5000

SAMPLES = 500
PART_N = 250
FRAME_LEN = 1272
TB_US = [10000, 5000, 1000, 500, 100]


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc


def build_part(seq, part, tb, ch_en, ch1, ch2, dig, trig_pos=0xFFFF):
    header = bytearray(20)
    header[0] = 0xA5
    header[1] = 0x5A
    header[2] = 0x01
    header[3] = 0x20
    header[4] = seq & 0xFF
    header[5] = (seq >> 8) & 0xFF
    header[6] = part
    header[7] = tb
    header[8] = ch_en
    header[9] = 0 if trig_pos == 0xFFFF else 1   # TRIG_FLAG
    header[10] = trig_pos & 0xFF                 # TRIG_POS
    header[11] = (trig_pos >> 8) & 0xFF
    header[12] = PART_N & 0xFF
    header[13] = (PART_N >> 8) & 0xFF
    header[14] = 0
    header[15] = 0
    tick = int(time.time() * 1000) & 0xFFFFFFFF
    header[16] = tick & 0xFF
    header[17] = (tick >> 8) & 0xFF
    header[18] = (tick >> 16) & 0xFF
    header[19] = (tick >> 24) & 0xFF

    payload = bytearray(1250)
    base = part * PART_N
    for i in range(PART_N):
        idx = base + i
        o = i * 4
        payload[o + 0] = ch1[idx] & 0xFF
        payload[o + 1] = (ch1[idx] >> 8) & 0xFF
        payload[o + 2] = ch2[idx] & 0xFF
        payload[o + 3] = (ch2[idx] >> 8) & 0xFF
        payload[1000 + i] = dig[idx] & 0x07

    frame = bytes(header) + bytes(payload)
    return frame + struct.pack("<H", crc16(frame))


def parse_ctl(data):
    """解析显示端下发的控制帧, 返回 (cmd, payload) 或 None"""
    if len(data) < 6 or data[0] != 0xA5 or data[1] != 0x5B:
        return None
    l = data[2]
    if len(data) != 6 + l:
        return None
    if struct.unpack("<H", data[4 + l:6 + l])[0] != crc16(data[:4 + l]):
        return None
    return data[3], data[4:4 + l]


def build_ack(cmd, payload=b""):
    f = bytes([0xA5, 0x5B, len(payload), cmd]) + payload
    return f + struct.pack("<H", crc16(f))


def build_wave(tb, t0):
    """按当前时基档生成一屏数据。

    t0 是这一屏的起始时刻(秒), 所有通道都用同一条时间轴 window*t + t0。
    之前每个通道额外乘不同的相位系数, 导致 CH1 每帧走 10 列、D0 走 5 列、
    D2 走 20 列, 看起来像数字波形自己在乱抖; 现在所有边沿都统一每帧移 2 列。
    """
    window = TB_US[tb] * 10 / 1e6      # 一屏覆盖时间(秒)

    ch1 = []
    ch2 = []
    dig = []
    for i in range(SAMPLES):
        t = window * i / float(SAMPLES - 1) + t0

        # CH1: 20Hz 正弦
        ch1.append(int(2048 + 900 * math.sin(2 * math.pi * 20.0 * t)))
        # CH2: 5Hz 方波
        ch2.append(3300 if (5.0 * t) % 1.0 < 0.5 else 600)

        # 数字通道: 120Hz / 40Hz / 10Hz
        # 都取 5Hz 的整数倍, 触发任意一路时其余通道也同步, 方便观察触发效果
        b0 = 1 if (120.0 * t) % 1.0 < 0.5 else 0
        b1 = 1 if (40.0 * t) % 1.0 < 0.5 else 0
        b2 = 1 if (10.0 * t) % 1.0 < 0.5 else 0
        dig.append(b0 | (b1 << 1) | (b2 << 2))
    return ch1, ch2, dig


def find_trigger_index(ch1, ch2, dig, src, edge, level):
    """返回 500 点里第一个触发边沿的位置(带小数), 没有则 -1。

    小数插值很重要: 只对齐到整数采样点的话, 每帧相位会差 1 个采样点,
    屏幕上的边沿就会来回跳一列。src: 1=CH1 2=CH2 3=D0 4=D1 5=D2。
    """
    if src >= 3:
        bit = src - 3
        for i in range(1, SAMPLES):
            a = (dig[i - 1] >> bit) & 1
            b = (dig[i] >> bit) & 1
            if (edge == 0 and a == 0 and b == 1) or (edge == 1 and a == 1 and b == 0):
                return (i - 1) + 0.5      # 边沿落在两个采样点之间
    else:
        a = ch1 if src == 1 else ch2
        for i in range(1, SAMPLES):
            hit = (edge == 0 and a[i - 1] < level <= a[i]) or \
                  (edge == 1 and a[i - 1] > level >= a[i])
            if hit:
                d = a[i] - a[i - 1]
                frac = (level - a[i - 1]) / d if d else 0.0
                if frac < 0.0:
                    frac = 0.0
                if frac > 1.0:
                    frac = 1.0
                return (i - 1) + frac
    return -1


DIG_FREQ = {3: 120.0, 4: 40.0, 5: 10.0}


def align_t0(t0, tb, trig):
    """把触发边沿挪到 pre 点所在的位置, 返回对齐后的起始时刻。

    数字通道直接用已知频率算出精确边沿时刻, 不靠采样点判断:
    120Hz 的周期不是整数个采样点, 用采样点对齐每帧会差一点, 屏幕上会看到
    边沿缓慢漂移。模拟通道则用插值后的过零点。
    """
    dt = (TB_US[tb] * 10 / 1e6) / (SAMPLES - 1)
    src = trig["src"]
    pre = trig["pre"]

    if src >= 3:
        f = DIG_FREQ.get(src, 0.0)
        if f <= 0.0:
            return None
        if trig["edge"] == 0:
            k = math.ceil(f * t0 - 1e-9)
            te = k / f
        else:
            k = math.floor(f * t0 - 0.5 + 1e-9) + 1
            te = (k + 0.5) / f
        return te - pre * dt

    ch1, ch2, dig = build_wave(tb, t0)
    idx = find_trigger_index(ch1, ch2, dig, src, trig["edge"], trig["level"])
    if idx < 0:
        return None
    return t0 + (idx - pre) * dt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sec", type=int, default=5)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--on", type=float, default=130.0, help="发送持续秒数")
    parser.add_argument("--off", type=float, default=0.0, help="停止发送秒数")
    parser.add_argument("--tb", type=int, default=-1, help="-1=每4秒轮换, 0..4=固定档位")
    parser.add_argument("--trig", type=int, default=0,
                        help="模拟触发源: 0=关 1=CH1 2=CH2 3=D0 4=D1 5=D2")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(("192.168.9.50", 5001))
        print("已绑定本地 192.168.9.50:5001, 可接收板子回复")
    except OSError as e:
        print(f"绑定本地 5001 失败({e}), 只发不收")

    sock.settimeout(0.002)

    def send(pkt):
        """UDP 发送偶尔会被网卡缓冲短暂占住, 超时就丢这一帧继续,
        不能让 1ms 的接收超时把整个脚本打崩"""
        try:
            sock.sendto(pkt, (TARGET_IP, TARGET_PORT))
            return True
        except OSError:
            return False

    hello = bytes([0xA5, 0x5B, 0x01, 0x01, 0x01])
    hello += struct.pack("<H", crc16(hello))
    send(hello)
    time.sleep(0.02)

    seq = 0
    start = time.perf_counter()
    sent = 0
    received = []
    cycle = args.on + args.off
    next_frame = time.perf_counter()
    override_tb = [None]        # 显示端 SET_TIME_BASE 下发的档位
    trig = {"src": args.trig, "edge": 0, "level": 2048, "pre": 50}   # 0 = 未开启触发
    trig_t0 = [None]            # 触发对齐后锁定的时间起点
    free_t0 = 0.0               # 自由运行时的时间轴

    def handle_rx(data, addr):
        received.append((addr, data))
        r = parse_ctl(data)
        if not r:
            return
        cmd, payload = r
        if cmd == 0x10 and len(payload) >= 1 and payload[0] < len(TB_US):
            override_tb[0] = payload[0]
            trig_t0[0] = None
            send(build_ack(0x90, bytes([override_tb[0]])))
            print(f"  <- 时基 -> {TB_US[override_tb[0]] / 1000}ms/格")
        elif cmd == 0x12 and len(payload) >= 6:
            trig["src"] = payload[0]
            trig["edge"] = payload[1]
            trig["level"] = struct.unpack("<H", payload[2:4])[0]
            trig["pre"] = struct.unpack("<H", payload[4:6])[0]
            trig_t0[0] = None
            send(build_ack(0x92, bytes([trig["src"], trig["edge"]])))
            names = ["OFF", "CH1", "CH2", "D0", "D1", "D2"]
            name = names[trig["src"]] if trig["src"] < len(names) else "?"
            print(f"  <- 触发 -> {name} {'上升' if trig['edge'] == 0 else '下降'} "
                  f"电平={trig['level']} pre={trig['pre']}")

    while time.perf_counter() - start < args.sec:
        elapsed = time.perf_counter() - start
        tb = override_tb[0] if override_tb[0] is not None else (args.tb if args.tb >= 0 else (int(elapsed // 4.0) % len(TB_US)))

        # 保持 60 列/秒 的滚动速度: 60fps 时每帧 1 列, 30fps 时每帧 2 列
        window = TB_US[tb] * 10 / 1e6
        free_t0 += ((60.0 / args.fps) / 340.0) * window

        trig_pos = 0xFFFF
        if trig["src"] > 0:
            # 触发开启后锁定第一次对齐的时间起点:
            # 测试数据是慢放(每帧只前进 1~2 列), 如果每帧都重新找下一个边沿,
            # 选中的边沿会隔一段时间跳一个周期, 其他通道看起来就在抖
            if trig_t0[0] is None:
                trig_t0[0] = align_t0(free_t0, tb, trig)
            if trig_t0[0] is not None:
                t0 = trig_t0[0]
                trig_pos = trig["pre"]
            else:
                t0 = free_t0
        else:
            trig_t0[0] = None
            t0 = free_t0
        ch1, ch2, dig = build_wave(tb, t0)

        if (elapsed % cycle) < args.on:
            send(build_part(seq, 0, tb, 0x1F, ch1, ch2, dig, trig_pos))
            send(build_part(seq, 1, tb, 0x1F, ch1, ch2, dig, trig_pos))
            sent += 2
            seq = (seq + 1) & 0xFFFF

        try:
            while True:
                data, addr = sock.recvfrom(2048)
                handle_rx(data, addr)
        except (BlockingIOError, socket.timeout):
            pass

        next_frame += 1.0 / args.fps
        while time.perf_counter() < next_frame:
            try:
                data, addr = sock.recvfrom(2048)
                handle_rx(data, addr)
            except (BlockingIOError, socket.timeout):
                time.sleep(0.002)

    sock.close()
    print(f"已发送 {sent} 包 ({sent // 2} 帧), 目标 {TARGET_IP}:{TARGET_PORT}")
    if received:
        print(f"收到 {len(received)} 个回复:")
        for addr, data in received[:5]:
            print(f"  来自 {addr}: {data.hex()}")
    else:
        print("未收到任何回复 (板子未回 ACK, 或端口不同)")


if __name__ == "__main__":
    main()
