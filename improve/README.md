# GPS实时定位追踪系统（改进版）

## 项目简介

本项目是 GPS 实时定位追踪系统的代码质量改进版本。原系统基于 **STM32F103ZET6 + BC20/EC200U-CN 4G 模块**实现定位数据的云端上报，改进版在保持原有功能不变的前提下，对代码结构、健壮性和可维护性进行了全面优化。

## 主要改进

| 改进方向 | 说明 |
|---------|------|
| 状态机架构 | 主循环采用有限状态机（FSM）设计，替代原项目的顺序阻塞写法，各模块解耦，响应更及时 |
| 错误处理 | 4G 模块通信、传感器读取等关键操作增加超时重试与错误码返回机制，避免程序卡死 |
| 配置集中化 | 将分散在各文件中的魔数、引脚定义统一收归至 `config.h`，修改参数只需改一处 |
| 凭据安全 | API Key、设备ID 等敏感信息通过 `secrets.h`（已加入 .gitignore）单独管理，防止泄露 |
| 代码注释 | 关键函数和逻辑段落补充中文注释，降低阅读门槛 |

## 硬件需求

| 硬件 | 数量 | 备注 |
|------|------|------|
| STM32F103ZET6 开发板 | 1 | 主控，最小系统板即可 |
| EC200U-CN / BC20 4G 模块 | 1 | 用于连接 OneNET 云平台上报数据 |
| DHT11 温湿度传感器 | 1 | 采集环境温湿度 |
| ESP8266 WiFi 模块 | 1 | 配合树莓派进行本地通信 |
| 树莓派（Zero 2W 或 4B） | 1 | 运行 Python 脚本，负责坐标下发与接收 |
| 继电器模块 | 1 | 用于控制智能锁等外设 |
| GPS 有源天线 | 1 | 搭配 4G 模块使用，提升定位精度 |

## 编译环境

### STM32 固件
- **IDE**：Keil MDK v5.x
- **固件库**：STM32F10x 标准外设库（StdPeriph）
- 工程文件位于 `firmware/stm32/` 目录

### ESP8266 固件
- **IDE**：Arduino IDE（1.8.x 或 2.x）
- **板支持包**：ESP8266 Board Package
- **依赖库**：Blinker（点灯科技）
- 源码：`firmware/esp8266/esp8266_improved.ino`

### Python 脚本
- **版本**：Python 3.8+
- **依赖安装**：
  ```bash
  cd scripts
  pip install -r requirements.txt
  ```

## 编译与烧录

### STM32
1. 使用 Keil MDK 打开 `firmware/stm32/` 下的工程文件
2. 根据实际硬件修改 `config.h` 中的引脚和通信参数
3. 编译（F7），生成 `.hex` 文件
4. 通过 ST-Link / J-Link 将固件烧录至 STM32 开发板

### ESP8266
1. Arduino IDE 中安装 ESP8266 板支持包和 Blinker 库
2. 打开 `firmware/esp8266/esp8266_improved.ino`
3. 修改 WiFi 名称、密码及 Blinker 设备密钥
4. 选择对应开发板型号和串口，上传固件

### Python 脚本
1. 安装依赖：`pip install -r scripts/requirements.txt`
2. 编辑 `scripts/config.json`，填入服务器地址和端口
3. 运行：`python scripts/send.py` 或 `python scripts/receive.py`

## 配置说明

所有可配置参数集中在以下文件中：

- **`firmware/stm32/config.h`**：服务器地址、端口、APN 信息、GPIO 引脚定义、超时时间等
- **`scripts/config.json`**：Python 脚本使用的服务器地址、端口、通信参数

> 敏感凭据（如 OneNET API Key、设备 ID）请填入 `secrets.h`（STM32 端）或 `config_local.json`（Python 端），这两个文件已被 `.gitignore` 忽略，不会提交到仓库。

## 目录结构

```
improve/
├── 项目不足与改进方案.md        # 原项目问题分析与改进方案文档
├── .gitignore                   # Git 忽略规则
├── README.md                    # 本文件
├── firmware/
│   ├── stm32/
│   │   ├── config.h             # 配置集中管理
│   │   ├── USER/
│   │   │   └── main.c           # 改进版主循环（状态机 + 错误处理）
│   │   └── HARDWARE/
│   │       └── 4G/
│   │           └── bc20_improved.c  # 改进版 4G 驱动
│   └── esp8266/
│       └── esp8266_improved.ino   # 改进版 ESP8266 代码
└── scripts/
    ├── config.json               # Python 脚本配置模板
    ├── requirements.txt          # Python 依赖清单
    ├── send.py                   # 改进版数据发送脚本
    └── receive.py                # 改进版数据接收脚本
```

## 注意事项

1. 改进代码在功能上与原项目保持完全一致，仅优化了代码质量和结构。
2. 使用前请将 `config.h` 中的 OneNET 服务器地址、API Key、设备 ID 等替换为您自己的账号信息。
3. `scripts/config.json` 同样需要根据实际环境修改服务器地址和端口。
4. 建议先阅读并理解原项目代码逻辑，再对照改进版学习改进思路。
5. `secrets.h` 和 `config_local.json` 包含敏感信息，切勿提交到公开仓库。
