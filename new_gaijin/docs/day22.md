# Day 22 - ng_timer计时服务

**日期**: ____/____/________
**预计时长**: 2.5 小时
**模块重点**: TIM2 / 毫秒计数器 / 非阻塞延时

---

> ⚠️ **现状说明（盲写后对照）**：本模块已有参考实现 `src/ng_timer.c`，头文件 `include/ng_timer.h`。今天**先不要打开 `src/ng_timer.c`**。流程：
> 1. **盲写**：在 `new_gaijin/practice/day22/` 下新建你自己的 `ng_timer.c`，接口以 `include/ng_timer.h` 为准；
> 2. **对照**：写完后打开 `src/ng_timer.c` 逐函数对比，重点看 TIM2 初始化的寄存器/库函数配置顺序、中断处理函数、以及"为什么不用 SysTick"；
> 3. **验证**：本模块直接包含 `stm32f10x.h`，**PC 端 gcc 无法编译**。验证方式改为：对照后把参考实现加入 Keil 工程编译（Keil MDK-Lite 免费，**编译不需要硬件**）；有开发板则烧录用 LED 闪烁间隔验证 1ms 精度。
>
> 从今天起进入第 4 周：模块开始依赖硬件寄存器，验证方式从"跑 PC 测试"变为"对照 + Keil 编译（+ 有硬件则烧录）"。无硬件的替代路径见 `学习方案.md` 第十一节。
>
> **不要修改 `src/`、`include/`、`adapter/` 下的参考实现。**

## 今日目标

今天结束时，你应该能够不看参考实现，独立写出基于TIM2的毫秒计数器和非阻塞延时接口，理解为什么它能替代阻塞式 `delay_ms()`，并能说出你的实现与参考实现的差异。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `improve/firmware/stm32/USER/main.c` | 第44-58行 | get_tick_count()函数和sys_tick_ms说明 |
| `stm32GPS定位模块/SYSTEM/delay/delay.c` | 全文 | 原版延时函数实现 |
| `new_gaijin/include/ng_timer.h` | 全文 | 接口声明——盲写必须遵守 |
| `improve/项目不足与改进方案.md` | 第238-240行 | P2-1重构延时机制方案 |
| `new_gaijin/src/ng_timer.c` | 全文 | **盲写完成后**再读，用于对照 |

## 知识要点

- **TIM2定时器**：STM32的通用定时器，配置为1ms中断，避免与原delay.c的SysTick冲突
- **毫秒计数器实现**：
  ```c
  volatile uint32_t sys_tick_ms = 0;
  
  void TIM2_IRQHandler(void) {
      if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
          sys_tick_ms++;
          TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
      }
  }
  
  uint32_t ng_timer_millis(void) {
      return sys_tick_ms;
  }
  ```
- **非阻塞延时**：
  ```c
  uint32_t start = ng_timer_millis();
  while (ng_timer_millis() - start < 1000) {
      // 可以做其他事情，不阻塞
  }
  ```
- **与delay_ms()的区别**：
  - delay_ms()：阻塞等待，期间无法做任何事
  - ng_timer：非阻塞，可以在等待期间处理其他任务
- **盲写时要想清楚的问题**：计数器溢出（约 49.7 天）时 `now - start` 为什么仍然正确？`volatile` 为什么必须加？

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 阅读delay.c，理解原版延时函数实现 | [ ] | SysTick配置方式 |
| 2 | 阅读 `include/ng_timer.h`，明确要实现哪些函数 | [ ] | 只看头文件，不看.c |
| 3 | 在 `practice/day22/` 盲写 `ng_timer.c`：TIM2初始化 + 中断处理 + millis + 非阻塞判断 | [ ] | 参考知识要点，查 STM32 标准库函数名 |
| 4 | 打开 `src/ng_timer.c` 对照：预分频/重装值怎么算出1ms、中断优先级、是否处理了溢出 | [ ] | 差异记入下方问题表 |
| 5 | 在Keil工程中把**参考实现** `src/ng_timer.c` 加入并编译通过 | [ ] | 无需烧录；为 Day26 集成预热 |
| 6 | （有硬件）烧录后用 `ng_timer_millis()` 控制LED每500ms翻转，秒表核对 | [ ] | 无硬件标记 `[H]` |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] `practice/day22/ng_timer.c`（自己的盲写实现）
- [ ] 与参考实现的差异对照笔记
- [ ] 参考实现在 Keil 中编译通过
- [ ] 毫秒计数器验证通过（无硬件标记 `[H]`）

## 自检清单

- [ ] 我能解释TIM2定时器的工作原理
- [ ] 我知道非阻塞延时和阻塞延时的区别
- [ ] 我能解释 uint32 溢出后时间差计算为何仍正确
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解时间基准是嵌入式系统的基础设施

## 明日预告

**Day23** 盲写 ng_error 错误处理模块（纯 C，可在 PC 上测试），并对照 HAL 适配层理解函数指针解耦。
