# new_gaijin — GPS实时定位追踪系统高级算法模块

## 一、项目简介

`new_gaijin` 是 GPS 实时定位追踪智能小车项目的**高级算法模块集合**，基于原项目（`stm32GPS定位模块/`）和改进版（`improve/`）的代码积累，进一步将核心算法提取为独立、可测试、可复用的 C 模块。

**核心设计理念**：
- **PC端优先开发测试**：算法模块先在PC上用gcc编译和测试，验证正确后再移植到STM32
- **编译开关共存**：通过8个独立开关（`USE_NEW_COORD`, `USE_NEW_GPS_PARSER`等）实现新旧代码安全切换
- **模块化设计**：每个 `ng_` 模块职责单一，通过标准接口协作
- **硬件解耦**：通过 HAL 适配层隔离算法逻辑和硬件驱动

**项目背景**：
- 主控 MCU：STM32F103ZET6
- 4G 模块：BC20 / EC200U-CN（GPS定位 + MQTT上传）
- 云平台：OneNET（mqtts.heclouds.com:1883）
- 坐标系：WGS84（GPS原始）→ GCJ02（高德地图）

---

## 二、模块列表

| 模块 | 文件 | 功能说明 | 状态 |
|------|------|----------|------|
| `ng_coord` | `src/ng_coord.c` + `include/ng_coord.h` | WGS84→GCJ02坐标转换算法（从led.c提取） | ✅ 已实现，含PC端测试 |
| `ng_gps_parser` | `src/ng_gps_parser.c` + `include/ng_gps_parser.h` | GPS NMEA数据解析（+QGPSLOC响应） | ✅ 已实现，含PC端测试 |
| `ng_json_builder` | `src/ng_json_builder.c` + `include/ng_json_builder.h` | OneNET JSON数据构建（带缓冲区保护） | ✅ 已实现，含PC端测试 |
| `ng_timer` | `src/ng_timer.c` + `include/ng_timer.h` | TIM2毫秒计数器，非阻塞延时服务 | ✅ 已实现（STM32端，待硬件集成验证） |
| `ng_error` | `src/ng_error.c` + `include/ng_error.h` | 错误计数+分级恢复（L1-L6） | ✅ 已实现（STM32端，待硬件集成验证） |
| `ng_led_status` | `src/ng_led_status.c` + `include/ng_led_status.h` | 非阻塞LED状态指示（8种闪烁模式） | ✅ 已实现（STM32端，待硬件集成验证） |
| `ng_mqtt_wrapper` | `src/ng_mqtt_wrapper.c` + `include/ng_mqtt_wrapper.h` | MQTT连接管理+自动重连 | ✅ 已实现（STM32端，待硬件集成验证） |
| `ng_app` | `src/ng_app.c` + `include/ng_app.h` | 顶层FSM适配器，协调所有ng_模块 | ✅ 已实现（STM32端，待硬件集成验证） |

> **学习提示**：以上 8 个模块均为**参考实现**。`docs/学习方案.md` 的第 3~4 周采用"盲写后对照"模式——先不看参考代码独立实现，再与 `src/ng_*.c` 对照，并运行 `test/` 下的已有测试验证。

---

## 三、编译开关说明

项目通过8个独立编译开关控制各模块是否启用（定义在 `include/ng_config.h`）：

```c
// 在 ng_config.h 中定义（或在Keil Project Options → C/C++ → Define 中添加）
#define USE_NEW_COORD       1   /* 1=新坐标模块, 0=原有行为 */
#define USE_NEW_GPS_PARSER  1   /* 1=新GPS解析,   0=原有行为 */
#define USE_NEW_JSON_BUILDER 1  /* 1=新JSON构建,  0=原有行为 */
#define USE_NEW_TIMER       1   /* 1=新定时器,    0=原有行为 */
#define USE_NEW_ERROR       1   /* 1=新错误处理,  0=原有行为 */
#define USE_NEW_LED_STATUS  1   /* 1=新LED状态,   0=原有行为 */
#define USE_NEW_MQTT        1   /* 1=新MQTT封装,  0=原有行为 */
#define USE_NEW_APP         0   /* 1=新应用逻辑,  0=原有行为（需验证后才开启） */
```

**切换方式**：
- **方式1**：在 `ng_config.h` 中将对应开关改为 `0` 或 `1`
- **方式2**：在 Keil 的 Project Options → C/C++ → Preprocessor Symbols → Define 中添加/移除对应宏名

---

## 四、PC端测试构建指南

### 4.1 环境要求
- gcc（MinGW-w64 on Windows，或 WSL/Linux gcc）
- make（可选，用于批量编译）

### 4.2 编译命令

> 各 `test_*.c` 不含 `main()`，只暴露 `test_xxx_run()`，由 `test_main.c` 统一聚合。因此需要把测试入口、三个测试文件和三个源文件**一起编译**，不能单独编译某个测试文件。

```bash
# 在 new_gaijin/test/ 目录下执行
gcc -I../include -o test_all test_main.c test_coord.c test_gps_parser.c test_json_builder.c \
    ../src/ng_coord.c ../src/ng_gps_parser.c ../src/ng_json_builder.c -lm

./test_all          # Windows 下为 test_all.exe，输出各套件通过/失败统计
```

### 4.3 测试用例说明
- `ng_coord`：使用已知坐标对验证（如北京天安门 WGS84→GCJ02）
- `ng_gps_parser`：覆盖有效/无效/截断/空/NULL等边界场景
- `ng_json_builder`：验证JSON格式正确性和缓冲区溢出保护

---

## 五、Keil项目集成指南

### 5.1 添加源文件
1. 在 Keil 工程中创建 `new_gaijin` Group
2. 添加以下源文件：
   - `src/ng_coord.c`
   - `src/ng_gps_parser.c`
   - `src/ng_json_builder.c`
   - `src/ng_timer.c`
   - `src/ng_error.c`
   - `src/ng_led_status.c`
   - `src/ng_mqtt_wrapper.c`
   - `src/ng_app.c`
   - `adapter/ng_hal_stm32.c`（STM32 HAL适配层）

### 5.2 配置头文件路径
在 Project Options → C/C++ → Include Paths 中添加：
```
new_gaijin/include
new_gaijin/adapter
```

### 5.3 启用编译开关
在 Project Options → C/C++ → Preprocessor Symbols → Define 中添加：
```
USE_NEW_COORD,USE_NEW_GPS_PARSER,USE_NEW_JSON_BUILDER,USE_NEW_TIMER,USE_NEW_ERROR,USE_NEW_LED_STATUS,USE_NEW_MQTT
```
（注意：`USE_NEW_APP` 需验证其他模块稳定后才开启）

### 5.4 编译验证
- 先不定义 `USE_NEW_*` 开关（全为0），编译原版代码 → 应成功
- 再定义各 `USE_NEW_*` 开关为1，编译new_gaijin代码 → 应成功
- 两种模式都能烧录运行 = 集成成功

---

## 六、目录结构

```
new_gaijin/
├── README.md              # 本文件
├── docs/                  # 文档目录
│   ├── 学习方案.md         # 30天学习总览
│   ├── 学习历程.md         # 进度跟踪总表
│   ├── day01.md           # Day01-Day30 每日学习计划
│   ├── day02.md
│   ├── ...
│   └── day30.md
├── include/               # 头文件（所有模块.h）
│   ├── ng_coord.h         # WGS84→GCJ02坐标转换
│   ├── ng_gps_parser.h    # GPS NMEA数据解析
│   ├── ng_json_builder.h  # OneNET JSON数据构建
│   ├── ng_timer.h         # TIM2毫秒计时服务
│   ├── ng_error.h         # 错误计数+分级恢复
│   ├── ng_led_status.h    # 非阻塞LED状态指示
│   ├── ng_mqtt_wrapper.h  # MQTT连接管理
│   ├── ng_app.h           # 顶层FSM适配器
│   ├── ng_config.h        # 编译开关与配置参数
├── src/                   # 算法模块源代码
│   ├── ng_coord.c         # WGS84→GCJ02坐标转换
│   ├── ng_gps_parser.c    # GPS NMEA数据解析
│   ├── ng_json_builder.c  # OneNET JSON数据构建
│   ├── ng_timer.c         # TIM2毫秒计时服务
│   ├── ng_error.c         # 错误计数+分级恢复
│   ├── ng_led_status.c    # 非阻塞LED状态指示
│   ├── ng_mqtt_wrapper.c  # MQTT连接管理
│   └── ng_app.c           # 顶层FSM适配器
├── test/                  # PC端测试代码
│   ├── test_main.c        # 测试主入口
│   ├── test_coord.c       # 坐标转换测试
│   ├── test_gps_parser.c  # GPS解析器测试
│   └── test_json_builder.c# JSON构建器测试
└── adapter/               # 硬件抽象适配层
    ├── ng_hal.h            # HAL接口定义
    └── ng_hal_stm32.c     # STM32端HAL适配实现
```

---

## 七、学习指南入口

本目录包含完整的 **30天学习计划**，帮助接手者从零开始理解项目并掌握new_gaijin模块的开发。

**快速开始**：
1. 阅读 [学习方案.md](docs/学习方案.md) — 了解30天学习总览和前置要求
2. 按顺序完成 [day01.md](docs/day01.md) 到 [day30.md](docs/day30.md) — 每天2-3小时
3. 使用 [学习历程.md](docs/学习历程.md) — 跟踪进度和自评

**前置知识**：
- C语言基础（指针、结构体、字符串操作）
- 了解GPIO和UART概念
- 了解TCP/IP和MQTT基本概念

**推荐学习顺序**：
1. 第1周（Day01-07）：理解原项目架构和数据流
2. 第2周（Day08-14）：理解改进版状态机和超时保护
3. 第3周（Day15-21）：PC端**盲写**核心算法模块，再对照已有实现并运行测试
4. 第4周（Day22-30）：STM32集成（参考已有模块实现）和端到端测试

---

## 八、相关文档

| 文档 | 路径 | 说明 |
|------|------|------|
| 项目总览 | `learn/00-项目总览.md` | 三层架构和数据流 |
| 代码阅读指南 | `learn/02-代码阅读指南.md` | 逐行讲解核心代码 |
| 学习路线图 | `learn/04-学习路线图.md` | 8周学习路径规划 |
| 项目不足与改进方案 | `improve/项目不足与改进方案.md` | 23个问题+改进方案 |
| 进阶改进与扩展规划 | `improve/进阶改进与扩展规划.md` | 算法/硬件/功能规划 |
| 改进版README | `improve/README.md` | 改进版项目说明 |

---

## 九、注意事项

1. **凭据安全**：涉及OneNET密钥和WiFi密码的 `config.h` 已加入 `.gitignore`，不要上传到公开仓库
2. **备份意识**：每次大改前用Git创建分支或备份整个项目
3. **渐进式集成**：每集成一个ng_模块就编译验证一次，不要批量修改
4. **回退机制**：始终保留 `USE_NEW_*` 开关，确保可以随时回退到原版代码

---

> **版本**：v1.1（算法模块已全部实现；学习计划更新为"盲写后对照"模式）
> **最后更新**：2026年9月
