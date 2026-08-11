# STM32 + FreeRTOS 串口通信与设备管理系统

这是一个用于 MCU 嵌入式求职展示的 STM32 + FreeRTOS 项目，目标是实现一条可靠、可测试、可解释的串口通信链路。

目标链路：

```text
PC Python Test Tool
        ↓ UART
STM32 UART → DMA + IDLE → Ring Buffer → Protocol Parser
        → FreeRTOS Queue → Device Task → Response
```

## 当前进度

已完成 P0-2 基础 UART 链路验证：

```text
Python → USB-TTL → STM32 USART1 → FreeRTOS CommTask
       ←          单字节原样回显          ←
```

当前固件使用 HAL 阻塞式单字节收发完成回显测试。CubeMX 工程中虽然已经配置 DMA，但当前回显链路尚未使用 DMA + IDLE。

已验证：

- Python 可以打开串口。
- PC 可以向 STM32 发送二进制数据。
- STM32 可以接收并原样返回数据。
- Python 可以比较发送数据与接收数据并输出测试结果。

## 硬件与工具

- MCU：STM32F103C8
- RTOS：FreeRTOS（CMSIS-RTOS V2）
- 开发工具：STM32CubeMX、Keil MDK-ARM
- 下载调试：ST-Link
- USB 转串口：CH340
- PC 测试：Python 3、pyserial

## 串口配置

| 参数 | 配置 |
| --- | --- |
| 串口 | USART1 |
| 波特率 | 115200 |
| 数据位 | 8 bit |
| 校验位 | None |
| 停止位 | 1 bit |
| 硬件流控 | None |

## 接线

| CH340 USB-TTL | STM32F103C8 |
| --- | --- |
| TX | PA10 / USART1_RX |
| RX | PA9 / USART1_TX |
| GND | GND |

TX 和 RX 必须交叉连接。建议使用 3.3 V TTL 电平；开发板独立供电时不要重复连接 USB-TTL 的 VCC。

## 项目结构

```text
.
├── firmware/              # STM32 固件工程
│   ├── App/               # 应用初始化
│   ├── BSP/               # 板级驱动接口
│   ├── Comm/              # 通信服务、协议和 Ring Buffer 骨架
│   ├── Common/            # 公共配置与类型
│   ├── Core/              # CubeMX 生成代码与 FreeRTOS 任务
│   ├── Device/            # 设备管理模块骨架
│   ├── Drivers/           # STM32 HAL 与 CMSIS
│   ├── Middlewares/       # FreeRTOS
│   ├── MDK-ARM/           # Keil 工程
│   └── stm32_freertos_serial_manager.ioc
├── tools/
│   └── uart_echo_test.py  # Python 串口回显测试
└── README.md
```

## 编译与下载

1. 使用 Keil 打开：

   ```text
   firmware/MDK-ARM/stm32_freertos_serial_manager.uvprojx
   ```

2. 编译工程并确认没有错误。
3. 通过 ST-Link 将固件下载到开发板。
4. 复位或重新上电开发板。

## Python 回显测试

安装依赖：

```powershell
python -m pip install pyserial
```

确认没有其他串口工具占用端口，然后在仓库根目录运行：

```powershell
python tools/uart_echo_test.py COM7
```

`COM7` 需要替换为实际的 CH340 串口号。测试程序发送三个原始字节：

```text
01 02 03
```

成功输出：

```text
Open port: COM7
Serial port opened
TX: 01 02 03
RX: 01 02 03
PASS
```

## 当前实现说明

`CommTask` 当前使用阻塞式 HAL 接口逐字节接收和回传数据。这种方式实现简单，适合验证串口参数、接线、USB-TTL、STM32 收发和 Python 测试链路，但不作为最终通信方案。

当前尚未实现：

- DMA + IDLE 数据接收
- Ring Buffer 数据搬运
- 流式协议解析
- FreeRTOS Queue 消息传递
- Device Task 命令处理与协议响应

计划协议格式：

```text
SOF | LEN | CMD | DATA | CRC16
```

## 常见问题

### 串口可以打开，但没有收到回显

依次检查：

1. USB-TTL TX 是否连接 PA10。
2. USB-TTL RX 是否连接 PA9。
3. USB-TTL 与 STM32 是否共地。
4. 串口参数是否为 115200、8N1、无流控。
5. COM 口是否被其他程序占用。

### Python 只收到部分数据

`serial.read(size)` 在超时后可能返回少于 `size` 的数据。测试时应检查实际接收长度，并重复执行和断电重启测试，确认链路稳定。

## 下一阶段

基础 UART 链路稳定后，将按以下顺序继续：

```text
UART DMA + IDLE
→ Ring Buffer
→ Protocol Parser
→ FreeRTOS Queue
→ Device Task
→ Response
```

每个模块完成后分别进行边界条件、异常数据和错误恢复测试。
