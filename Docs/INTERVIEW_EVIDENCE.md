# 面试证据索引：智能送药小车

总讲义见 [模块化五项目面试讲义](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/tree/main/docs/interview-handbook)，本项目重点对应 [ARM/FreeRTOS](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/docs/interview-handbook/02-arm-freertos.md)、[P2 项目故事](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/docs/interview-handbook/06-project-stories.md#p2智能送药小车) 与 [P2 实验](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/docs/interview-handbook/09-experiments.md#3-p2非阻塞状态机与-tick-回绕)。固定证据标签为本仓库 `study-step-2-nonblocking-fsm`；总讲义固定标签为 `study-step-7-detailed-handbook`。

| 常见问题 | 代码证据 | 工程回答 |
|---|---|---|
| 中断里应该做什么？ | `Core/stm32f10x_it.c` | USART ISR 只完成两字节组帧和队列通知；病房匹配、转向决策留在任务上下文 |
| 为什么用长度为 1 的队列？ | `Core/main.c` | OpenMV 帧和重量都是“最新值”语义，`xQueueOverwrite` 明确表达丢旧保新 |
| 多任务能否同时控制电机？ | `vControlTask` | 电机外设只有控制任务一个写入者，逻辑任务发布命令，消除竞争和最后写入者不确定性 |
| 如何避免状态机阻塞？ | `APP/cart_control/cart_state.c` | 转向、提示灯和掉头都由时间戳推进；没有长阻塞延时或无限队列等待 |
| Tick 回绕怎么办？ | `prvElapsed` 和主机测试 | 使用无符号减法比较持续时间，并覆盖 `UINT32_MAX` 回绕测试 |
| 传感器断线怎么办？ | `APP/hx711/hx711.c` | DOUT 等待有 100 ms 超时，错误沿调用链返回，不会永久卡死传感器任务 |
| 怎么发现资源创建失败？ | `Core/main.c`、`FreeRTOSConfig.h` | 队列与任务创建后断言，启用 malloc-failed 与 level-2 栈溢出 hook |
| OLED 在调度器启动前为何可能卡住？ | `BSP/delay/delay_timer.c`、MDK 工程文件 | 原实现读取尚未启动的 SysTick；现在先启 TIM2 1 MHz 计数器，微秒延时不依赖 RTOS tick；ARMCC 固件已完整链接，实板时序仍待测 |

## 验证边界

主机 Debug/Release 测试验证状态转移、输出命令和 Tick 回绕；Keil ARMCC 5.06u6 完整构建已生成 AXF/HEX，日志 0 错误、0 警告。修正从大容量芯片遗留的目标内存设置后，map 显示 Flash 使用 `0x4EF8`、上限 `0x10000`，RAM 使用 `0x2808`、上限 `0x5000`，与 STM32F103C8 的 64 KB / 20 KB 边界一致。实板时序仍待验证。没有实车时，不声称已经验证 PID 参数、路口光学阈值、转向时长或 HX711 标定系数。
