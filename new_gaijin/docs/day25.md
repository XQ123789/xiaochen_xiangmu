# Day 25 - ng_mqtt_wrapper MQTT管理

**日期**: ____/____/________
**预计时长**: 3 小时
**模块重点**: MQTT连接管理 / 自动重连 / 状态检查

---

> ⚠️ **现状说明（盲写后对照）**：本模块已有参考实现 `src/ng_mqtt_wrapper.c`（255 行），头文件 `include/ng_mqtt_wrapper.h`。今天**先不要打开 `src/ng_mqtt_wrapper.c`**。流程：
> 1. **盲写骨架**：在 `practice/day25/` 盲写 `ng_mqtt_wrapper.c`——重点是**状态机与重连策略的逻辑**，AT 指令收发一律通过 `ng_hal.h` 的接口调用，不要自己拼 AT 字符串细节；
> 2. **对照**：打开 `src/ng_mqtt_wrapper.c`，看它如何与 `ng_error` 联动、重连的退避策略、发送前的连接检查放在哪一层；
> 3. **验证**：本模块只依赖 ng_ 头文件与 `string.h`，可在 PC 上 `gcc -c -I../include -I../adapter ng_mqtt_wrapper.c` 做语法检查；行为验证需硬件 + SIM 卡 + OneNET。
>
> **不要修改 `src/`、`include/`、`adapter/` 下的参考实现。**

## 今日目标

今天结束时，你应该能够不看参考实现，独立写出 MQTT 连接管理的状态机骨架（连接建立、发送前检查、断线重连、失败计数上报 ng_error），并能说出参考实现在可靠性上比你多做了什么。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `improve/firmware/stm32/HARDWARE/4G/bc20_improved.c` | 第100-200行 | MQTT连接和数据发送函数 |
| `improve/firmware/stm32/USER/main.c` | 第250-268行 | STATE_ERROR_RECOVERY重连逻辑 |
| `improve/项目不足与改进方案.md` | 第253-255行 | P2-4 MQTT断线重连方案 |
| `new_gaijin/include/ng_mqtt_wrapper.h` | 全文 | 接口与状态枚举——盲写必须遵守 |
| `new_gaijin/adapter/ng_hal.h` | MQTT/AT 相关接口 | 盲写时能调用哪些底层能力 |
| `new_gaijin/src/ng_mqtt_wrapper.c` | 全文 | **盲写完成后**再读，用于对照 |

## 知识要点

- **模块功能**：
  - 封装MQTT连接建立（AT+QMTOPEN + AT+QMTCONN）
  - 封装数据发送（AT+QMTPUBEX）
  - 连接状态检查（AT+QMTSTAT?）
  - 自动重连逻辑（发送前检查，断线则重连）
- **状态机设计**：
  ```
  DISCONNECTED → CONNECTING → CONNECTED → SENDING → CONNECTED
                                              ↓
                                         DISCONNECTED (断线)
  ```
- **重连策略**：
  - 发送数据前检查连接状态
  - 断线时执行重连流程
  - 重连失败时递增错误计数
  - 超过最大重试次数触发系统恢复
- **与bc20_improved.c的关系**：
  - ng_mqtt_wrapper 通过 HAL 接口间接使用 AT 指令能力
  - 在上层提供统一的MQTT管理接口
- **盲写时要想的问题**：重连是"立刻重试"还是"等一段时间再试"？连续失败 3 次和 30 次应该有区别吗？——对照时看参考实现怎么处理。

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 阅读bc20_improved.c中的MQTT相关函数 | [ ] | 理解连接和发送流程 |
| 2 | 阅读 `include/ng_mqtt_wrapper.h` 与 `adapter/ng_hal.h`，明确接口和可用的底层能力 | [ ] | 只看头文件，不看.c |
| 3 | 在 `practice/day25/` 盲写 `ng_mqtt_wrapper.c` 状态机骨架：connect / publish / check / reconnect | [ ] | 失败时调用 ng_error 接口计数 |
| 4 | `gcc -c -I../include -I../adapter ng_mqtt_wrapper.c` 语法检查通过 | [ ] | 只编译不链接 |
| 5 | 打开 `src/ng_mqtt_wrapper.c` 对照：状态转换条件、重连退避、与 ng_error 的联动方式 | [ ] | 差异记入下方问题表 |
| 6 | 在Keil工程中把参考实现加入并编译通过；有硬件则拔天线模拟断线观察重连 | [ ] | 无硬件标记 `[H]` |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] `practice/day25/ng_mqtt_wrapper.c`（状态机骨架）
- [ ] 与参考实现的差异对照笔记（重点：重连策略）
- [ ] 参考实现 Keil 编译通过；断线重连验证（无硬件标记 `[H]`）

## 自检清单

- [ ] 我能描述MQTT连接管理的状态机
- [ ] 我知道如何实现自动重连
- [ ] 我能说出参考实现的重连策略与我的差异
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解MQTT连接管理是通信可靠性的保障

## 明日预告

**Day26** 编译开关集成：按 README 第五节把全部参考实现加入 Keil 工程，用 `ng_config.h` 的 8 个独立开关验证新旧代码共存。
