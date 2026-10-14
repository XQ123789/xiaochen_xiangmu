# Day 15 - PC测试环境验证

**日期**: ____/____/________
**预计时长**: 2 小时
**模块重点**: gcc编译环境 / new_gaijin目录结构 / 已有测试跑通

---

> ⚠️ **现状说明**：`new_gaijin/` 的目录结构、8 个模块的参考实现和 PC 端测试**都已经存在**，今天不需要创建任何目录或模块。今天的任务是把 gcc 环境配好，并把已有的测试全部编译跑通——这套测试就是接下来 Day16~21 盲写时的"标准答案"。

## 今日目标

今天结束时，你应该能够在 PC 上用 gcc 一条命令编译出 `test_all`，运行后看到"所有测试通过"，并说清楚 `new_gaijin/` 每个子目录放什么。

## 阅读清单

| 文件 | 行号范围 | 关注重点 |
|------|----------|----------|
| `new_gaijin/README.md` | 二、四、六节 | 模块状态表、PC端测试构建指南、目录结构 |
| `new_gaijin/test/test_main.c` | 全文（43行） | 测试运行器如何聚合三个测试套件；文件头注释里的编译命令 |
| `improve/进阶改进与扩展规划.md` | 第477-493行 | HAL设计参考 |

## 知识要点

- **PC端测试的意义**：
  - 算法逻辑可以在PC上独立验证，无需烧录硬件
  - gcc编译速度快，调试方便
  - 测试用例可重复运行
- **实际目录结构**（已存在，注意头文件在 `include/` 而非 `src/`）：
  ```
  new_gaijin/
  ├── include/       # 所有模块头文件 ng_*.h + ng_config.h（编译开关）
  ├── src/           # 8个模块的参考实现 ng_*.c
  ├── test/          # PC端测试：test_main.c（入口）+ test_coord.c / test_gps_parser.c / test_json_builder.c
  ├── adapter/       # 硬件抽象层：ng_hal.h + ng_hal_stm32.c
  ├── docs/          # 学习文档（本文件所在）
  └── README.md
  ```
- **测试的组织方式**：各 `test_*.c` **没有 `main()`**，只暴露 `test_xxx_run()` 函数，由 `test_main.c` 统一调用并汇总通过/失败数。因此**必须把 test_main.c 和三个测试文件、三个源文件一起编译**，单独编译某个 `test_*.c` 会链接失败（找不到 main）。
- **正确的编译命令**（在 `new_gaijin/test/` 目录下执行）：
  ```bash
  gcc -I../include -o test_all test_main.c test_coord.c test_gps_parser.c test_json_builder.c ../src/ng_coord.c ../src/ng_gps_parser.c ../src/ng_json_builder.c -lm
  ./test_all        # Windows 下为 test_all.exe
  ```

## 实践任务

| 步骤 | 任务描述 | 完成状态 | 备注 |
|------|----------|----------|------|
| 1 | 在PC上安装gcc编译环境（MinGW-w64 或 WSL） | [ ] | `gcc --version` 能输出版本号即可 |
| 2 | 浏览 `include/`、`src/`、`test/`、`adapter/` 四个目录，对照README模块表数一数文件 | [ ] | 8个.c + 9个.h + 4个测试文件 + 2个adapter文件 |
| 3 | 进入 `test/` 目录，用上面的命令编译出 `test_all` | [ ] | 若报"找不到头文件"，检查 `-I../include` |
| 4 | 运行 `test_all`，确认输出"所有测试通过！" | [ ] | 记录三个套件各自的用例数 |
| 5 | 故意把 `src/ng_coord.c` 里某个常量改错，重新编译运行，观察测试如何报错，然后改回来 | [ ] | 体会测试作为"标准答案"的反馈作用 |
| 6 | 把编译命令写成 `test/build.sh` 或 `build.bat`（可选） | [ ] | 后续每天要反复用 |

## 遇到的问题与解决方案

| 问题描述 | 原因分析 | 解决方案 | 耗时 |
|----------|----------|----------|------|
| | | | |

## 今日成果

- [ ] gcc编译环境可用
- [ ] `test_all` 编译成功、运行全部通过
- [ ] 能说出 `include/ src/ test/ adapter/` 各放什么

## 自检清单

- [ ] 我能用gcc编译多文件C程序并指定头文件目录
- [ ] 我知道new_gaijin的目录结构，知道头文件在 `include/`
- [ ] 我理解为什么单独编译 `test_coord.c` 会失败
- [ ] 我完成了所有实践任务
- [ ] 我记录了遇到的问题和解决方案

## 明日预告

**Day16** 开始第一个模块 `ng_coord` 的**盲写**：先不看 `src/ng_coord.c`，只根据 `include/ng_coord.h` 的接口声明和 `led.c` 里的原始算法自己实现，写完再对照参考实现并用今天跑通的测试验证。
