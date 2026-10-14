# xiaochen_xiangmu — STM32 GPS 实时定位追踪与智能锁箱系统

基于 STM32F103ZET6 + EC200U/BC20 4G + 树莓派 + ESP8266 的毕业设计项目：GPS 实时定位上报、
OneNET 云平台监控、网页端可视化监控台、树莓派串口导航下发、ESP8266 智能锁箱控制。

> ⚠️ 仓库中所有密钥类字符串（`your_product_id` / `your_device_name` / `your_blinker_auth` /
> `your_authtoken` / `YOUR_SIGN_HERE` 等）均为占位符。使用前请在 OneNET / 点灯科技 / natapp
> 控制台获取自己的凭据并替换。

## 系统结构

```
链路一 · 定位上报
  STM32F103ZET6 + GPS + EC200U(4G) + DHT11 ──MQTT──▶ OneNET 云平台
  ▲ 串口                                    （温湿度/经纬度/速度/海拔/方向）
  └── 树莓派（send.py 下发 NAV 目标 / receive.py 接收 GPS 帧）

链路二 · 智能锁箱
  手机 APP(Blinker) / 网页 ──▶ ESP8266 + 继电器 ──▶ 锁箱
  （树莓派经 natapp 内网穿透可从外网访问 ESP8266 网页）
```

## 目录导航

| 目录 | 内容 |
|------|------|
| [stm32-gps-module/](stm32-gps-module/) | STM32 主固件（Keil MDK 工程）：GPS 解析、GCJ-02 转换、OneNET 上报 |
| [improve/](improve/) | 固件改进版：状态机架构、错误处理、配置集中化（见其 README） |
| [new_gaijin/](new_gaijin/) | 再构版：模块化 C 代码 + 30 天学习文档 + 单元测试 |
| [web_dashboard/](web_dashboard/) | **网页可视化监控台**（单文件 index.html）：地图轨迹/图表/远程锁箱/导航目标，详见其 README |
| [esp8266/](esp8266/) | ESP8266 锁箱控制固件（Blinker + 网页双端） |
| [gps-serial-python/](gps-serial-python/) | 树莓派与 STM32 串口收发的 Python 脚本原始版 |
| [improve/scripts/](improve/scripts/) | 串口脚本改进版（send.py / receive.py） |
| [raspberrypi-remote-access/](raspberrypi-remote-access/) | 树莓派远程访问笔记（SSH/VNC/向日葵/natapp） |
| [raspberry-pi-pins/](raspberry-pi-pins/) | 树莓派引脚定义资料 |
| [learn/](learn/) | 项目学习笔记（硬件基础/协议详解/代码阅读） |
| [replication-checklist.md](replication-checklist.md) / [car-workflow.txt](car-workflow.txt) | 复刻清单与整车工作流程 |

## 快速开始（可视化监控台）

```bash
cd web_dashboard
python -m http.server 8000
# 浏览器打开 http://127.0.0.1:8000，点「演示模式」即可无硬件体验
```

## 硬件清单

STM32F103ZET6 开发板 · EC200U-CN/BC20 4G 模块 · GPS 有源天线 · DHT11 ·
树莓派 Zero 2W/4B · ESP8266 · 继电器模块 · 电磁锁。

## License

仅供学习交流使用。
