# STM32 FreeRTOS Serial Manager

基于 STM32F103C8 和 FreeRTOS 的二进制串口通信项目。系统通过 USART1 DMA + IDLE 接收不定长数据，依次完成字节缓存、协议解析、CRC 校验、任务间消息传递和命令响应。

当前版本提供完整的 PING 往返链路，可用于验证 PC 与 MCU 之间的通信、错误帧丢弃和数据流重同步。

```text
PC Python
→ USART1
→ DMA + IDLE
→ Ring Buffer
→ Streaming Parser
→ CRC16
→ FreeRTOS Queue
→ DeviceTask
→ UART Response
```

## Features

- USART1 115200 8N1 串口通信；
- 128 字节 Normal DMA + IDLE 不定长接收；
- 256 字节 Ring Buffer，支持满/空判断和下标回绕；
- Streaming Parser，支持半包、连续帧、垃圾前缀和非法长度重同步；
- CRC-16/CCITT-FALSE 完整性校验；
- FreeRTOS Queue 按值传递协议帧；
- `CMD_PING` 请求与响应；
- Python 串口 smoke test；
- Ring Buffer、Parser 和 CRC16 的 PC 侧 C 测试。

## Architecture

```text
                    STM32F103C8

USART1 RX
    │
    ▼
HAL_UARTEx_ReceiveToIdle_DMA()
    │
    │  RxEvent Callback
    │  保存接收长度与 ready 标志
    ▼
CommTask（AboveNormal）
    │
    ├─ DMA Buffer：128 bytes
    ├─ Ring Buffer：256 bytes
    ├─ Streaming Parser
    ├─ CRC16 Validation
    └─ Protocol Queue：8 frames
                         │
                         ▼
                 DeviceTask（Normal）
                         │
                         └─ CMD_PING Handler
                         │
                         ▼
                HAL_UART_Transmit()
                         │
                         ▼
                     USART1 TX
```

接收与响应流程：

1. `CommTask` 启动 `HAL_UARTEx_ReceiveToIdle_DMA()`，并关闭 DMA Half Transfer 中断。
2. IDLE 或 DMA Transfer Complete 事件触发后，回调保存本批有效长度并置位 ready 标志。
3. `CommTask` 将 DMA 数据写入 Ring Buffer，再逐字节送入 Streaming Parser。
4. Parser 完成组帧后，`CommTask` 对 `LEN | CMD | DATA` 计算 CRC。
5. CRC 正确的 `ProtocolFrame_t` 按值写入深度为 8 的 FreeRTOS Queue。
6. `DeviceTask` 读取命令帧，处理 `CMD_PING` 并通过 USART1 返回响应。

| 参数 | 配置 |
| --- | ---: |
| RX DMA Buffer | 128 bytes |
| Ring Buffer | 256 bytes |
| DATA 最大长度 | 32 bytes |
| `ProtocolFrame_t` | 36 bytes |
| Protocol Queue | 8 frames |
| `CommTask` 优先级 | AboveNormal |
| `DeviceTask` 优先级 | Normal |

## Protocol

### Frame Format

```text
+------+-----+-----+-----------+-------+-------+
| SOF  | LEN | CMD | DATA[LEN] | CRC_H | CRC_L |
+------+-----+-----+-----------+-------+-------+
 1 byte 1 byte 1 byte 0..32 bytes 1 byte  1 byte
```

| 字段 | 说明 |
| --- | --- |
| `SOF` | 固定为 `0xAA` |
| `LEN` | DATA 字节数，不包含 CMD，范围 `0..32` |
| `CMD` | 命令字 |
| `DATA` | 可选负载 |
| `CRC_H` | CRC16 高字节 |
| `CRC_L` | CRC16 低字节 |

完整帧长度为 `5 + LEN`，最短 5 字节，最长 37 字节。

### CRC16

| 参数 | 值 |
| --- | --- |
| Algorithm | CRC-16/CCITT-FALSE |
| Poly | `0x1021` |
| Init | `0xFFFF` |
| RefIn / RefOut | `false / false` |
| XorOut | `0x0000` |
| 计算范围 | `LEN \| CMD \| DATA` |

`SOF` 和 CRC 自身不参与计算，CRC 高字节先发送。标准测试向量：

```text
"123456789" → 0x29B1
```

### Commands

| 名称 | 值 | 方向 | DATA | 说明 |
| --- | ---: | --- | --- | --- |
| `CMD_PING` | `0x01` | PC → MCU | 空 | 通信探测 |
| `CMD_PING_RESP` | `0x81` | MCU → PC | 空 | PING 响应 |

请求：

```text
AA 00 01 0D 2E
```

响应：

```text
AA 00 81 9C A6
```

帧处理规则：

- SOF 前的无关字节会被忽略；
- `LEN > 32` 时 Parser 丢弃当前输入并重新同步；
- CRC 错误帧不会进入业务 Queue；
- Queue 满时采用非阻塞丢弃策略；
- 未识别的命令当前不返回响应。

## Hardware

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F103C8，Cortex-M3，72 MHz |
| RTOS | FreeRTOS 10.3.1，CMSIS-RTOS V2 |
| UART | USART1，115200，8N1，无流控 |
| TX / RX | PA9 / PA10 |
| RX DMA | DMA1 Channel 5，Normal，高优先级 |
| TX DMA | DMA1 Channel 4 已配置，业务发送暂未使用 |
| 固件库 | STM32F1 HAL / CMSIS |
| 工程 | STM32CubeMX + Keil MDK-ARM |
| 下载调试 | ST-Link |
| PC 测试 | Python 3、pyserial、GCC |

### Wiring

| USB-TTL | STM32F103C8 |
| --- | --- |
| TX | PA10 / USART1_RX |
| RX | PA9 / USART1_TX |
| GND | GND |

TX 和 RX 需要交叉连接，并确保两端共地。建议使用 3.3 V TTL 电平；开发板独立供电时不要重复连接 USB-TTL 的 VCC。

## Repository Layout

```text
.
├── firmware/
│   ├── Comm/
│   │   ├── ring_buffer.*             # Ring Buffer
│   │   ├── protocol.*                # 协议类型与 Streaming Parser
│   │   ├── crc16.*                   # CRC-16/CCITT-FALSE
│   │   └── *_test.c                  # PC 侧 C 测试
│   ├── Core/
│   │   └── Src/freertos.c            # CommTask、DeviceTask 与 Queue
│   ├── Device/                       # 设备管理扩展目录
│   ├── Drivers/                      # STM32 HAL 与 CMSIS
│   ├── Middlewares/                  # FreeRTOS
│   ├── MDK-ARM/                      # Keil 工程
│   └── stm32_freertos_serial_manager.ioc
├── tools/
│   └── uart_ping_test.py             # UART 端到端测试
└── README.md
```

## Build and Flash

1. 使用 Keil 打开：

   ```text
   firmware/MDK-ARM/stm32_freertos_serial_manager.uvprojx
   ```

2. 首次打开时，在 `Options for Target → Debug / Utilities` 中确认 ST-Link 和 STM32F1 Flash Algorithm。
3. 编译目标 `stm32_freertos_serial_manager`。
4. 使用 ST-Link 下载固件。
5. 复位开发板并连接 USB-TTL。

CubeMX 工程入口：

```text
firmware/stm32_freertos_serial_manager.ioc
```

## Test

### UART Smoke Test

安装依赖：

```powershell
python -m pip install pyserial
```

在仓库根目录运行：

```powershell
python tools/uart_ping_test.py COM7
```

将 `COM7` 替换为实际串口号。测试内容：

- PING 请求与固定响应比对，共执行 5 轮；
- 发送 2 个错误 CRC 帧，确认设备不响应；
- 发送垃圾字节后追加合法 PING，确认 Parser 能够重新同步。

全部通过时输出：

```text
ALL TESTS PASSED
```

### PC-side C Tests

以下测试不需要 STM32 硬件：

```powershell
Set-Location firmware\Comm

gcc -std=c99 -Wall -Wextra -Werror ring_buffer_test.c ring_buffer.c -o ring_buffer_test.exe
.\ring_buffer_test.exe

gcc -std=c99 -Wall -Wextra -Werror crc16_test.c crc16.c -o crc16_test.exe
.\crc16_test.exe

gcc -std=c99 -Wall -Wextra -Werror protocol_test.c protocol.c -o protocol_test.exe
.\protocol_test.exe

gcc -std=c99 -Wall -Wextra -Werror protocol_ring_buffer_test.c protocol.c ring_buffer.c -o protocol_ring_buffer_test.exe
.\protocol_ring_buffer_test.exe
```

| 测试 | 覆盖内容 |
| --- | --- |
| `ring_buffer_test` | 空读、正常读写、满容量、FIFO 和回绕 |
| `crc16_test` | 标准向量、业务向量、PING 和空输入 |
| `protocol_test` | 完整帧、半包、连续帧、垃圾前缀和非法长度恢复 |
| `protocol_ring_buffer_test` | 协议帧跨两个 Ring Buffer 输入批次时的状态保留 |

测试成功时均输出 `ALL TESTS PASSED`。

## Observability

固件提供以下 RAM 运行时计数，可在 Keil 调试器中观察：

| 变量 | 含义 |
| --- | --- |
| `ping_count` | 已发送的 PING 响应数 |
| `frame_count` | CRC 正确且成功入队的帧数 |
| `crc_error_count` | CRC 校验失败帧数 |
| `queue_drop_count` | Queue 满导致的丢帧数 |
| `comm_alive` | `CommTask` 已处理的 DMA 批次数 |
| `device_alive` | Queue Get 未成功次数，正常情况下主要为等待超时 |

## Design Notes

- Queue 按值传递 `ProtocolFrame_t`，避免 DMA 缓冲区和局部对象的指针生命周期问题。
- Parser 负责组帧与重同步，CRC 校验由 `CommTask` 完成。
- CRC 正确的帧才进入业务 Queue，传输层错误不会进入 `DeviceTask`。
- USART1 由 `DeviceTask` 统一发送业务响应，无需额外 TX Mutex。
- TX DMA 已由 CubeMX 配置，当前 PING 响应仍使用阻塞式 `HAL_UART_Transmit()`。

## Known Limitations

- Normal RX DMA 在 IDLE 或 Transfer Complete 后需要由 `CommTask` 重新启动，重启前存在接收盲窗。
- `CommTask` 通过 1 ms 轮询 ready 标志处理接收批次。
- 响应发送为阻塞模式，尚未使用已配置的 TX DMA。
- 协议目前没有 ACK、重传、事务序号或通用 Error Response。
- UART/DMA 自动恢复尚未实现；部分 HAL 错误会进入 `Error_Handler()`。
- PING Handler 当前按 CMD 识别请求，没有额外校验 `LEN = 0`。

## Roadmap

- `DeviceState` 与 `DeviceManager` 状态管理；
- `CMD_SET_MODE`、`CMD_GET_STATUS` 和 `CMD_GET_STATS` 命令集；
- 参数校验、未知命令处理和统一 Error Response；
- 覆盖 `SET → GET` 状态一致性的 Python 硬件测试。

## License

仓库根目录目前没有项目级 `LICENSE`。STM32 HAL、CMSIS 和 FreeRTOS 等第三方代码分别保留其原始许可证。
