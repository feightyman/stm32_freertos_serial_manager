# STM32 + FreeRTOS 串口通信与设备管理系统

这是一个用于 MCU 嵌入式求职展示的 STM32 + FreeRTOS 项目，目标是实现一条可靠、可测试、可解释的串口通信链路。

目标链路：

```text
PC Python Test Tool
        ↓ UART
STM32 UART → DMA + IDLE → Ring Buffer → Protocol Parser
        → CRC16 校验 → FreeRTOS Queue → Device Task → Response
```

## 当前进度

**全链路已打通并验收**：

```text
PC Python Test Tool
→ CH340 USB-TTL
→ STM32 USART1 RX
→ DMA + IDLE 不定长接收
→ Ring Buffer
→ Streaming Protocol Parser（含 CRC 字节消费）
→ CRC16 校验（CommTask）
→ FreeRTOS Queue（按值传递 ProtocolFrame_t）
→ DeviceTask（命令分发）
→ PING Response（UART TX）
```

## 协议规格

```text
SOF | LEN | CMD | DATA | CRC_H | CRC_L
```

| 字段 | 说明 |
| --- | --- |
| SOF | 固定 `0xAA` |
| LEN | DATA 字节数，允许 0，最大 32（不含 CMD） |
| CMD | 命令字 |
| DATA | 负载数据，长度由 LEN 决定 |
| CRC_H / CRC_L | CRC16 高字节在前（先发高字节） |

CRC 参数：

- 算法：CRC-16/CCITT-FALSE
- Poly `0x1021`，Init `0xFFFF`，无反射，XorOut `0x0000`
- 标准测试向量：`"123456789"` → `0x29B1`
- 计算范围：`LEN | CMD | DATA`（不含 SOF 和 CRC 自身）
- 校验位置：CommTask（Parser 只负责组帧，不判断 CRC 对错）

## 命令表

| 命令 | 值 | 方向 | 说明 |
| --- | --- | --- | --- |
| CMD_PING | `0x01` | PC → MCU | 心跳探测 |
| CMD_PING_RESP | `0x81` | MCU → PC | 响应帧（bit7 置位表示响应，`0x01 \| 0x80`） |

PING 请求帧：`AA 00 01 0D 2E`，响应帧：`AA 00 81 9C A6`。

## 已验证

- Python 可以打开串口、发送协议帧、接收响应。
- DMA + IDLE 不定长接收，跨 IDLE 批次保留半包状态。
- Ring Buffer：空读、正常读写、写满拒绝、head/tail 回绕。
- Parser：完整帧、半包、连续两帧、Garbage + 合法帧、非法 LEN 后重同步。
- CRC16：标准向量 `0x29B1`、帧向量 `0xF69E` / `0x0D2E` 等。
- 坏 CRC 帧：不响应、不计入正常帧数（端到端验证）。
- PING 往返：5 次连续请求均收到 `AA 00 81 9C A6`。
- 垃圾数据 + 合法帧：重同步后正常响应。

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
│   ├── Comm/              # Ring Buffer、Protocol Parser、CRC16 及独立测试
│   ├── Common/            # 公共配置与类型
│   ├── Core/              # CubeMX 生成代码与 FreeRTOS 任务
│   ├── Device/            # 设备管理模块骨架
│   ├── Drivers/           # STM32 HAL 与 CMSIS
│   ├── Middlewares/       # FreeRTOS
│   ├── MDK-ARM/           # Keil 工程
│   └── stm32_freertos_serial_manager.ioc
├── tools/
│   └── uart_ping_test.py  # Python 协议帧测试
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

## Python PING 测试

安装依赖：

```powershell
python -m pip install pyserial
```

确认没有其他串口工具占用端口，然后在仓库根目录运行：

```powershell
python tools/uart_ping_test.py COM7
```

`COM7` 需要替换为实际的 CH340 串口号。测试覆盖三组：

1. 合法 PING ×5，逐字节比对响应 `AA 00 81 9C A6`；
2. 坏 CRC 帧 ×2，验证无响应（坏帧被丢弃）；
3. 垃圾数据 + 合法 PING，验证错误恢复。

全部通过时输出：

```text
PING 1: PASS
PING 2: PASS
PING 3: PASS
PING 4: PASS
PING 5: PASS
BAD CRC 1: PASS (no response)
BAD CRC 2: PASS (no response)
RESYNC: PASS
ALL TESTS PASSED
```

## 独立测试（PC，无需硬件）

在 `firmware\Comm` 目录运行：

```powershell
# CRC16 算法测试
gcc -std=c99 -Wall -Wextra -Werror crc16_test.c crc16.c -o crc16_test.exe
.\crc16_test.exe

# Parser 测试（含 CRC 字节消费）
gcc -std=c99 -Wall -Wextra -Werror protocol_test.c protocol.c -o protocol_test.exe
.\protocol_test.exe

# Ring Buffer + Parser 跨批次集成测试
gcc -std=c99 -Wall -Wextra -Werror protocol_ring_buffer_test.c protocol.c ring_buffer.c -o protocol_ring_buffer_test.exe
.\protocol_ring_buffer_test.exe
```

均输出 `ALL TESTS PASSED`。

## 当前实现说明

### 接收路径（CommTask）

- RX 使用 `HAL_UARTEx_ReceiveToIdle_DMA()`，Normal 模式，缓冲区 128 字节。
- Callback 获取有效长度并通知 CommTask。
- CommTask 将 DMA 数据写入 Ring Buffer（256 字节，满则拒绝），再逐字节读出送入 Parser。
- Parser 输出完整帧（含 CRC 两个字节）后，CommTask 计算 `LEN|CMD|DATA` 的 CRC 并与帧内 CRC 比对：通过则按值入队，失败则丢帧并计数。
- 队列满时非阻塞丢弃并计数，不阻塞接收路径。

### 发送路径（DeviceTask）

- `osMessageQueueGet` 等待命令帧（1000 ms 超时，超时则维持心跳计数）。
- 收到 `CMD_PING` 构建响应帧 `SOF|LEN|CMD|CRC_H|CRC_L`，阻塞式 `HAL_UART_Transmit` 发送。
- 全系统只有一个写者（DeviceTask）写 USART1，因此无需 Mutex 也不会互相打断。

### 关键设计决策

- **Queue 按值传递**：`ProtocolFrame_t` 为 36 字节定长结构（全 `uint8_t` 无 padding），入队即拷贝，无指针生命周期问题。
- **Parser 与 CRC 分离**：Parser 只负责组帧与重同步，CRC 校验在 CommTask —— 坏帧不会进入队列，职责单一、易调试。
- **任务上下文使用普通队列 API**：Parser 在 CommTask 内运行，入队用 `osMessageQueuePut`（非 FromISR 版），语义正确。
- **CRC_H 在前**：发送与接收同一字节序约定，`ProtocolFrame_CrcOk` 与响应构建共用同一计算范围规则（跳过 SOF、不含 CRC 自身）。

## 当前限制（技术债）

- Normal DMA blind window：IDLE 事件到 CommTask 重新启动 DMA 之间到达的字节会丢失（HAL 在 IDLE 时停止 DMA）。当前测试节奏下未触发，后续如需消除需改 DMA 架构。
- 无 TX DMA，响应使用阻塞发送（任务上下文可接受）。
- Ring Buffer 满时本批剩余数据被拒绝（仅计数）。
- 队列满时丢帧（仅计数），无重传机制。

## 常见问题

### 串口可以打开，但 PING 无响应

依次检查：

1. USB-TTL TX 是否连接 PA10，RX 是否连接 PA9，是否共地。
2. 串口参数是否为 115200、8N1、无流控。
3. COM 口是否被其他程序占用。
4. 发送的帧是否带正确 CRC（可用 `tools/uart_ping_test.py` 自动构造）。
5. Keil 调试会话是否处于暂停状态（暂停时 DMA 照收但任务不处理，恢复后可能补处理积压数据）。

### PING 偶发失败

- 检查是否发送过快：帧之间留出响应时间（115200 下响应约 0.5 ms，1 s 超时足够）。
- 若怀疑 DMA blind window 丢字节，降低发送频率或改为整帧单次发送后等待响应。

## 下一阶段

当前链路已全部打通并验收：

```text
UART DMA + IDLE → Ring Buffer → Parser → CRC16 → Queue → Device Task → Response
```

**停止新增大型功能，进入测试和面试阶段**：

- 稳定性测试：长时间连续 PING、异常输入注入、断电重启回归。
- 面试讲稿：能解释每个模块的设计决策与 trade-off（见"关键设计决策"）。
- Debug 故事：sizeof 长度 Bug、CRC 字节序窄化、调试器暂停假失败。
