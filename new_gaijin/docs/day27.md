# Day 27 - ng_app FSM适配器

**日期**: ____/____/________
**预计时长**: 3 小时
**模块重点**: 顶层状态机 / 模块协调 / 应用层

---

> ⚠️ **现状说明（盲写后对照）**：本模块已有参考实现 `src/ng_app.c`（488 行，是 8 个模块中最大的），头文件 `include/ng_app.h`。今天**先不要打开 `src/ng_app.c`**。流程：
> 1. **盲写骨架**：在 `practice/day27/` 盲写 `ng_app.c`——只需实现状态枚举、`ng_app_init()`、`ng_app_run()` 主循环调度和每个状态的**空函数骨架**，在每个状态里用注释写清"这里会调用哪个 ng_ 模块的哪个函数"。不要求填满 488 行；
> 2. **对照**：打开 `src/ng_app.c`，看每个状态的实际实现、状态转换条件、超时处理、以及错误恢复状态如何与 `ng_error` 的等级联动；
> 3. **验证**：本模块只依赖 ng_ 头文件与 `string.h`，可在 PC 上 `gcc -c -I../include -I../adapter ng_app.c` 做语法检查；行为验证需 Day28 的端到端环境。
>
> **不要修改 `src/`、`include/`、`adapter/` 下的参考实现。**

## 今日目标

今天结束时，你应该能够不看参考实现，独立设计出顶层状态机的状态集合与转换关系，写出调度骨架并标明每个状态依赖的 ng_ 模块；对照后能画出完整的模块调用关系图，并说出参考实现在超时与错误恢复上是怎么做的。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `improve/firmware/stm32/USER/main.c` | 第19-276行 | 改进版6状态状态机（ng_app的设计原型） |
| `improve/进阶改进与扩展规划.md` | 第384-398行 | 层次化状态机设计建议 |
| `new_gaijin/include/ng_app.h` | 全文 | 状态枚举与接口——盲写必须遵守 |
| `new_gaijin/src/ng_app.c` | 全文 | **盲写完成后**再读，用于对照 |

## 知识要点

- **ng_app职责**：
  - 顶层状态机，管理所有ng_模块的生命周期
  - 协调GPS读取→坐标转换→MQTT上传的完整流程（JSON构建由ng_mqtt_wrapper内部处理）
  - 调用ng_led_status更新状态指示
  - 调用ng_error处理异常情况
- **状态机设计**（与改进版main.c类似）：
  ```
  NG_APP_STATE_INIT → NG_APP_STATE_MODULE_INIT → NG_APP_STATE_GPS_READ → NG_APP_STATE_GPS_PROCESS → NG_APP_STATE_DATA_UPLOAD → NG_APP_STATE_IDLE
                                                                                                                                          ↓
                                                                                                            NG_APP_STATE_ERROR_RECOVERY
  ```
- **模块调用关系**：
  - NG_APP_STATE_MODULE_INIT：初始化4G模块、DHT11等外设
  - NG_APP_STATE_GPS_READ：调用ng_gps_parser解析GPS数据
  - NG_APP_STATE_GPS_PROCESS：调用ng_coord转换坐标，调用ng_timer管理DHT11间隔
  - NG_APP_STATE_DATA_UPLOAD：调用ng_mqtt_wrapper发送（内部自动构建JSON）
  - 所有状态：调用ng_led_status更新LED，调用ng_error记录错误
- **与改进版main.c的关系**：
  - ng_app是改进版main.c的进一步模块化
  - 将状态机逻辑从main.c中提取到独立模块
- **盲写时要想的问题**：某个状态卡住（如 GPS 一直不定位）怎么跳出？ERROR_RECOVERY 恢复完回到哪个状态？`ng_app_run()` 每次主循环调用一次还是内部死循环？——对照时看参考实现的选择。

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 重读改进版 main.c 第19-276行，列出 6 个状态与转换条件 | [ ] | 这是 ng_app 的原型 |
| 2 | 阅读 `include/ng_app.h`，明确状态枚举、上下文结构体、函数签名 | [ ] | 只看头文件，不看.c |
| 3 | 在 `practice/day27/` 盲写 `ng_app.c` 骨架：init / run 调度 / 各状态空函数 + 注释标明依赖模块 | [ ] | 先画状态图再写代码 |
| 4 | `gcc -c -I../include -I../adapter ng_app.c` 语法检查通过 | [ ] | 只编译不链接 |
| 5 | 打开 `src/ng_app.c` 对照：每个状态的超时处理、ERROR_RECOVERY 如何按 ng_error 等级选择恢复动作、LED 模式在哪切换 | [ ] | 差异记入下方问题表 |
| 6 | 画出 ng_app 的完整模块调用关系图（ng_app → 7 个模块 → HAL → 原版驱动） | [ ] | 这张图是 Day30 交接文档的核心 |
| 7 | 在 Keil 中开 `USE_NEW_APP=1` 编译一次（不烧录），确认无错误后**改回 0** | [ ] | Day28 端到端测试前保持 APP=0 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] `practice/day27/ng_app.c`（状态机骨架 + 依赖注释）
- [ ] 与参考实现的差异对照笔记（重点：超时与错误恢复）
- [ ] 完整模块调用关系图
- [ ] `USE_NEW_APP=1` 编译通过验证

## 自检清单

- [ ] 我能画出ng_app的模块调用关系图
- [ ] 我知道顶层FSM如何协调所有ng_模块
- [ ] 我能说出参考实现处理"状态卡死"的方式
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解模块化架构是大型嵌入式系统的基础

## 明日预告

**Day28** 全系统集成测试，端到端验证整个系统。无硬件请按 `学习方案.md` 第十一节替代路径：PC 端串联三算法模块 + MQTT 客户端工具手动验证 OneNET 链路。
