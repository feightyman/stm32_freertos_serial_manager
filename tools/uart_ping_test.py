import sys
import serial
import time


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd: int, data: bytes = b"") -> bytes:
    payload = bytes([len(data), cmd]) + data
    crc = crc16_ccitt_false(payload)
    return bytes([0xAA, len(data), cmd]) + data + bytes([(crc >> 8) & 0xFF, crc & 0xFF])


PING = build_frame(0x01)                        # AA 00 01 0D 2E
PING_RESP = bytes([0xAA, 0x00, 0x81, 0x9C, 0xA6])
BAD_CRC = bytes([0xAA, 0x00, 0x01, 0x0D, 0x2F])  # 故意错 CRC
GARBAGE = bytes([0x00, 0x55, 0xFF, 0x7E])


def main():
    if len(sys.argv) != 2:
        print("Usage: %s <port>" % sys.argv[0])
        return
    ser = serial.Serial(sys.argv[1], 115200, timeout=1)
    failures = 0

    # 1. 合法 PING x5
    for i in range(1, 6):
        ser.reset_input_buffer()
        ser.write(PING)
        ser.flush()
        rx = ser.read(5)
        if rx == PING_RESP:
            print("PING %d: PASS" % i)
        else:
            print("PING %d: FAIL RX=%s" % (i, rx.hex(" ")))
            failures += 1

    # 2. 坏 CRC x2 -> 不应有响应
    for i in range(1, 3):
        ser.reset_input_buffer()
        ser.write(BAD_CRC)
        ser.flush()
        time.sleep(0.2)
        rx = ser.read(1)
        if rx == b"":
            print("BAD CRC %d: PASS (no response)" % i)
        else:
            print("BAD CRC %d: FAIL RX=%s" % (i, rx.hex(" ")))
            failures += 1

    # 3. 垃圾数据 + 合法 PING -> 重同步并响应
    ser.reset_input_buffer()
    ser.write(GARBAGE + PING)
    ser.flush()
    rx = ser.read(5)
    if rx == PING_RESP:
        print("RESYNC: PASS")
    else:
        print("RESYNC: FAIL RX=%s" % rx.hex(" "))
        failures += 1

    ser.close()
    print("ALL TESTS PASSED" if failures == 0 else "TESTS FAILED: %d" % failures)


if __name__ == "__main__":
    main()
