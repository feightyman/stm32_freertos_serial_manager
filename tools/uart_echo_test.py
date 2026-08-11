import sys
import serial
import time

TEST_DATA = bytes([0x01, 0x02, 0x03])

def main():
    if len(sys.argv) != 2:
        print("Usage: %s <port>" % sys.argv[0])
        return
    port = sys.argv[1]
    print(f"Open port: {port}")

    try:
        ser = serial.Serial(
            port=port,
            baudrate=115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1
        )
    except serial.SerialException as e:
        print("Open serial failed:")
        print(e)
        return

    print("Serial port opened")
    ser.reset_input_buffer()
    print(
        "TX:",
        TEST_DATA.hex(" ")
    )
    # 发送数据
    ser.write(TEST_DATA)

    # 等待 stm32 回传
    time.sleep(0.1)

    rx_data = ser.read(len(TEST_DATA))
    print(
        "RX:",
        rx_data.hex(" ")
    )
    if rx_data == TEST_DATA:
        print("PASS")
    else:
        print("FAIL")
    ser.close()

if __name__ == "__main__":
    main()
