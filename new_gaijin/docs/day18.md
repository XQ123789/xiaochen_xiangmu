# Day 18 - ng_gps_parser模块

**日期**: ____/____/________
**预计时长**: 3 小时
**模块重点**: GPS解析 / NMEA格式 / sscanf

---

> ⚠️ **现状说明（盲写后对照）**：本模块已有参考实现 `src/ng_gps_parser.c`，头文件 `include/ng_gps_parser.h`。今天**先不要打开 `src/ng_gps_parser.c`**。流程：
> 1. **盲写**：在 `new_gaijin/practice/day18/` 下新建你自己的 `ng_gps_parser.c`，接口和数据结构以 `include/ng_gps_parser.h` 为准；
> 2. **对照**：写完后打开 `src/ng_gps_parser.c` 逐函数对比，把差异记入下方问题表；
> 3. **验证**：在 `test/` 下把编译命令中的 `../src/ng_gps_parser.c` 换成 `../practice/day18/ng_gps_parser.c`，跑 `test_all`。
>
> **不要修改 `src/` 和 `include/` 下的参考实现。**

## 今日目标

今天结束时，你应该能够不看参考实现，独立写出解析BC20模块 `+QGPSLOC` 响应的模块，提取经纬度、海拔、速度、方向信息，并通过已有测试。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `stm32GPS定位模块/USER/main.c` | 第49-67行 | 原版GPS解析逻辑（sscanf） |
| `improve/firmware/stm32/USER/main.c` | 第144-173行 | 改进版GPS解析逻辑 |
| `new_gaijin/include/ng_gps_parser.h` | 全文 | 接口与结构体定义——盲写必须遵守 |
| `learn/02-代码阅读指南.md` | 第99-115行 | sscanf解析格式说明 |
| `new_gaijin/src/ng_gps_parser.c` | 全文 | **盲写完成后**再读，用于对照 |

## 知识要点

- **GPS响应格式**：
  ```
  +QGPSLOC: UTC,latitude,longitude,alt,unused,speed,direction,...
  ```
  示例：`+QGPSLOC: 081234.00,3990.7123,N,11623.4567,E,50.2,0.5,120.3`
- **解析方法**：使用 `sscanf` 跳过不需要的字段
  ```c
  sscanf(gps_start + 9, "%*[^,],%lf,%lf,%*f,%lf,%*[^,],%lf,%lf",
         &lat, &lon, &alt, &speed, &dir);
  ```
  - `%*[^,]`：跳过字段（读取但不存储）
  - `%lf`：读取double值
- **模块设计**：
  - 输入：GPS响应字符串
  - 输出：解析后的结构体（lat, lon, alt, speed, dir, valid）
  - 错误处理：解析失败返回valid=0

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 理解原版GPS解析逻辑（main.c第49-67行） | [ ] | sscanf格式字符串含义 |
| 2 | 阅读 `include/ng_gps_parser.h`，明确结构体字段和函数签名 | [ ] | 只看头文件，不看.c |
| 3 | 在 `practice/day18/` 盲写 `ng_gps_parser.c` | [ ] | 先想清楚：NULL、找不到前缀、字段不足各怎么处理 |
| 4 | 用gcc把你的实现接入 `test_all` 编译运行 | [ ] | 替换 `../src/ng_gps_parser.c` 为你的文件路径 |
| 5 | 打开 `src/ng_gps_parser.c` 对照：前缀查找方式、sscanf返回值检查、valid 标志的设置时机 | [ ] | 差异记入下方问题表 |
| 6 | 若测试未全过，根据对照结果修正自己的实现直至通过 | [ ] | 不要直接复制参考代码 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] `practice/day18/ng_gps_parser.c`（自己的盲写实现）
- [ ] 接入 `test_all` 后GPS解析套件全部通过
- [ ] 与参考实现的差异对照笔记
- [ ] 理解sscanf解析GPS数据的原理

## 自检清单

- [ ] 我能解释GPS NMEA数据格式
- [ ] 我知道sscanf中%*[^,]的含义
- [ ] 我的实现通过了已有测试
- [ ] 我能说出参考实现在健壮性上比我多做了什么
- [ ] 我理解GPS解析是定位系统的核心功能

## 明日预告

**Day19** 精读 `test/test_gps_parser.c` 的边界用例设计，并补充遗漏场景。
