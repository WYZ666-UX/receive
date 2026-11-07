#!/usr/bin/env python3
"""按协议 §10.2 生成本地测试图案载荷, 校验标准答案 CRC32。"""

import binascii
import struct

SINE64 = [
    2048, 2205, 2360, 2512, 2660, 2802, 2937, 3063,
    3179, 3285, 3378, 3459, 3526, 3579, 3617, 3640,
    3648, 3640, 3617, 3579, 3526, 3459, 3378, 3285,
    3179, 3063, 2937, 2802, 2660, 2512, 2360, 2205,
    2048, 1891, 1736, 1584, 1436, 1294, 1159, 1033,
     917,  811,  718,  637,  570,  517,  479,  456,
     448,  456,  479,  517,  570,  637,  718,  811,
     917, 1033, 1159, 1294, 1436, 1584, 1736, 1891,
]


def ch1_value(k):
    half = 50
    p = k % 100
    d = p if p < half else 100 - p
    v = 0 + (4095 * d) // half
    return min(v, 4095)


def ch2_value(k):
    return SINE64[k % 64]


def dig_value(k):
    return (1 if k % 2 == 1 else 0) | (2 if k % 3 == 2 else 0) | (4 if k % 4 == 3 else 0)


def build_payload(k0):
    payload = bytearray(1250)
    for i in range(250):
        k = k0 + i
        o = i * 4
        payload[o:o + 2] = struct.pack("<H", ch1_value(k))
        payload[o + 2:o + 4] = struct.pack("<H", ch2_value(k))
        payload[1000 + i] = dig_value(k)
    return bytes(payload)


def main():
    print("正弦表自检: sum=%d xor=0x%04X (期望 131072 / 0x0F80)" %
          (sum(SINE64), eval("0" + "".join(" ^ %d" % v for v in SINE64))))
    print("前 16 点:")
    for k in range(16):
        print("  k=%2d CH1=%4d CH2=%4d D=%d" % (k, ch1_value(k), ch2_value(k), dig_value(k)))

    for part, k0, expect in ((0, 0, 0xAA003E01), (1, 250, 0x753D82DE)):
        payload = build_payload(k0)
        got = binascii.crc32(payload) & 0xFFFFFFFF
        print("PART%d CRC32 = 0x%08X %s (期望 0x%08X)" %
              (part, got, "PASS" if got == expect else "FAIL", expect))


if __name__ == "__main__":
    main()
