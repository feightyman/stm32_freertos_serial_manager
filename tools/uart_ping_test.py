"""STM32 二进制串口协议的端到端验证工具。

线上帧为 ``SOF | LEN | CMD | DATA | CRC_H | CRC_L``；CRC 覆盖
``[LEN, CMD, DATA...]``。脚本负责构造请求、解析并校验响应，同时覆盖
错误响应、静默丢弃和跨 UART 接收批次等边界场景。
"""

import sys
import time
from dataclasses import dataclass
from typing import Callable

import serial


SOF = 0xAA
PROTOCOL_MAX_DATA_LEN = 32

CMD_PING = 0x01
CMD_PING_RESP = 0x81
CMD_SET_MODE = 0x02
RESP_SET_MODE = 0x82
CMD_GET_STATUS = 0x03
RESP_GET_STATUS = 0x83
RESP_ERROR = 0xFF

ERR_INVALID_LENGTH = 0x01
ERR_INVALID_PARAM = 0x02
ERR_UNKNOWN_CMD = 0x03

MODE_IDLE = 0x00
MODE_ACTIVE = 0x01
INVALID_MODE = 0xFF
UNKNOWN_CMD = 0x7E

BAUD_RATE = 115200
RESPONSE_TIMEOUT_S = 1.0
NO_RESPONSE_TIMEOUT_S = 1.0
READ_POLL_SLICE_S = 0.05
STARTUP_SETTLE_S = 0.10
HALF_PACKET_GAP_S = 0.05
PING_REPETITIONS = 5
BAD_CRC_REPETITIONS = 2
GARBAGE_PREFIX = bytes([0x00, 0x55, 0xFF, 0x7E])


class FrameReadError(RuntimeError):
    """接收侧协议错误的基类。"""


class NoResponseTimeout(FrameReadError):
    """在整体截止时间前没有收到 SOF。"""


class IncompleteFrame(FrameReadError):
    """已经收到帧起点，但在整体截止时间前没有收齐。"""


class InvalidFrameLength(FrameReadError):
    """已收到 LEN 越界候选，但截止时间内没有找到合法帧。"""


class FrameCrcError(FrameReadError):
    """帧结构完整，但接收 CRC 与本地计算结果不一致。"""


@dataclass(frozen=True)
class ReceivedFrame:
    """CRC 校验通过的接收帧，以及解析该帧前跳过的前缀字节。"""

    raw: bytes
    length: int
    cmd: int
    data: bytes
    crc: int
    discarded_prefix: bytes = b""


def crc16_ccitt_false(data: bytes) -> int:
    """按非反射 CRC-16/CCITT-FALSE 计算输入字节序列的校验值。"""

    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd: int, data: bytes = b"") -> bytes:
    """按线上格式组帧；CMD 必须为 1 byte，DATA 最多为 32 bytes。"""

    if not 0 <= cmd <= 0xFF:
        raise ValueError(f"CMD out of range: {cmd}")
    if len(data) > PROTOCOL_MAX_DATA_LEN:
        raise ValueError(
            f"DATA too long: {len(data)} > {PROTOCOL_MAX_DATA_LEN}"
        )

    payload = bytes([len(data), cmd]) + data
    crc = crc16_ccitt_false(payload)
    return bytes([SOF]) + payload + crc.to_bytes(2, "big")


def read_exact(ser: serial.Serial, size: int, deadline: float, stage: str) -> bytes:
    """在同一个整体 deadline 内读满 size bytes，并在返回前恢复串口 timeout。"""

    received = bytearray()

    while len(received) < size:
        remaining_time = deadline - time.monotonic()
        if remaining_time <= 0:
            raise IncompleteFrame(
                f"timeout while reading {stage}: "
                f"received {len(received)}/{size} byte(s), partial={received.hex(' ')}"
            )

        previous_timeout = ser.timeout
        try:
            ser.timeout = min(READ_POLL_SLICE_S, remaining_time)
            chunk = ser.read(size - len(received))
        finally:
            ser.timeout = previous_timeout

        if chunk:
            received.extend(chunk)

    return bytes(received)


def read_frame(ser: serial.Serial, timeout_s: float = RESPONSE_TIMEOUT_S) -> ReceivedFrame:
    """扫描并返回一个 CRC 正确的完整帧，所有字段共享同一个整体超时。

    SOF 前的字节和 LEN 越界的候选会被跳过并记录；若最终仍未得到合法帧，
    根据已经观察到的数据阶段抛出对应的 FrameReadError 子类。
    """

    deadline = time.monotonic() + timeout_s
    discarded = bytearray()
    saw_any_byte = False
    saw_sof = False
    last_invalid_length = None
    pending_sof = False

    while True:
        # LEN 字节本身为 0xAA 时，把同一个字节复用为下一候选帧的 SOF。
        if pending_sof:
            marker = bytes([SOF])
            pending_sof = False
        else:
            remaining_time = deadline - time.monotonic()
            if remaining_time <= 0:
                # 区分非法 LEN、残帧和完全无响应，便于定位链路停在哪个阶段。
                if last_invalid_length is not None:
                    raise InvalidFrameLength(
                        f"no valid frame followed invalid LEN={last_invalid_length}; "
                        f"discarded={discarded.hex(' ')}"
                    )
                if saw_sof:
                    raise IncompleteFrame(
                        "timeout after SOF while waiting for LEN"
                    )
                if saw_any_byte:
                    raise NoResponseTimeout(
                        f"only non-SOF bytes received: {discarded.hex(' ')}"
                    )
                raise NoResponseTimeout("no response before overall deadline")

            previous_timeout = ser.timeout
            try:
                ser.timeout = min(READ_POLL_SLICE_S, remaining_time)
                marker = ser.read(1)
            finally:
                ser.timeout = previous_timeout

            if not marker:
                continue
            saw_any_byte = True

        if marker[0] != SOF:
            discarded.extend(marker)
            continue

        saw_sof = True
        try:
            length_bytes = read_exact(ser, 1, deadline, "LEN")
        except IncompleteFrame as exc:
            raise IncompleteFrame(
                f"timeout after SOF while reading LEN; discarded={discarded.hex(' ')}"
            ) from exc

        length = length_bytes[0]
        if length > PROTOCOL_MAX_DATA_LEN:
            # 越界候选不再按其声明长度读取，继续在当前 deadline 内寻找下一帧。
            discarded.extend((SOF, length))
            last_invalid_length = length
            if length == SOF:
                pending_sof = True
            continue

        tail = read_exact(
            ser,
            length + 3,
            deadline,
            f"CMD + DATA({length}) + CRC16",
        )
        cmd = tail[0]
        data = tail[1 : 1 + length]
        received_crc = int.from_bytes(tail[-2:], "big")
        # CRC 输入从 LEN 开始，到 DATA 结束；SOF 和接收 CRC 本身不参与计算。
        crc_input = length_bytes + tail[:-2]
        expected_crc = crc16_ccitt_false(crc_input)
        raw = bytes([SOF]) + length_bytes + tail

        if received_crc != expected_crc:
            raise FrameCrcError(
                f"CRC mismatch: received=0x{received_crc:04X}, "
                f"expected=0x{expected_crc:04X}, frame={raw.hex(' ')}"
            )

        return ReceivedFrame(
            raw=raw,
            length=length,
            cmd=cmd,
            data=data,
            crc=received_crc,
            discarded_prefix=bytes(discarded),
        )


def read_buffered_input(ser: serial.Serial) -> bytes:
    waiting = ser.in_waiting
    return ser.read(waiting) if waiting else b""


def require_clean_input(ser: serial.Serial, context: str) -> None:
    """确保当前用例开始前没有遗留响应，避免把旧帧误判为本次结果。"""

    pending = read_buffered_input(ser)
    if pending:
        raise AssertionError(
            f"unexpected buffered input before {context}: {pending.hex(' ')}"
        )


def write_all(ser: serial.Serial, data: bytes) -> None:
    """写出完整请求并等待主机发送缓冲区排空；短写视为链路错误。"""

    written = ser.write(data)
    ser.flush()
    if written != len(data):
        raise IOError(f"short serial write: wrote {written}/{len(data)} byte(s)")


def assert_frame(
    frame: ReceivedFrame,
    expected_cmd: int,
    expected_data: bytes,
    context: str,
) -> None:
    """同时校验响应语义字段和重新组装后的完整原始帧。"""

    if frame.discarded_prefix:
        raise AssertionError(
            f"{context}: discarded unexpected response prefix "
            f"{frame.discarded_prefix.hex(' ')}"
        )
    if frame.cmd != expected_cmd:
        raise AssertionError(
            f"{context}: CMD=0x{frame.cmd:02X}, expected=0x{expected_cmd:02X}"
        )
    if frame.length != len(expected_data):
        raise AssertionError(
            f"{context}: LEN={frame.length}, expected={len(expected_data)}"
        )
    if frame.data != expected_data:
        raise AssertionError(
            f"{context}: DATA={frame.data.hex(' ')}, "
            f"expected={expected_data.hex(' ')}"
        )

    expected_raw = build_frame(expected_cmd, expected_data)
    if frame.raw != expected_raw:
        raise AssertionError(
            f"{context}: frame={frame.raw.hex(' ')}, "
            f"expected={expected_raw.hex(' ')}"
        )


def send_and_expect(
    ser: serial.Serial,
    request: bytes,
    expected_cmd: int,
    expected_data: bytes,
    context: str,
) -> ReceivedFrame:
    """执行一次无遗留输入的请求—响应交互，并校验预期帧。"""

    require_clean_input(ser, context)
    write_all(ser, request)
    response = read_frame(ser)
    assert_frame(response, expected_cmd, expected_data, context)
    return response


def assert_no_response(
    ser: serial.Serial,
    timeout_s: float,
    context: str,
) -> None:
    """在指定时段内确认串口保持静默，任意接收字节都判为失败。"""

    deadline = time.monotonic() + timeout_s
    received = bytearray()

    while time.monotonic() < deadline:
        buffered = read_buffered_input(ser)
        if buffered:
            received.extend(buffered)
            break

        remaining_time = deadline - time.monotonic()
        if remaining_time <= 0:
            break

        previous_timeout = ser.timeout
        try:
            ser.timeout = min(READ_POLL_SLICE_S, remaining_time)
            chunk = ser.read(1)
        finally:
            ser.timeout = previous_timeout

        if chunk:
            received.extend(chunk)
            received.extend(read_buffered_input(ser))
            break

    if received:
        raise AssertionError(
            f"{context}: expected silence, received {received.hex(' ')}"
        )


def run_case(
    name: str,
    action: Callable[[], None],
    failures: list[str],
) -> None:
    """记录单个用例结果但继续执行后续用例，便于一次收集多个故障。"""

    try:
        action()
    except Exception as exc:
        failures.append(f"{name}: {exc}")
        print(f"[FAIL] {name}: {exc}")
    else:
        print(f"[PASS] {name}")


def main() -> int:
    """打开指定串口、按顺序执行验证，并在退出前尝试恢复 MODE_IDLE。"""

    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} COMx")
        return 2

    port = sys.argv[1]
    failures: list[str] = []

    try:
        ser = serial.Serial(
            port=port,
            baudrate=BAUD_RATE,
            timeout=READ_POLL_SLICE_S,
        )
    except serial.SerialException as exc:
        print(f"Cannot open {port}: {exc}")
        return 2

    def test_ping() -> None:
        for index in range(PING_REPETITIONS):
            send_and_expect(
                ser,
                build_frame(CMD_PING),
                CMD_PING_RESP,
                b"",
                f"PING repetition {index + 1}",
            )

    def test_initial_status() -> None:
        send_and_expect(
            ser,
            build_frame(CMD_GET_STATUS),
            RESP_GET_STATUS,
            bytes([MODE_IDLE]),
            "initial GET_STATUS",
        )

    def test_set_active_and_get() -> None:
        send_and_expect(
            ser,
            build_frame(CMD_SET_MODE, bytes([MODE_ACTIVE])),
            RESP_SET_MODE,
            bytes([MODE_ACTIVE]),
            "SET_MODE(ACTIVE)",
        )
        send_and_expect(
            ser,
            build_frame(CMD_GET_STATUS),
            RESP_GET_STATUS,
            bytes([MODE_ACTIVE]),
            "GET_STATUS after SET_MODE(ACTIVE)",
        )

    def test_invalid_mode_preserves_state() -> None:
        send_and_expect(
            ser,
            build_frame(CMD_SET_MODE, bytes([INVALID_MODE])),
            RESP_ERROR,
            bytes([CMD_SET_MODE, ERR_INVALID_PARAM]),
            "SET_MODE(invalid)",
        )
        send_and_expect(
            ser,
            build_frame(CMD_GET_STATUS),
            RESP_GET_STATUS,
            bytes([MODE_ACTIVE]),
            "GET_STATUS after invalid mode",
        )

    def test_invalid_length() -> None:
        send_and_expect(
            ser,
            build_frame(CMD_SET_MODE),
            RESP_ERROR,
            bytes([CMD_SET_MODE, ERR_INVALID_LENGTH]),
            "SET_MODE with invalid LEN",
        )

    def test_unknown_command() -> None:
        send_and_expect(
            ser,
            build_frame(UNKNOWN_CMD),
            RESP_ERROR,
            bytes([UNKNOWN_CMD, ERR_UNKNOWN_CMD]),
            "unknown CMD",
        )

    def test_bad_crc_is_silent() -> None:
        require_clean_input(ser, "Bad CRC")
        bad_crc_frame = bytearray(build_frame(CMD_PING))
        # 只翻转 CRC_L 的一位，保持帧结构和请求字段均合法。
        bad_crc_frame[-1] ^= 0x01

        for index in range(BAD_CRC_REPETITIONS):
            write_all(ser, bytes(bad_crc_frame))
            assert_no_response(
                ser,
                NO_RESPONSE_TIMEOUT_S,
                f"Bad CRC repetition {index + 1}",
            )

    def test_half_packet() -> None:
        require_clean_input(ser, "Half Packet")
        request = build_frame(CMD_PING)
        # 在 LEN 后暂停，验证 MCU Parser 能跨两个 DMA/IDLE 批次保留状态。
        split_at = 2
        write_all(ser, request[:split_at])
        time.sleep(HALF_PACKET_GAP_S)
        require_clean_input(ser, "second half of Half Packet")
        write_all(ser, request[split_at:])
        response = read_frame(ser)
        assert_frame(response, CMD_PING_RESP, b"", "Half Packet")

    def test_continuous_frames() -> None:
        require_clean_input(ser, "Continuous Frames")
        request = build_frame(CMD_PING)
        write_all(ser, request + request)

        first = read_frame(ser)
        assert_frame(first, CMD_PING_RESP, b"", "Continuous Frames #1")
        second = read_frame(ser)
        assert_frame(second, CMD_PING_RESP, b"", "Continuous Frames #2")

    def test_garbage_prefix() -> None:
        require_clean_input(ser, "Garbage")
        write_all(ser, GARBAGE_PREFIX + build_frame(CMD_PING))
        response = read_frame(ser)
        assert_frame(response, CMD_PING_RESP, b"", "Garbage")

    def test_invalid_length_resynchronization() -> None:
        require_clean_input(ser, "Resynchronization")
        malformed_prefix = bytes([SOF, PROTOCOL_MAX_DATA_LEN + 1])
        write_all(ser, malformed_prefix + build_frame(CMD_PING))
        response = read_frame(ser)
        assert_frame(response, CMD_PING_RESP, b"", "Resynchronization")

    def cleanup_idle() -> None:
        """清除残留输入后执行 SET/GET 闭环，尽量恢复可重复测试的初始状态。"""

        cleanup_issues = []
        pending = read_buffered_input(ser)
        if pending:
            cleanup_issues.append(
                f"discarded pending input before cleanup: {pending.hex(' ')}"
            )

        try:
            send_and_expect(
                ser,
                build_frame(CMD_SET_MODE, bytes([MODE_IDLE])),
                RESP_SET_MODE,
                bytes([MODE_IDLE]),
                "final SET_MODE(IDLE)",
            )
            send_and_expect(
                ser,
                build_frame(CMD_GET_STATUS),
                RESP_GET_STATUS,
                bytes([MODE_IDLE]),
                "final GET_STATUS",
            )
        except Exception as exc:
            cleanup_issues.append(str(exc))

        if cleanup_issues:
            raise AssertionError("; ".join(cleanup_issues))

    try:
        time.sleep(STARTUP_SETTLE_S)
        startup_bytes = read_buffered_input(ser)
        if startup_bytes:
            print(f"[INFO] discarded startup input: {startup_bytes.hex(' ')}")

        # 用例 3 建立 ACTIVE 状态，用例 4 随后验证非法参数不会改变该状态。
        run_case("1. PING", test_ping, failures)
        run_case("2. Initial GET_STATUS is IDLE", test_initial_status, failures)
        run_case("3. SET_MODE(ACTIVE) then GET_STATUS", test_set_active_and_get, failures)
        run_case(
            "4. Invalid mode is rejected and state remains ACTIVE",
            test_invalid_mode_preserves_state,
            failures,
        )
        run_case("5. Invalid LEN", test_invalid_length, failures)
        run_case("6. Unknown CMD", test_unknown_command, failures)
        run_case("7. Bad CRC is silent", test_bad_crc_is_silent, failures)
        run_case("R1. Half Packet", test_half_packet, failures)
        run_case("R2. Continuous Frames", test_continuous_frames, failures)
        run_case("R3. Garbage before frame", test_garbage_prefix, failures)
        run_case(
            "R4. Invalid LEN resynchronization",
            test_invalid_length_resynchronization,
            failures,
        )
    finally:
        # 即使前面已有失败，也单独记录最终状态恢复结果并关闭串口。
        run_case("8. Restore IDLE", cleanup_idle, failures)
        ser.close()

    if failures:
        print(f"\n{len(failures)} test(s) failed:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print("\nALL TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
