# Day 07 - 4G模块AT指令(下)

**日期**: ____/____/________
**预计时长**: 3 小时
**模块重点**: MQTT协议 / JSON数据上传 / AT指令进阶

---

## 今日目标

今天结束时，你应该能够完整描述从MQTT连接到数据上传的全过程，画出完整的AT指令交互时序，并理解JSON数据格式。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `stm32GPS定位模块/HARDWARE/4G/4G.c` | 第120-272行 | CSTX_4G_RegALIYUNIOT()和CSTX_4G_ALYIOTSenddata() |
| `learn/02-代码阅读指南.md` | 第149-167行 | MQTT注册和数据上传流程说明 |
| `learn/03-通信协议详解.md` | 全文 | MQTT协议和JSON格式详解 |
| `improve/项目不足与改进方案.md` | 第48-53行 | 函数参数顺序BUG分析 |

## 知识要点

- **MQTT连接AT指令序列**（CSTX_4G_RegALIYUNIOT）：
  1. `AT+QMTDISC=0` → 断开旧连接
  2. `AT+QMTCLOSE=0` → 关闭旧会话
  3. `AT+QMTCFG="version"` → 设置MQTT版本
  4. `AT+QMTOPEN=0,"mqtts.heclouds.com",1883` → 打开TCP连接
  5. `AT+QMTCONN=0,"device_name","product_id","device_secret"` → MQTT登录
- **数据上传流程**（CSTX_4G_ALYIOTSenddata）：
  1. `MakeOnenetPayload()` 生成JSON字符串
  2. `AT+QMTSUB` → 订阅属性设置Topic
  3. `AT+QMTPUBEX` → 准备发布数据
  4. 等待 `>` 提示符 → 发送JSON + `0x1A`(Ctrl+Z)
  5. 验证 `+QMTPUBEX: 0,0,0` 确认成功
- **JSON数据格式**：包含温度、湿度、经纬度、海拔、速度、方向
- **已知BUG**：4G.h声明参数顺序（direction,speed）与4G.c实现（speed,direction）不一致

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 精读 `4G.c` 第120-272行，找到MQTT连接函数 | [ ] | 逐条AT指令分析 |
| 2 | 找到MakeOnenetPayload()函数，理解JSON生成逻辑 | [ ] | 记录JSON格式 |
| 3 | 找到CSTX_4G_ALYIOTSenddata()，画出上传流程 | [ ] | 从JSON生成到发送完成 |
| 4 | 对比4G.h声明和4G.c实现的参数顺序差异 | [ ] | 记录这个BUG |
| 5 | 在纸上画出完整的MQTT连接+数据上传AT指令时序图 | [ ] | 包含所有AT指令和响应 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] MQTT连接AT指令时序图
- [ ] 数据上传完整流程图
- [ ] JSON数据格式示例

## 自检清单

- [ ] 我能描述MQTT的发布/订阅模式
- [ ] 我知道数据上传的完整AT指令序列
- [ ] 我完成了所有实践任务
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解整个系统的数据是如何从STM32到达云端的

## 明日预告

**Day08** 将开始第2周学习，对比原版和改进版main.c，理解状态机架构的引入。
