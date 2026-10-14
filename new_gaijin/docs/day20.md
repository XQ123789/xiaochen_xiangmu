# Day 20 - ng_json_builder模块

**日期**: ____/____/________
**预计时长**: 3 小时
**模块重点**: JSON构建 / 缓冲区保护 / sprintf

---

> ⚠️ **现状说明（盲写后对照）**：本模块已有参考实现 `src/ng_json_builder.c`，头文件 `include/ng_json_builder.h`。今天**先不要打开 `src/ng_json_builder.c`**。流程：
> 1. **盲写**：在 `new_gaijin/practice/day20/` 下新建你自己的 `ng_json_builder.c`，接口以 `include/ng_json_builder.h` 为准；
> 2. **对照**：写完后打开 `src/ng_json_builder.c` 逐函数对比，重点看缓冲区保护的写法；
> 3. **验证**：在 `test/` 下把编译命令中的 `../src/ng_json_builder.c` 换成 `../practice/day20/ng_json_builder.c`，跑 `test_all`。
>
> **不要修改 `src/` 和 `include/` 下的参考实现。**

## 今日目标

今天结束时，你应该能够不看参考实现，独立写出生成OneNET JSON格式、带缓冲区溢出保护的构建模块，并通过已有测试。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `stm32GPS定位模块/HARDWARE/4G/4G.c` | 第200-272行 | 原版MakeOnenetPayload()和JSON生成 |
| `improve/firmware/stm32/HARDWARE/4G/bc20_improved.c` | 第200-300行 | 改进版JSON生成逻辑 |
| `improve/firmware/stm32/config.h` | 第38行 | JSON_BUF_SIZE定义 |
| `new_gaijin/include/ng_json_builder.h` | 全文 | 接口声明——盲写必须遵守 |
| `new_gaijin/src/ng_json_builder.c` | 全文 | **盲写完成后**再读，用于对照 |

## 知识要点

- **OneNET JSON格式**：
  ```json
  {"datastreams":[{"id":"temperature","datapoints":[{"value":25}]},
                  {"id":"humidity","datapoints":[{"value":60}]},
                  {"id":"latitude","datapoints":[{"value":39.91}]},
                  {"id":"longitude","datapoints":[{"value":116.40}]},
                  {"id":"altitude","datapoints":[{"value":50.2}]},
                  {"id":"speed","datapoints":[{"value":0.5}]},
                  {"id":"direction","datapoints":[{"value":120.3}]}]}
  ```
- **缓冲区保护**：
  - 使用`snprintf()`替代`sprintf()`，防止缓冲区溢出
  - 检查返回值，确保数据完整写入
  - 缓冲区大小由`JSON_BUF_SIZE`（256字节）控制
- **模块化设计**：
  - 输入：温度、湿度、经纬度、海拔、速度、方向
  - 输出：JSON字符串（写入调用者提供的缓冲区）
  - 返回值：0=成功，1=缓冲区不足

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 阅读原版和改进版的JSON生成代码 | [ ] | 理解OneNET数据格式 |
| 2 | 阅读 `include/ng_json_builder.h`，明确输入参数和返回值约定 | [ ] | 只看头文件，不看.c |
| 3 | 在 `practice/day20/` 盲写 `ng_json_builder.c`，用snprintf并检查返回值 | [ ] | 想清楚：缓冲区刚好差1字节时怎么判定 |
| 4 | 用gcc把你的实现接入 `test_all` 编译运行 | [ ] | 替换 `../src/ng_json_builder.c` 为你的文件路径 |
| 5 | 打开 `src/ng_json_builder.c` 对照：snprintf返回值如何判定溢出、GPS无效时字段如何处理 | [ ] | 差异记入下方问题表 |
| 6 | 若测试未全过，根据对照结果修正自己的实现直至通过 | [ ] | 不要直接复制参考代码 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] `practice/day20/ng_json_builder.c`（自己的盲写实现）
- [ ] 接入 `test_all` 后JSON套件全部通过
- [ ] 与参考实现的差异对照笔记
- [ ] 理解snprintf缓冲区保护机制

## 自检清单

- [ ] 我能描述OneNET JSON数据格式
- [ ] 我知道snprintf和sprintf的区别
- [ ] 我的实现通过了已有测试
- [ ] 我能说出参考实现的溢出判定逻辑与我的差异
- [ ] 我理解缓冲区保护是防止安全漏洞的关键

## 明日预告

**Day21** 精读 `test/test_json_builder.c`，跑通三模块全部测试，并进行第3周回顾。
