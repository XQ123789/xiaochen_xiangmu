# Day 10 - 配置集中管理

**日期**: ____/____/________
**预计时长**: 2 小时
**模块重点**: config.h / 魔数消除 / 凭据安全

---

## 今日目标

今天结束时，你应该能够列出config.h中每个宏定义的含义，并映射到原项目中对应的硬编码位置。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `improve/firmware/stm32/config.h` | 第1-72行（全文） | 所有配置宏定义 |
| `stm32GPS定位模块/USER/main.c` | 第12-26行 | 原版波特率和看门狗魔数 |
| `stm32GPS定位模块/HARDWARE/4G/4G.c` | 第13-15行 | 原版硬编码凭据 |
| `improve/项目不足与改进方案.md` | 第69-76行 | 魔数泛滥问题分析 |
| `improve/项目不足与改进方案.md` | 第342-363行 | 凭据外部化改进方案 |

## 知识要点

- **config.h分类**：
  - 通信配置：`SERVER_IP`、`SERVER_PORT`、`PRODUCT_KEY`、`DEVICE_NAME`
  - UART配置：`UART_DEBUG_BAUD`、`UART_4G_BAUD`、`UART_PI_BAUD`
  - 缓冲区配置：`UART_BUF_SIZE`、`AT_CMD_BUF_SIZE`、`JSON_BUF_SIZE`
  - 超时配置：`AT_CMD_TIMEOUT_MS`、`GPS_WAIT_MS`、`LOOP_INTERVAL_MS`
  - 传感器配置：`DHT11_READ_INTERVAL`、`DHT11_PIN`
  - 看门狗配置：`WDG_PRESCALE`、`WDG_RELOAD`
  - LED模式：`LED_MODE_INIT`、`LED_MODE_READY`等
- **原版魔数位置**：
  - `main.c`第26行：`IWDG_Init(6, 500)` → `WDG_PRESCALE=6, WDG_RELOAD=500`
  - `main.c`第78行：`delay_ms(1000)` → `LOOP_INTERVAL_MS=1000`
  - `4G.c`第13行：`PRODUCEKEY "your_product_id"` → `PRODUCT_KEY`
- **安全改进**：凭据从源码移至config.h，配合.gitignore防止泄露

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 精读config.h全文，列出每个宏定义的含义 | [ ] | 制作配置项对照表 |
| 2 | 在原项目中找到每个魔数的对应位置 | [ ] | 标注文件名和行号 |
| 3 | 理解为什么config.h应该加入.gitignore | [ ] | 凭据安全考虑 |
| 4 | 对比原版和改进版的波特率设置方式 | [ ] | 115200 → UART_DEBUG_BAUD |
| 5 | 思考：还有哪些参数应该提取到config.h？ | [ ] | 列出你的建议 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] config.h配置项对照表
- [ ] 原版魔数→改进版宏定义映射表
- [ ] 理解凭据安全隔离的重要性

## 自检清单

- [ ] 我能说出config.h中每个宏的含义
- [ ] 我知道为什么要把魔数提取为宏定义
- [ ] 我完成了所有实践任务
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解配置集中化是代码可维护性的基础

## 明日预告

**Day11** 将学习ESP8266改进版代码，理解WiFi断线自动重连机制。
