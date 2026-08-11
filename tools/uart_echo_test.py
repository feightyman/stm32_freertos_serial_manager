import sys
import serial
import time

TEST_LENGTHS = [1, 3, 17, 64]

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

    for length in TEST_LENGTHS:
        test_data = bytes(range(1, length + 1))

        ser.reset_input_buffer()
        print(f"Length: {length}")
        print(
            "TX:",
            test_data.hex(" ")
        )
        # 发送数据
        ser.write(test_data)
        ser.flush()



        rx_data = ser.read(length)
        print(
            "RX:",
            rx_data.hex(" ")
        )
        if rx_data == test_data:
            print("PASS")
        else:
            print("FAIL")
    ser.close()

if __name__ == "__main__":
    main()
