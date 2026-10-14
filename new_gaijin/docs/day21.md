# Day 21 - ng_json_builder测试 + 周回顾

**日期**: ____/____/________
**预计时长**: 3 小时
**模块重点**: 完整测试套件 / 第3周回顾 / 模块验证

---

> ⚠️ **现状说明**：`test/test_json_builder.c` 已存在，通过 `test_json_builder_run()` 由 `test_main.c` 聚合。今天不从零写测试文件，重点是：精读它 → 补漏用例 → **用你三天盲写的三个模块一起替换参考实现，跑通全部测试** → 整理本周"我的实现 vs 参考实现"差异总结。

## 今日目标

今天结束时，你应该能够让 `practice/` 下自己盲写的 ng_coord、ng_gps_parser、ng_json_builder 三个模块同时接入 `test_all` 并全部通过，并写出一份三模块差异总结。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `new_gaijin/test/test_json_builder.c` | 全文 | 已有用例：正常/缺失/溢出/特殊值各对应哪个 |
| `new_gaijin/src/ng_json_builder.c` | 全文 | 参考实现（昨天已对照） |
| `new_gaijin/src/ng_coord.c` | 全文 | 参考实现（Day16已对照） |
| `new_gaijin/src/ng_gps_parser.c` | 全文 | 参考实现（Day18已对照） |

## 知识要点

- **完整测试覆盖**：
  - 正常JSON构建：所有字段有效
  - 部分字段缺失：GPS无效时只上传温湿度
  - 缓冲区不足：传入小缓冲区，验证不溢出
  - 特殊值：负数温度、极大坐标值
- **第3周成果检查**：
  - ng_coord：坐标转换算法独立可用
  - ng_gps_parser：GPS数据解析健壮可靠
  - ng_json_builder：JSON生成安全无溢出
- **编译命令**（在 `new_gaijin/test/` 下，全部换成你的实现）：
  ```bash
  gcc -I../include -o test_mine test_main.c test_coord.c test_gps_parser.c test_json_builder.c \
      ../practice/day16/ng_coord.c ../practice/day18/ng_gps_parser.c ../practice/day20/ng_json_builder.c -lm
  ./test_mine
  ```

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 精读 `test/test_json_builder.c`，把每个用例归入上方四类场景 | [ ] | 找出覆盖盲区 |
| 2 | 在 `practice/day21/test_json_builder_extra.c` 补至少2个用例 | [ ] | 如：缓冲区恰好等于所需长度、所有传感器值为0 |
| 3 | 用上方命令把**三个盲写模块一起**接入 `test_all` 编译运行，确保全部通过 | [ ] | 这是第3周里程碑的硬性条件 |
| 4 | 回顾第3周，填写 `学习历程.md` 的进度表和第3周里程碑 | [ ] | 记录遇到的关键问题 |
| 5 | 整理"我的实现 vs 参考实现"差异总结（三模块各至少3条） | [ ] | 归类：健壮性 / 可读性 / 性能 / 接口设计 |
| 6 | 本周下午为机动时间：补欠账，或提前浏览 `adapter/ng_hal.h` 为第4周预热 | [ ] | 见学习方案第三节节奏规则 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] `practice/day21/test_json_builder_extra.c`
- [ ] 三个盲写模块同时接入 `test_all` 全部通过
- [ ] 三模块差异总结文档
- [ ] `学习历程.md` 第3周里程碑已填写

## 自检清单

- [ ] 我能独立编译和测试所有PC端模块
- [ ] 我知道每个模块的函数接口和返回值含义
- [ ] 我能说出参考实现在每个模块上比我多考虑了哪些点
- [ ] 我记录了遇到的问题和解决方案
- [ ] 我理解第3周的模块是第4周集成的基础

## 明日预告

**Day22** 开始第4周STM32集成，首先盲写 ng_timer 计时服务的骨架，再对照参考实现。第4周起模块依赖硬件寄存器，验证方式从"跑测试"变为"gcc语法检查 + Keil编译"。
