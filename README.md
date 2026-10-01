# Smart Medicine Delivery Cart · STM32F103C8T6

基于嵌入式视觉与 FreeRTOS 的智能送药小车控制固件。项目来自本人主导的竞赛原型开发：围绕赛道循迹、病房数字识别、配送状态机与称重判定完成系统方案和控制实现，不是从培训模板直接拼装的工程。

> **MCU**：STM32F103C8T6（Cortex-M3，72 MHz，64 KB Flash / 20 KB SRAM）  
> **项目记录**：2021 年全国大学生电子设计竞赛相关作品；STAR 资料记录为省级一等奖。  
> **本人职责**：项目负责人，主导硬件设计、FreeRTOS 任务划分、电机控制、传感器联调与整机调试。

## 项目背景

目标是在无遥控干预下，让小车沿引导线从药房出发，根据视觉端识别的病房数字决定配送方向，完成停靠/投递判定后返回。核心难点是：5 ms 的运动控制不能被显示、称重或串口处理阻塞，同时任务间状态必须可追溯、可扩展。

## 当前可验证实现

```text
OpenMV ── USART3 ISR ─┐
7 路灰度 ────────────┼─> Control (5 ms) ─> PID ─> PWM / 差速电机
HX711 ───────────────┼─> Sensor (100 ms) ─> WeightQueue
                      └─> UI_Logic (20 ms) ─> 配送状态机 / OLED
```

| 任务 | 周期 | 优先级 | 责任 |
| --- | ---: | ---: | --- |
| Control | 5 ms | 4 | 灰度偏差计算、PID、PWM 差速控制 |
| UI_Logic | 20 ms | 3 | 病房号处理、配送状态机、转向决策 |
| Sensor | 100 ms | 1 | HX711 采样、OLED 刷新 |

- `UartQueue`：USART3 RXNE 中断向 `UI_Logic` 传递 OpenMV 数据。
- `WeightQueue`：`Sensor` 任务向 `UI_Logic` 传递称重结果。
- 当前串口接收为两字节 RXNE 处理；转弯为可标定的定时差速转向。

## 技术要点

- 以速率单调的优先级分配保障 200 Hz 控制环优先执行。
- 将循迹 PID、配送状态机与低频称重/显示拆分，避免慢外设拖慢电机控制。
- 集成 OpenMV 串口输入、7 路灰度传感器、HX711、OLED、PWM 电机驱动和差速控制。
- 保留 STM32 StdPeriph + FreeRTOS 的传统裸机/RTOS 工程布局，方便在 Keil 中审阅、移植和继续扩展。

## 工程结构

```text
APP/                 电机、PID、灰度、称重、OLED 和业务逻辑
BSP/                 延时、按键、LED、串口等板级支持
Core/                main、中断、FreeRTOSConfig
Libraries/           CMSIS 与 STM32 StdPeriph
FreeRTOS/            FreeRTOS 内核、include、Cortex-M3 移植层
Project/MDK-ARM/     Keil µVision 工程
Docs/                STAR 文档、架构审查和构建验证
```

## 构建与验证

1. 使用 Keil MDK 5 打开 `Project/MDK-ARM/Fire_FreeRTOS.uvprojx`。
2. 选择 `Fire_FreeRTOS`，执行 **Rebuild all target files**。

已在 Keil MDK 5.26.2 / ARMCC 5.06 update 6 下完成编译和链接验证；资源占用与复核结果见 [构建验证记录](Docs/BUILD_VERIFICATION.md)。`.gitignore` 已排除 Keil 的编译、链接和用户布局文件，仓库可直接用于 GitHub 展示。
