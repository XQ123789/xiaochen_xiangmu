# GPS 实时定位监控台（网页端可视化）

为 **GPS 实时定位追踪系统** 补齐的网页端可视化，覆盖项目的**两条链路**：

```
链路一 · 定位上报          STM32F103 + GPS + EC200U/BC20 4G + DHT11 ──MQTT──▶ OneNET 云平台
                          （树莓派 send.py/receive.py 经串口与 STM32 交互）
链路二 · 智能锁箱控制      手机/网页 ──▶ ESP8266（Blinker + 网页服务器）──▶ 继电器 ──▶ 锁箱
                          （树莓派经 natapp 内网穿透从外网访问 ESP8266）
```

单文件、零构建：直接 `file://` 打开或用任意静态服务器托管；手机浏览器打开即为"APP 端"（可添加到主屏幕全屏使用）。

## 功能

| 模块 | 说明 |
|------|------|
| 实时地图 | 高德 GCJ-02 瓦片（无需 key）+ Leaflet：方向箭头标记、轨迹线、跟随模式、轨迹全览、清空 |
| 数据卡片 | 温度、湿度、速度、海拔、方向罗盘、坐标、消息计数 |
| 历史趋势 | ECharts 折线：温度/湿度/速度（保留最近 600 个数据点，双 Y 轴） |
| 数据记录 | 最近 100 条表格（带空状态引导），一键导出全部记录 CSV（带 BOM） |
| 消息日志 | 连接/订阅/收发/错误日志（连接失败显示原因，连续 6 次重连失败自动停止） |
| 🎯 导航目标 | 地图右键/长按（或"设目标模式"点选）设目标：显示**距离与方位角**，生成 `NAV,经度,纬度` 指令串（树莓派 `send.py` 的下发格式）一键复制 |
| 🔒 智能锁箱 | 填 ESP8266 地址（内网 IP 或 natapp 隧道），开锁/关锁/查状态（对应固件 `/on` `/off` `/status` 接口） |
| 串口帧解析 | 自定义 MQTT 模式直接识别 `GPS,纬度,经度*校验和`（receive.py 格式）与 `NAV,经度,纬度` 文本帧，树莓派转发即可可视化 |
| 三种数据源 | ① 演示模式（内置模拟器）② OneNET MQTT ③ 任意自定义 MQTT Broker（WSS） |
| token 生成 | 内置 HMAC-MD5，填 AccessKey 自动生成 OneNET token（与 `onenet_token.py` 逐字节一致，37 项单元测试验证） |

## 快速开始

```bash
cd web_dashboard
python -m http.server 8000
# 浏览器访问 http://127.0.0.1:8000（直接双击 index.html 也可）
```

1. 点右上角 **「▶ 演示模式」** —— 无需配置，每 3 秒模拟一帧北京城区车载运动 + DHT11 数据。
2. 真实接入：**「⚙ 连接设置」** 选模式填参数（见下表），点 **连接**（校验不通过弹窗不会关闭）。
3. 地图上**右键/长按**设导航目标；**🔒 智能锁箱**面板填 ESP8266 地址控制锁箱。

## 接入配置

### 模式一：OneNET 平台（MQTT over WebSocket）

| 字段 | 说明 |
|------|------|
| WebSocket 地址 | `wss://mqtts.heclouds.com:443/mqtt`（默认已填） |
| 产品 ID / 设备名 | OneNET 控制台对应产品、设备 |
| AccessKey | 设备详情页 key（base64），页面自动生成 token，有效期可设 |
| 或直接粘贴 token | 用项目根目录 `onenet_token.py` 生成后粘贴，优先级更高 |

连接后自动订阅 `$sys/{产品ID}/{设备}/thing/property/post/reply`（上报应答，正常回执以绿色 OK 日志显示）与 `.../thing/property/set`（属性下发）。

> ⚠ **重要限制（页面会自动检测）**：OneNET 的 MQTT 设备主题是设备私有的，浏览器**无法订阅另一台设备**（如真实 STM32）的上行数据。若与真实设备填同一设备名，双方 clientId 相同会互相踢线——页面检测到反复被断开会红字提示。**建议在 OneNET 单独建一个设备给网页联调用**；要监控真实设备的持续数据，请用模式二。

### 模式二：自定义 MQTT（推荐用于集中监控）

任意支持 WebSocket 的 Broker（自建 EMQX / Mosquitto / broker.emqx.io 等）：
- 订阅主题默认 `gps/+/data`（`+` 通配可监控多设备），发布主题默认 `gps/web/data`；
- 树莓派/服务端把数据转发到该主题即可；**明文 `ws://` 地址会弹确认框警告**（token 不加密传输）。

消息体自动识别四类：

```jsonc
// ① OneNET 物模型（固件 MakeOnenetPayload 原生格式）
{"id":"123","params":{"temp":{"value":25},"humi":{"value":60},
 "latitude":{"value":30.123456},"longitude":{"value":120.654321},
 "altitude":{"value":10.5},"speed":{"value":1.25},"direction":{"value":90.0}}}

// ② 普通 JSON（别名：lat/lng/lon、temperature、heading、course；ts 秒/毫秒均可）
{"temp":25,"humi":60,"lat":30.123456,"lon":120.654321,"alt":10.5,"speed":1.25,"dir":90,"ts":1760000000}

// ③ GPS 串口帧（receive.py 接收格式，纬度在前，带*校验和）
GPS,30.123456,120.654321*7A

// ④ NAV 导航帧（send.py 下发格式，经度在前）→ 地图上显示为目标点
NAV,116.404,39.915
```

### 智能锁箱（ESP8266）

固件 `esp8266_improved.ino` 在 80 端口提供 `/`（控制页）、`/on`（开）、`/off`（关）、`/status`（状态）。本页面对接后三者：

- **内网**：填 `http://192.168.x.x`（ESP8266 的 IPv4，同网段直接访问）；
- **外网**：填 natapp 隧道地址（树莓派上 `./natapp -authtoken=xxx` 映射 ESP8266 端口）。

已知浏览器限制（面板内有提示）：
- ESP8266WebServer 默认不带 CORS 头，跨域读取回复会被拦——开/关指令自动降级为 `no-cors` 盲发（指令能到、回复读不到，可用固件串口日志确认）；`/status` 跨域读不到时红字说明；
- 本页若以 **https** 打开则无法访问 **http** 设备（混合内容限制），请用 http 或 file:// 打开。

## 已知上游固件问题（动态工作流审查发现，未改固件代码）

以下问题在**原固件**中，影响页面显示的数据正确性，建议在 Keil 工程中修复：

1. **🔴 速度与方向颠倒（high）** — `stm32GPS定位模块/USER/main.c:52` 的 `sscanf` 把 `+QGPSLOC` 第 7 字段 `cog`（航向角）读进了 `speed`、第 8 字段 `spkm`（km/h）读进了 `dir`。修法：把最后两个接收参数交换为 `&dir, &speed`，并把 km/h ÷3.6 转成 m/s。
2. **🟠 未定位时上报未初始化变量（medium）** — `main.c` 中 `bd_lat/bd_lon` 未初始化、解析失败分支未清 `alt/speed/dir`，可能输出非法 JSON 或溢出 256 字节缓冲区。修法：解析失败时跳过本轮上报。
3. **🟡 GCJ-02 变换非标准（low）** — `HARDWARE/LED/led.c:96` 的偏移迭代系数合计约 0.917 倍标准值再叠加固定补偿，地图上可能有数十米级系统偏差。建议换用标准 wgs84→gcj02 算法（`gcj02.c` 已有独立实现可对照）。

## 安全说明

- 三个 CDN 依赖（Leaflet/ECharts/MQTT.js）均**锁定版本 + SRI 完整性校验**（sha384），CDN 被篡改时脚本拒绝执行；
- AccessKey/token 仅保存在本机浏览器，默认不落 localStorage（勾选"记住密钥"才保存，明文，注意本机安全）；
- 弹窗内容全部 DOM + textContent 构建，无 innerHTML 注入面；
- 明文 `ws://` 连接会弹确认框二次确认；
- CSV 导出均为数值/时间字段。

## 技术栈

Leaflet 1.9.4 · ECharts 5.5.1 · MQTT.js 5.10.1 · 原生 JS，无框架、无构建。CDN 失败时页面给出降级提示；`<dialog>` 不支持时自动降级为固定定位弹窗（iOS ≤ 15.3）。

## 目录

```
web_dashboard/
├── index.html          # 单文件监控台（本页）
├── README.md
└── _scratch/           # 开发期验证脚本（可删除）
    ├── extract.js      # 从 index.html 抽取内联脚本
    ├── app_test.js     # 37 项单元测试（token/解析/串口帧/距离方位/边界）
    └── token_test.js   # token 算法与 python 对照验证
```
