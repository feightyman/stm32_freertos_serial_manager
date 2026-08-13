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

已完成 UART DMA + IDLE 不定长接收、Ring Buffer、最小 Streaming Protocol Parser 以及串口回显链路验证：

```text
PC Python Test Tool
→ CH340 USB-TTL
→ STM32 USART1 RX
→ DMA 批量接收
→ Receive Event Callback
→ Ring Buffer
→ FreeRTOS CommTask
→ Streaming Protocol Parser
→ Complete Frame
→ UART Echo
```

已验证：

- Python 可以打开串口。
- PC 可以向 STM32 发送二进制数据。
- DMA 可以将不定长数据搬入接收缓冲区。
- Receive Event Callback 可以取得本批有效长度。
- CommTask 可以将 DMA 数据写入 Ring Buffer，再读出并原样返回 PC。
- DMA 数据复制完成后会立即重新启动接收，再执行 Echo。
- Ring Buffer 已验证空读、正常写入/读取、写满、超容量拒绝和 head/tail 回绕。
- Python 已验证 1、3、17、64 字节，结果均为 TX == RX。
- 最小协议格式为 `SOF | LEN | CMD | DATA`，CRC16 暂未加入。
- Parser 面向连续字节流，不依赖 DMA 批次或 IDLE 作为帧边界。
- Parser 独立测试已验证完整单帧、半包、连续两帧、Garbage + 合法帧、非法 LEN 后重新同步。
- Ring Buffer + Parser 独立集成测试已验证跨两批输入保留半包状态并提取一帧。
- 板上已验证一帧拆成两次 DMA + IDLE 接收后，Parser 能提取 `LEN=3`、`CMD=0x10`、`DATA=0x11 0x22 0x33`。

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
│   ├── Comm/              # Ring Buffer、Protocol Parser 及独立测试
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

`COM7` 需要替换为实际的 CH340 串口号。测试程序在同一个串口会话中依次发送 1、3、17、64 字节，分别读取相同长度的数据并比较 TX 与 RX。

四组测试均应输出 `PASS`：

```text
Length: 1  PASS
Length: 3  PASS
Length: 17 PASS
Length: 64 PASS
```

## Ring Buffer 独立测试

Ring Buffer 使用固定容量的 `uint8_t` 数组，不使用 `malloc`。缓冲区满时拒绝新数据并返回失败，不覆盖已有数据。

在 Visual Studio Developer PowerShell 中运行：

```powershell
cd firmware\Comm
cl /nologo /TC /W4 ring_buffer_test.c ring_buffer.c /Fe:ring_buffer_test.exe
.\ring_buffer_test.exe
```

独立测试覆盖空缓冲区读取、正常写入/读取、写满、超容量写入拒绝以及 head/tail 回绕。

## Streaming Protocol Parser

当前实现的最小协议格式：

```text
SOF | LEN | CMD | DATA
```

- `SOF` 固定为 `0xAA`。
- `LEN` 表示 `DATA` 的字节数，不包含 `CMD`。
- `LEN` 允许为 0，最大为 32。
- Parser 每次接收一个字节，并在完整帧产生时返回成功。
- Parser 状态跨函数调用、Ring Buffer 读空和 DMA + IDLE 接收批次保留。
- CRC16 暂未实现。

在 `firmware\Comm` 目录运行 Parser 独立测试：

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
    protocol_test.c protocol.c `
    -o protocol_test.exe
\.\protocol_test.exe
```

测试覆盖：

- 完整单帧
- 半包
- 连续两帧
- Garbage + 合法帧
- 非法 LEN 后重新同步

运行 Ring Buffer + Parser 独立集成测试：

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
    protocol_ring_buffer_test.c protocol.c ring_buffer.c `
    -o protocol_ring_buffer_test.exe
\.\protocol_ring_buffer_test.exe
```

该测试将一帧拆成两批写入 Ring Buffer，验证第一批读空后 Parser 保留半包状态，第二批到达后恰好提取一个完整 Frame。

## 当前实现说明

### 已实现

已完成 UART DMA + IDLE → Ring Buffer → CommTask → Streaming Protocol Parser → Complete Frame 闭环，同时保留 Echo 便于链路调试。

- RX 使用 `HAL_UARTEx_ReceiveToIdle_DMA()`
- DMA 使用 Normal 模式
- DMA 接收缓冲区容量为 128 字节
- Callback 获取有效长度并通知 CommTask
- Ring Buffer 容量为 256 字节，满时拒绝新数据
- CommTask 将 DMA 有效数据写入 Ring Buffer，再逐字节读出并送入 Parser
- Parser 在 CommTask 启动时初始化一次，不随 DMA/IDLE 批次重新初始化
- 提取完整帧后通过 `frame_count` 进行板上调试验证
- DMA 在数据复制完成后立即重新启动
- CommTask 使用阻塞式 TX 完成 Echo
- Python 已验证 1、3、17、64 字节，TX == RX

### 当前限制

- 当前使用单接收缓冲区
- RX 使用 Normal DMA，每次 IDLE 或接收满后需要由 CommTask 重新启动
- Ring Buffer 满时本批剩余数据会被拒绝，当前仅通过返回值体现
- IDLE 只表示串口线空闲，不等于协议帧边界
- 当前只提取完整 Frame，尚未接入 FreeRTOS Queue 和 Device Task
- 尚未实现 CRC16

当前最小协议格式与后续目标：

```text
当前：SOF | LEN | CMD | DATA
目标：SOF | LEN | CMD | DATA | CRC16
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

当前阶段已经完成最小 Streaming Parser 与 Ring Buffer 集成验证。后续按计划继续：

```text
CRC16
→ FreeRTOS Queue
→ Device Task
→ Response
```

本阶段不继续扩展 Queue 或 Device Task；每个后续模块仍需先独立测试，再接入现有链路。
