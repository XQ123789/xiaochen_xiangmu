# Day 23 - ng_error错误处理 + HAL适配层

**日期**: ____/____/________
**预计时长**: 3 小时
**模块重点**: 错误计数 / 分级恢复 / 函数指针适配

---

> ⚠️ **现状说明（盲写后对照）**：`src/ng_error.c`、`adapter/ng_hal.h`、`adapter/ng_hal_stm32.c` 均已存在。今天分两部分：
> - **ng_error 盲写**：它是**纯 C 模块**（只依赖 `string.h`），在 `practice/day23/` 盲写后可直接在 PC 上 `gcc -c -I../include ng_error.c` 编译，甚至写个小测试；
> - **HAL 适配层对照阅读**：`ng_hal.h` 定义接口、`ng_hal_stm32.c` 是 STM32 端实现（依赖 usart/wdg/led/4G 等原版驱动，只能 Keil 编译）。这部分**不盲写**，重点是读懂"算法模块如何通过函数指针间接调用硬件"。
>
> **不要修改 `src/`、`include/`、`adapter/` 下的参考实现。**

## 今日目标

今天结束时，你应该能够不看参考实现，独立写出错误计数与分级阈值判断模块并在 PC 上编译通过；能画出 `ng_hal.h` 中接口结构体、`ng_hal_stm32.c` 实现、以及调用方（如 `ng_mqtt_wrapper`）三者的关系图。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `improve/firmware/stm32/USER/main.c` | 第30-36行 | 原版错误计数变量 |
| `improve/进阶改进与扩展规划.md` | 第455-466行 | 分级错误恢复设计（L1-L6） |
| `new_gaijin/include/ng_error.h` | 全文 | 接口声明——盲写必须遵守 |
| `improve/进阶改进与扩展规划.md` | 第486-493行 | HAL函数指针设计 |
| `new_gaijin/adapter/ng_hal.h` | 全文 | HAL 接口定义（对照阅读） |
| `new_gaijin/adapter/ng_hal_stm32.c` | 全文 | STM32 端实现：每个函数指针指向哪个原版驱动函数 |
| `new_gaijin/src/ng_error.c` | 全文 | **盲写完成后**再读，用于对照 |

## 知识要点

- **ng_error模块功能**：
  - 错误计数器：记录各类错误发生次数
  - 阈值判断：超过阈值触发恢复流程
  - 分级恢复：L1重试→L2清空重试→L3硬件复位→L4系统复位
- **HAL适配层设计**：
  ```c
  typedef struct {
      void (*init)(uint32_t baud);
      void (*send)(const char *data, uint16_t len);
      uint8_t (*available)(void);
      uint8_t (*read)(char *buf, uint16_t max_len);
  } ng_hal_driver_t;
  ```
  （以上为示意，实际字段以 `adapter/ng_hal.h` 为准）
- **函数指针适配**：
  - PC端：使用mock函数（printf输出）
  - STM32端：使用真实UART函数（Uart2_SendStr等）
- **设计优势**：
  - 算法模块不直接依赖硬件
  - PC端和STM32端共用同一套算法代码
  - 更换硬件只需修改HAL适配层

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 阅读 `include/ng_error.h`，明确错误类型枚举、计数结构体、函数签名 | [ ] | 只看头文件，不看.c |
| 2 | 在 `practice/day23/` 盲写 `ng_error.c`：计数、阈值判断、恢复等级推进、复位计数 | [ ] | `gcc -c -I../include ng_error.c` 编译通过 |
| 3 | 写一个 `practice/day23/test_error.c`：模拟连续失败 N 次，验证等级从 L1 逐级上升 | [ ] | 纯 C，PC 上可直接跑 |
| 4 | 打开 `src/ng_error.c` 对照：等级阈值怎么定义、成功后是否清零、有没有上限保护 | [ ] | 差异记入下方问题表 |
| 5 | 精读 `adapter/ng_hal.h` + `ng_hal_stm32.c`，画出"接口结构体 → STM32 实现 → 调用方"关系图 | [ ] | 标出每个函数指针最终调用的原版驱动函数 |
| 6 | 在Keil工程中把 `src/ng_error.c` 和 `adapter/ng_hal_stm32.c` 加入并编译通过 | [ ] | 无需烧录 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] `practice/day23/ng_error.c` + `test_error.c`（PC 上跑通）
- [ ] 与参考实现的差异对照笔记
- [ ] HAL 三层关系图
- [ ] ng_error 与 HAL 在 Keil 中编译通过

## 自检清单

- [ ] 我能解释分级错误恢复的L1-L6机制
- [ ] 我知道函数指针如何实现硬件解耦
- [ ] 我能指出 `ng_hal_stm32.c` 里每个接口对应的原版驱动函数
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解HAL层是跨平台代码复用的关键

## 明日预告

**Day24** 盲写 ng_led_status 非阻塞LED状态指示的框架，再对照参考实现的完整 8 种模式。
