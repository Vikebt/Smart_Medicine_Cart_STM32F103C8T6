# 项目二：智能送药小车 — STAR 完整梳理

> **代码基线**：STM32F103C8T6 + STM32 StdPeriph + FreeRTOS  
> **最后校准**：2026-08-30；本文区分“当前源码可验证实现”和“后续设计目标”，不以设计稿替代源码事实。

## 一、项目基本信息

| 项目项 | 内容 |
| --- | --- |
| 项目名称 | 基于嵌入式视觉的智能送药小车 |
| 项目来源 | 个人主导的竞赛原型；项目资料记录关联 2021 年全国大学生电子设计竞赛 F 题 |
| 项目成果 | 项目资料记录为完成实物原型并获省级一等奖 |
| 本人角色 | 项目负责人：主导硬件设计、FreeRTOS 任务划分、电机控制、传感器联调与整机调试 |
| 主控 | STM32F103C8T6，Cortex-M3，72 MHz，64 KB Flash / 20 KB SRAM |
| 软件基础 | STM32 StdPeriph、FreeRTOS v9、Keil MDK / ARMCC |

> 历史竞赛成果来自项目资料；本次代码整理仅验证当前工程能否编译、链接及其软件架构，不重新宣称赛道性能或硬件测试数据。

## 二、Situation（背景）

项目针对配送赛道中的自主循迹、路口病房号识别、投递判定和返回流程设计。难点在于电机控制需要更高频率执行，而称重、显示和串口输入不应阻塞控制环；因此采用 OpenMV 与 STM32 分工、FreeRTOS 多任务协同的方式组织原型。

## 三、Task（目标）

1. 用灰度传感器获得循迹偏差，以 PID 驱动差速 PWM 电机控制。
2. 通过 OpenMV 的串口输入接收病房号/控制信息，并交由业务逻辑任务处理。
3. 通过 HX711 获取重量信息，用于配送状态机的装载/投递判断。
4. 在 C8T6 的资源限制下，使实时控制、状态逻辑和低频传感器/显示任务可并发运行。

## 四、Action（当前源码实现）

### 4.1 系统数据流

```text
OpenMV ── USART3 RXNE ISR ──> UartQueue ───┐
7 路灰度 ─────────────────> Control (5 ms) ─> PID ─> PWM / 差速电机
HX711 ─────────────────────> Sensor (100 ms) ─> WeightQueue ─┐
                                                             └─> UI_Logic (20 ms) ─> 配送状态机 / OLED
```

OpenMV 串口采用 USART3（PB10 / PB11）。现有代码以 RXNE 中断接收两字节数据，并通过 `xQueueSendFromISR` 发送到 `UartQueue`；并**未**实现 DMA + IDLE 接收。

### 4.2 FreeRTOS 任务与 IPC

| 任务 | 周期 | 优先级 | 栈深度（创建参数） | 职责 |
| --- | ---: | ---: | ---: | --- |
| Control | 5 ms | 4 | 512 | 灰度偏差、PID、PWM 差速输出 |
| UI_Logic | 20 ms | 3 | 256 | 病房号处理与配送状态机 |
| Sensor | 100 ms | 1 | 128 | HX711 采样和 OLED 刷新 |

| IPC 对象 | 创建参数 | 数据流 |
| --- | --- | --- |
| `UartQueue` | 10 × `uint8_t` | USART3 ISR → UI_Logic |
| `WeightQueue` | 5 × `float` | Sensor → UI_Logic |

控制任务采用 5 ms 周期的调度方式，目的是使 PID 控制频率稳定。实际执行耗时、循迹速度和控制效果仍需要结合真实赛道与电机参数实测，不在当前源码验证中给出数值承诺。

### 4.3 外设与控制模块

| 模块 | 当前代码职责 |
| --- | --- |
| OpenMV | 通过 USART3 向 STM32 输入短帧信息 |
| 灰度传感器 | 提供循迹偏差输入 |
| PID | 将偏差转换为左右轮差速修正量 |
| 电机驱动 | PWM 与方向控制输出 |
| HX711 | 获取重量采样，传递给业务逻辑 |
| OLED | 由 Sensor 任务刷新运行状态 |

现有转向实现是**可标定的定时差速转向**。它与电池电压、轮胎、地面摩擦和电机差异有关，部署前应通过实物重新标定时间和 PID 参数。

### 4.4 统一工程布局

```text
APP/                 电机、PID、灰度、称重、OLED 与业务模块
BSP/                 延时、按键、LED、串口等板级支持
Core/                main、中断、FreeRTOSConfig
Libraries/           CMSIS 与 STM32 StdPeriph
FreeRTOS/            内核、include 与 Cortex-M3 移植层
Project/MDK-ARM/     Keil µVision 工程
Docs/                STAR、架构审查与构建验证记录
```

## 五、Result（交付与验证）

- 当前 Keil 工程为 `Project/MDK-ARM/Fire_FreeRTOS.uvprojx`，目标为 `Fire_FreeRTOS`。
- 使用 Keil MDK 5.26.2 / ARMCC 5.06 update 6 全量编译并链接：**0 Error(s), 0 Warning(s)**。
- 最近一次验证资源占用：Code 17712 B，RO-data 2340 B，RW-data 192 B，ZI-data 10072 B；满足 F103C8T6 的 64 KB Flash / 20 KB SRAM 约束。
- `.gitignore` 已覆盖 Keil 构建目录和目标文件，验证产物未保留在仓库中。

详细记录见 [BUILD_VERIFICATION.md](BUILD_VERIFICATION.md)。

## 六、设计目标与当前边界

下列内容曾出现在早期 STAR 设计稿中，但当前仓库未发现对应驱动、初始化和调用链，故不能描述为已实现：

- MPU6050 角度闭环转向；
- USART3 DMA + IDLE 接收；
- IWDG、HeartbeatQueue 与任务心跳联动；
- 上电自检；
- Flash 参数持久化；
- DWT 执行时间实测数据。

要实现这些目标，需要先在实物上确认 MPU6050 I2C、OpenMV 串口、电机驱动的 PWM/方向脚，以及 PID、角速度和转弯标定参数。相关审查依据见 [ARCHITECTURE_REVIEW.md](ARCHITECTURE_REVIEW.md)。

## 七、面试陈述建议

可以强调“通过优先级将 5 ms PID 控制与 20 ms 状态逻辑、100 ms 称重/显示分离；使用 ISR-to-Queue 和任务间队列解耦视觉输入、称重结果与业务状态”。不要把未在代码中落地的 DMA、IMU 闭环或可靠性机制作为当前实现陈述。

## 八、后续可优化项

1. 接入编码器或 MPU6050 后实现闭环转向，并建立实物标定流程。
2. 将 OpenMV 输入升级为 DMA + IDLE 或带校验的帧协议，增加丢帧统计。
3. 加入 IWDG、任务心跳、自检和 Flash 配置持久化；完成实际赛道上的控制周期、转弯和称重测试记录。
