# Day 26 - 编译开关集成

**日期**: ____/____/________
**预计时长**: 2.5 小时
**模块重点**: 编译开关 / 新旧代码共存 / 渐进式集成

---

> ⚠️ **现状说明**：今天**不盲写**。8 个模块的参考实现已经齐备，编译开关也已在 `include/ng_config.h` 中定义好——是 **8 个独立开关**（`USE_NEW_COORD`、`USE_NEW_GPS_PARSER`、`USE_NEW_JSON_BUILDER`、`USE_NEW_TIMER`、`USE_NEW_ERROR`、`USE_NEW_LED_STATUS`、`USE_NEW_MQTT`、`USE_NEW_APP`，取值 0/1），**不是**早期设想的单一 `USE_NEW_ALG`。今天的任务是按 `new_gaijin/README.md` 第五节的集成指南，把全部参考实现加入 Keil 工程，并验证"全关"和"全开（除 APP）"两种配置都能编译通过。
>
> **无硬件**：本日全部任务只需 Keil 编译，不需要烧录，可以完整完成。

## 今日目标

今天结束时，你应该能够让 Keil 工程在 `ng_config.h` 全部开关为 0 时编译出原版行为、在 7 个开关为 1（`USE_NEW_APP` 保持 0）时编译出 new_gaijin 版本，两种配置均无错误无警告，并能解释为什么用 8 个独立开关而不是 1 个总开关。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `new_gaijin/README.md` | 第三节、第五节 | 开关说明、Keil 集成步骤（源文件清单、Include 路径、宏定义） |
| `new_gaijin/include/ng_config.h` | 全文 | 8 个开关的定义与注释，以及其它配置参数 |
| `new_gaijin/adapter/ng_hal_stm32.c` | 全文 | 它依赖哪些原版驱动头文件——决定 Include 路径要加什么 |
| `improve/firmware/stm32/USER/main.c` | 全文 | 改进版main.c（新版基础） |
| `stm32GPS定位模块/USER/main.c` | 全文 | 原版main.c（保留作为回退） |

## 知识要点

- **8 个独立开关的设计意图**：
  - 每个模块可以单独启用/回退，出问题时能精确定位到是哪个模块引入的
  - 允许渐进式集成：先开 `USE_NEW_COORD` 一个，编译烧录没问题再开下一个
  - `USE_NEW_APP` 默认为 0：顶层状态机替换风险最大，必须等其余 7 个模块稳定后才开
- **开关的使用方式**（以 `#if` 判断值，不是 `#ifdef`）：
  ```c
  #if USE_NEW_COORD
      ng_coord_wgs84_to_gcj02(...);   // 新模块
  #else
      wgs84_to_amap(...);             // led.c 中的原版函数
  #endif
  ```
- **Keil配置两种方式**：
  - 方式1：直接改 `ng_config.h` 中的 0/1
  - 方式2：Project Options → C/C++ → Preprocessor Symbols → Define 中添加 `USE_NEW_COORD=1,...`（会覆盖头文件默认值，需头文件配合 `#ifndef` 保护）
- **集成验证步骤**：
  1. 全部开关设 0 → 编译 → 应成功（等价于原版）
  2. 7 个开关设 1、`USE_NEW_APP=0` → 编译 → 应成功
  3. 只开 `USE_NEW_COORD` → 编译 → 应成功（验证单模块可独立启用）
- **注意事项**：
  - 确保新旧代码没有符号冲突（函数名、变量名）；new_gaijin 模块统一 `ng_` 前缀
  - `adapter/ng_hal_stm32.c` 依赖原版 `usart.h / wdg.h / delay.h / led.h / 4G.h / dht11.h`，Include 路径必须包含这些目录

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 备份 Keil 工程（复制整个工程目录） | [ ] | 出问题可回退 |
| 2 | 按 README 5.1 在 Keil 新建 `new_gaijin` Group，加入 8 个 `src/*.c` + `adapter/ng_hal_stm32.c` | [ ] | 共 9 个源文件 |
| 3 | 按 README 5.2 添加 Include 路径 `new_gaijin/include`、`new_gaijin/adapter` | [ ] | 编译报"找不到头文件"多半是这一步漏了 |
| 4 | `ng_config.h` 全部开关设 0，编译 | [ ] | 记录警告/错误并解决 |
| 5 | 7 个开关设 1（APP 保持 0），编译 | [ ] | 记录 Code/RO/RW/ZI 大小 |
| 6 | 只开 `USE_NEW_COORD`，编译 | [ ] | 验证单模块独立启用 |
| 7 | 对比三种配置的编译输出大小，写一段话解释差异来源 | [ ] | 记入笔记 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] Keil 工程已包含全部 new_gaijin 源文件与 Include 路径
- [ ] 三种开关配置（全关 / 7开 / 仅COORD）均编译通过
- [ ] 三种配置的代码体积对比记录

## 自检清单

- [ ] 我能解释编译开关的作用，以及为什么用 8 个独立开关
- [ ] 我知道 `#if USE_NEW_X` 与 `#ifdef` 的区别
- [ ] 我知道如何在Keil中配置 Include 路径和预处理宏
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解编译开关是渐进式重构的安全保障

## 明日预告

**Day27** 盲写 ng_app 顶层FSM的状态机骨架，再对照 488 行的参考实现，理解它如何协调全部 ng_ 模块。
