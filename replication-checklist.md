# GPS 实时定位系统复刻清单（核心定位链路）

> 依据：原版 Keil 工程 `stm32GPS定位模块`（STM32F103ZET6 + BC20/EC200U + DHT11 → OneNET 云平台）
> 用法：从阶段 0 开始逐项打勾，全部完成后在 OneNET 控制台即可看到设备在线、GPS 经纬度和温湿度数据。

```
DHT11 ──PA11──┐
              STM32F103ZET6 ──PA2/PA3 + PA12──> EC200U/BC20 ──4G──> OneNET (MQTT)
CH340 调试串口 ──PA9/PA10──┘
```

---

## 阶段 0：硬件与环境核对

- [ ] STM32F103ZET6 开发板（ZET6 最小系统板均可，代码只用了少量 GPIO/串口）
- [ ] 4G 模块：**优先用 EC200U-CN**（Cat-4，全网通）；BC20 是 NB-IoT/Cat-M 模块，依赖当地 NB 网络覆盖，作为备选。两者 AT 指令基本兼容，同一套代码
- [ ] 中国移动 4G SIM 卡：已实名、已开流量、未欠费（BC20 若用 NB 网络需确认卡支持）
- [ ] GPS **有源天线**（必须；无源天线室内基本定不了位）
- [ ] 4G LTE 主天线
- [ ] DHT11 温湿度传感器（用带 4.7K 上拉的模块板最省事）
- [ ] CH340 / CP2102 USB 转 TTL 模块
- [ ] ST-Link V2（或 J-Link）
- [ ] 杜邦线若干（母对母、公对母都备一些）
- [ ] **4G 模块供电能力 ≥ 2A 峰值**（发射瞬间电流大，供电不足会出现注网失败/反复重启；所有设备必须共地）
- [ ] 电脑环境：Keil MDK5 + STM32F1 器件包、CH340 驱动、ST-Link 驱动、串口助手（SSCOM / XCOM 均可）

---

## 阶段 1：接线

| STM32 引脚 | 功能 | 连接到 | 说明 |
|---|---|---|---|
| PA2 (USART2_TX) | TX2 | 4G 模块 **RXD** | 交叉连接 |
| PA3 (USART2_RX) | RX2 | 4G 模块 **TXD** | 交叉连接 |
| PA12 | 电源控制 | 4G 模块 PWRKEY/EN | 代码开机自动拉高 1s 再拉低，模拟开机键；若模块不需要则不接 |
| PA11 | GPIO | DHT11 DATA | 模块无上拉时外接 4.7KΩ 到 3.3V |
| PA9 (USART1_TX) | TX1 | CH340 RX | 调试打印，115200 |
| PA10 (USART1_RX) | RX1 | CH340 TX | 调试（本链路只看打印，可不接 TX） |
| PB10/PB11 (USART3) | — | （预留树莓派，本阶段不接） | |
| GND | — | 4G 模块 GND、CH340 GND | **必须共地** |
| 5V/3.3V | — | 按模块要求供电 | EC200U 模块多用 3.3~4.2V，确认底板供电 |

提示：
- [ ] 上电前再核对一遍 PA2/PA3 是否交叉（TX→RX），接反是最常见的"模块无响应"原因
- [ ] 只跑通定位链路时 DHT11 可先不接，不影响 GPS 上云

---

## 阶段 2：OneNET 平台配置

- [ ] 注册 OneNET 账号，进入控制台
- [ ] 创建产品：接入协议选 **MQTT**（旧版多协议接入），数据协议 **OneJSON/物模型**
- [ ] 记录 **产品 ID**（形如 `your_product_id`）和 **AccessKey**（产品级 master-key 或设备级 key，在产品/设备详情页）
- [ ] 产品下**添加设备**，记录**设备名称**（如 `your_device_name`，可用任意英文名）
- [ ] 给物模型添加 **7 个属性**（标识符必须与代码一字不差）：

| 标识符 | 含义 | 建议类型 |
|---|---|---|
| temp | 温度 | int32 |
| humi | 湿度 | int32 |
| latitude | 纬度 | double |
| longitude | 经度 | double |
| altitude | 海拔 | double |
| speed | 速度 | double |
| direction | 航向 | double |

- [ ] 用本目录 **`onenet_token.py`** 生成设备 token（填产品 ID、设备名、AccessKey、有效期），得到可直接粘贴的 `DEVICESECRET` 字符串
- [ ] （可选，强烈建议）先用 MQTTX 等工具手动连一次验证 token：地址 `mqtts.heclouds.com:1883`，ClientID=设备名，Username=产品 ID，Password=token。能连上就说明凭证没问题，排除了后面一半的故障可能

---

## 阶段 3：代码修改与编译

- [ ] Keil 打开 `stm32GPS定位模块\USER\CSTX.uvprojx`
- [ ] 修改 `HARDWARE\4G\4G.c` **第 13–15 行**三行凭证，替换成阶段 2 得到的自己的值：

```c
#define PRODUCEKEY     "你的产品ID"
#define DEVICENAME     "你的设备名"
#define DEVICESECRET   "version=2018-10-31&res=products%2Fyour_product_id%2Fdevices%2Fyour_device_name&et=0000000000&method=md5&sign=YOUR_SIGN_HERE&et=...&method=md5&sign=..."   // 脚本生成
```

- [ ] 编译（0 Error 即可；工程器件设为 STM32F103RB 而板子是 ZET6，**不用改**，代码 32KB 在两种芯片上都能跑）

---

## 阶段 4：烧录

- [ ] ST-Link 四线接板子 SWD：SWDIO / SWCLK / GND / 3.3V
- [ ] Keil 直接 Load，或用 STM32CubeProgrammer 烧录新生成的 hex
- [ ] ⚠️ **不要直接烧资料里现成的 `OUTPUT\CSTX.hex`**——里面是原作者的 OneNET 凭证，数据会发到别人的设备上，你的云端永远收不到

---

## 阶段 5：逐条验证

串口助手开 CH340，波特率 **115200**，按下表逐条对照（代码会自动执行整个 AT 流程）：

- [ ] 打印复位原因（POR/PIN，说明程序在跑）
- [ ] `start init EC200X` → 开始 4G 初始化
- [ ] `ATE1` → `OK`（模块应答，串口接对了）
- [ ] `AT+CIMI` → 返回 `460` 开头（读到 SIM 卡）
- [ ] `AT+CGATT?` → `+CGATT: 1`（注网成功，最慢 1–2 分钟，期间会反复查询）
- [ ] `AT+CSQ` → 第一个数值 > 10（信号质量）
- [ ] `AT+QGPS=1` → `OK`，随后 `+QGPS: 1`（GNSS 打开）
- [ ] `+QMTOPEN: 0,0`（TCP 连上 OneNET 服务器）
- [ ] `+QMTCONN: 0,0,0`（**MQTT 鉴权通过 ← token 正确的标志**）
- [ ] `+QMTPUBEX: 0,0,0`（数据发布成功）
- [ ] OneNET 控制台设备状态变**在线**，物模型数据里 temp/humi 有值
- [ ] 把设备拿到**室外或窗边**，首次冷启动定位需 1–5 分钟；出现 `+QGPSLOC` 经纬度后，OneNET 上 latitude/longitude 更新（已做 WGS84→GCJ02 转换，可直接叠加高德地图）

---

## 阶段 6：常见问题排查

| 现象 | 原因 | 处理 |
|---|---|---|
| 板子反复重启，打印 IWDGRST | 看门狗约 3.2s，某步 AT 等待超时（常见为注网慢） | 检查天线、信号、SIM；供电不足也会导致 |
| ATE1 无任何应答 | TX/RX 接反、模块没供电、波特率不对 | 核对 PA2/PA3 交叉；确认 115200 |
| CIMI 读不到 460 | SIM 未插好 / 欠费 / 卡制式不符 | 换卡确认；BC20 需 NB 网络卡 |
| CGATT 一直 0 | 信号弱、流量未开通、天线未接 | 移到信号好处；查卡状态 |
| QMTOPEN 失败 | 网络未附着 / 服务器不可达 | 先确认 CGATT:1 |
| QMTCONN 失败 | **token 错**（et 过期、key 用错、编码问题） | 用 `onenet_token.py` 重新生成；先用 MQTTX 验证 |
| QGPS 一直无定位 | 室内 / 天线问题 / 冷启动慢 | 必须室外或窗边，有源天线朝天空，等 1–5 分钟 |
| 设备在线但无数据 | 物模型标识符与代码不一致 | 核对 7 个标识符拼写 |
| 坐标偏几百米 | GCJ02 转换含手动校准偏移 | `HARDWARE\LED\led.c` 中 `EAST_OFFSET/NORTH_OFFSET` 按当地微调 |
| 串口乱码 | 波特率不是 115200 / 共地不良 | 核对两端波特率、检查 GND |

---

## 阶段 7：可选扩展（核心链路跑通后再做）

- ESP8266 智能锁盒：`esp8266\esp8266.ino`（Arduino 烧录，GPIO0 接继电器，Blinker APP + 网页双控制）
- 树莓派联动：USART3（PB10/PB11）↔ 树莓派串口 115200，跑 `发送和接收GPS坐标python代码` 里的 send.py / receive.py
- 代码升级：`improve\`（状态机重构版）、`new_gaijin\`（PC 可测试算法库版）

---

## 关键文件索引

| 文件 | 内容 |
|---|---|
| `stm32GPS定位模块\USER\CSTX.uvprojx` | Keil 工程入口 |
| `stm32GPS定位模块\USER\main.c` | 主流程：初始化 → 4G 注网 → MQTT → 循环发布 |
| `stm32GPS定位模块\HARDWARE\4G\4G.c` | 4G/AT/MQTT 驱动，**凭证在第 13–15 行** |
| `stm32GPS定位模块\HARDWARE\DHT11\` | 温湿度驱动（PA11） |
| `stm32GPS定位模块\HARDWARE\LED\led.c` | WGS84→GCJ02 坐标转换（含校准偏移） |
| `learn\00-项目总览.md` ~ `04-学习路线图.md` | 项目文档，最全面的参考 |
| `learn\03-通信协议详解.md` | OneNET/token/MQTT 协议细节 |
| `EC200U-CN AT指令.txt`、`AT+CPIN.txt` | AT 指令手册与一次真实的手动联机记录 |
| `onenet_token.py`（本目录新增） | OneNET 设备 token 生成脚本 |
