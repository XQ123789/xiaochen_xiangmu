# Day 12 - Python脚本改进

**日期**: ____/____/________
**预计时长**: 2 小时
**模块重点**: Python / 日志记录 / 串口重连

---

## 今日目标

今天结束时，你应该能够对比两版Python代码的差异，理解logging模块的使用和串口重连机制的实现。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `发送和接收GPS坐标python代码/receive.py.txt` | 全文 | 原版接收脚本（缺少import serial） |
| `发送和接收GPS坐标python代码/send.py.txt` | 全文 | 原版发送脚本 |
| `improve/scripts/receive.py` | 第1-187行（全文） | 改进版接收脚本 |
| `improve/scripts/send.py` | 第1-174行（全文） | 改进版发送脚本 |
| `improve/scripts/config.json` | 全文 | 配置文件 |
| `improve/scripts/requirements.txt` | 全文 | Python依赖管理 |

## 知识要点

- **原版问题**：
  - 文件扩展名错误（`.py.txt`无法直接运行）
  - `receive.py`缺少`import serial`
  - 无异常处理，串口断开直接崩溃
  - 无依赖管理（缺少requirements.txt）
- **改进版变化**：
  - 修正扩展名为`.py`
  - 添加完整的import语句
  - 引入logging模块：同时输出到控制台和文件
  - 串口重连机制：断开后自动尝试重新打开
  - 支持配置文件`config.json`：串口端口和波特率可配置
  - 添加`requirements.txt`：`pip install -r requirements.txt`

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 对比两版receive.py，找出所有改进点 | [ ] | 至少列出5处差异 |
| 2 | 理解logging配置：StreamHandler和FileHandler | [ ] | 为什么需要两种输出 |
| 3 | 找到串口重连逻辑，理解异常处理机制 | [ ] | try-except结构 |
| 4 | 阅读config.json，理解配置加载流程 | [ ] | 默认配置vs文件配置 |
| 5 | 在PC上运行改进版receive.py（如有串口设备） | [ ] | 观察日志输出 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] 原版vs改进版Python代码对比表
- [ ] logging模块使用笔记
- [ ] 串口重连机制流程图

## 自检清单

- [ ] 我能解释Python logging模块的用法
- [ ] 我知道为什么需要串口重连机制
- [ ] 我完成了所有实践任务
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解requirements.txt是Python项目依赖管理的标准方式

## 明日预告

**Day13** 将深度分析《项目不足与改进方案》，建立问题跟踪表。
