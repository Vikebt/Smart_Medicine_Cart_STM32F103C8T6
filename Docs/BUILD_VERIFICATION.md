# 构建验证记录

| 项目 | 验证日期 | 工具链 | 结果 | 程序资源占用 |
| --- | --- | --- | --- |
| Smart Medicine Delivery Cart | 2026-10-08 | Keil MDK / ARMCC 5.06 update 6 | 0 Error(s), 0 Warning(s) | Code 17696 B, RO-data 2340 B, RW-data 180 B, ZI-data 10068 B |

验证工程：`Project/MDK-ARM/Fire_FreeRTOS.uvprojx`，目标：`Fire_FreeRTOS`。

本次采用 Keil 命令行全量构建并完成链接与 HEX 生成。构建产物仅用于验证，已在提交前清理；`.gitignore` 会阻止其再次进入版本库。

链接 map 显示 `LR_IROM1` 使用 `0x4EF8 / 0x10000`，`RW_IRAM1` 使用 `0x2808 / 0x5000`，与 STM32F103C8 的 64 KB Flash 和 20 KB SRAM 边界一致。
