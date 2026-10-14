# 03 - 通信协议详解

本文详细讲解项目中涉及的各通信协议和数据格式，帮助你理解"数据是怎么在各个模块之间流动的"。

---

## 一、AT 指令通信流程

STM32 通过 UART2（PA2-TX, PA3-RX）与 BC20/EC200U-CN 4G 模块通信，所有指令都以 `\r\n` 结尾。

### 1.1 初始化序列

上电后，STM32 通过 PA12 控制 4G 模块开机（拉高 1 秒→拉低），然后依次执行：

```
发送: ATE1\r\n              ← 开启回显，测试通信
期望: OK

发送: ATI\r\n               ← 查询模块型号
期望: EC200U-CN

发送: AT+CIMI\r\n           ← 读取 SIM 卡 IMSI
期望: 46000xxxxxxxxxx       ← 460 开头 = 中国移动

发送: AT+CGATT?\r\n         ← 查询网络注册状态
期望: +CGATT: 1             ← 1 = 已附着网络

发送: AT+CSQ\r\n            ← 查询信号质量
期望: +CSQ: 18,0            ← 第一个值 0~31，越大越好

发送: AT+QGPS=1\r\n         ← 开启 GPS
期望: OK
```

### 1.2 GPS 数据读取

```
发送: AT+QGPSLOC=2\r\n
期望响应:
+QGPSLOC: 084235.0,39.915123,116.404567,15.3,0.0,0.0,12,0.5,0.0,...
```

响应字段含义（逗号分隔）：
| 位置 | 含义 | 示例 |
|------|------|------|
| 0 | UTC 时间 | 084235.0 |
| 1 | 纬度 | 39.915123 |
| 2 | 经度 | 116.404567 |
| 3 | 定位精度因子 | 15.3 |
| 4 | 海拔高度（米） | 0.0 |
| 5 | 速度（km/h） | 0.0 |
| 6 | 卫星数 | 12 |
| 7 | 方向角 | 0.5 |

代码中使用 `sscanf` 提取：
```c
sscanf(gps_start + 9, "%*[^,],%lf,%lf,%*f,%lf,%*[^,],%lf,%lf",
       &lat, &lon, &alt, &speed, &dir);
```
- `%*[^,]` → 跳过 UTC 时间字段
- `%lf` → 读取 double 值（纬度、经度）
- `%*f` → 跳过精度因子
- 后续读取海拔、速度、方向

### 1.3 MQTT 连接与数据上传序列

```
AT+QMTDISC=0\r\n                    ← 断开旧连接
AT+QMTCLOSE=0\r\n                   ← 关闭旧会话
AT+QMTCFG="version",0,4\r\n         ← 设置 MQTT v3.1.1

AT+QMTOPEN=0,"mqtts.heclouds.com",1883\r\n   ← 打开 TCP 连接到 OneNet
等待: +QMTOPEN: 0,0                           ← 0,0 表示成功

AT+QMTCONN=0,"your_device_name","your_product_id","<token>"\r\n  ← MQTT 登录
等待: +QMTCONN: 0,0,0                                 ← 0,0,0 表示成功
```

---

## 二、UART 通信配置详解

### 2.1 基本参数

所有串口统一配置：
- **波特率**：115200 bps
- **数据位**：8 位
- **停止位**：1 位
- **校验**：无（None）
- **流控**：无

### 2.2 接收机制

串口使用**中断接收**方式，核心结构：

```c
// 每个 UART 有一个接收缓冲区
typedef struct {
    uint8_t buf[BUFLEN];   // 数据缓冲区
    uint16_t index;        // 当前写入索引
} Buffer_TypeDef;

Buffer_TypeDef buf_uart2;  // UART2 的缓冲区（与 4G 模块通信）
```

**工作流程**：
1. 每收到一个字节，触发 UART 中断
2. 中断服务函数将字节存入 `buf_uart2.buf[index]`，index++
3. 主程序读取 `buf_uart2.buf` 处理完整响应
4. 处理完毕后调用 `Clear_Buffer()` 清空缓冲区

### 2.3 发送函数

```c
void Uart2_SendStr(char *str);    // 发送字符串
void USART2_Send(char *data, int len);  // 发送指定长度数据
void UART2_send_byte(uint8_t byte);     // 发送单字节
```

---

## 三、OneNet 平台接入流程

### 3.1 平台信息

- **平台**：中国移动 OneNet（物联网开放平台）
- **协议**：MQTT v3.1.1
- **服务器**：`mqtts.heclouds.com:1883`
- **产品 Key**：`your_product_id`
- **设备名**：`your_device_name`

### 3.2 认证方式

使用 Token 认证（version=2018-10-31 签名方式）：
```
MQTT Client ID: 设备名（your_device_name）
MQTT Username:  产品 Key（your_product_id）
MQTT Password:  签名 Token
```

Token 生成规则：
```
version=2018-10-31
&res=products%2F{产品ID}%2Fdevices%2F{设备名}
&et=3778667000           ← 过期时间戳
&method=md5              ← 签名算法
&sign=<MD5签名结果>       ← URL 编码后的 Base64
```

### 3.3 Topic 定义

| Topic | 方向 | 用途 |
|-------|------|------|
| `$sys/{产品Key}/{设备名}/thing/property/post` | 设备→云端 | 上报属性数据 |
| `$sys/{产品Key}/{设备名}/thing/property/set` | 云端→设备 | 云端下发控制指令 |

实际 Topic 路径：
```
$sys/your_product_id/your_device_name/thing/property/post   ← 数据上报
$sys/your_product_id/your_device_name/thing/property/set    ← 指令订阅
```

### 3.4 数据上传流程

```
1. AT+QMTSUB=0,1,"$sys/.../set",2       ← 订阅控制 Topic
2. AT+QMTPUBEX=0,0,0,0,"$sys/.../post",<长度>  ← 准备发布
3. 等待 ">" 提示符
4. 发送 JSON 数据
5. 发送 0x1A（Ctrl+Z）结束
6. 等待 "+QMTPUBEX: 0,0,0" 确认
```

---

## 四、ESP8266 通信协议

### 4.1 Blinker 协议

Blinker 使用 MQTT 连接点灯科技自有云平台：
- **设备密钥**：`your_blinker_auth`
- 组件通过 Key 标识：`btn-abc`（按钮）、`num-abc`（数值显示）
- 按钮回调：`button1_callback()` → 翻转 GPIO0 电平
- 数据回调：`dataRead()` → 接收 App 发来的任意数据

### 4.2 HTTP 接口

Web 服务器监听 80 端口，提供 RESTful 风格接口：

| 路径 | 方法 | 功能 | 响应 |
|------|------|------|------|
| `/` | GET | 返回控制页面 | HTML 页面 |
| `/on` | GET | 打开设备 | "已打开" |
| `/off` | GET | 关闭设备 | "已关闭" |
| `/status` | GET | 查询状态 | "已打开" 或 "已关闭" |
| 其他 | GET | 404 | "页面未找到！" |

前端 JavaScript 每 2 秒轮询 `/status` 刷新页面状态。

---

## 五、WGS84 → GCJ02 坐标转换原理

### 5.1 为什么需要转换？

- GPS 模块输出的是 **WGS84** 坐标（国际通用坐标系）
- 中国法规要求地图使用 **GCJ02** 坐标（国测局加密坐标系，又称"火星坐标系"）
- 高德地图使用 GCJ02，百度地图使用 BD09（GCJ02 再加一次偏移）
- 不转换会导致地图上出现几百米的偏差

### 5.2 转换算法

代码在 `led.c` 的 `wgs84_to_gcj02()` 函数中实现：

```c
// 核心参数
#define PI       3.1415926535897932384626
#define EARTH_A  6378245.0           // 长半轴
#define EARTH_EE 0.00669342162296594323  // 偏心率平方

// 转换步骤
1. 计算相对于中国中心（105°E, 35°N）的经纬度差值
2. 用多项式 + 三角函数计算偏移量 delta_lon, delta_lat
3. 将偏移量转换为经纬度增量（考虑地球曲率）
4. 迭代 3 次精化
5. 加上固定微调偏移（EAST_OFFSET, NORTH_OFFSET）
```

`get_delta()` 函数中的偏移量计算使用了：
- 线性项：`dLon + 2*dLat`
- 二次项：`0.1*dLon² + 0.1*dLon*dLat`
- 三角函数项：`sin(6π*dLon)`, `sin(2π*dLon)` 等多次谐波
- 平方根项：`sqrt(|dLon|)`

---

## 六、数据格式（JSON Payload）

### 6.1 OneNet 上报数据格式

由 `MakeOnenetPayload()` 函数生成：

```json
{
  "id": "123",
  "params": {
    "temp": {"value": 25},
    "humi": {"value": 60},
    "latitude": {"value": 39.915123},
    "longitude": {"value": 116.404567},
    "altitude": {"value": 15.30},
    "speed": {"value": 0.50},
    "direction": {"value": 180.0}
  }
}
```

字段说明：
| 字段 | 类型 | 说明 |
|------|------|------|
| id | string | 消息标识（固定 "123"） |
| temp | int | 温度（°C，0~255） |
| humi | int | 湿度（%，0~255） |
| latitude | double | GCJ02 纬度（6 位小数） |
| longitude | double | GCJ02 经度（6 位小数） |
| altitude | double | 海拔高度（米，2 位小数） |
| speed | double | 速度（m/s，2 位小数） |
| direction | double | 方向角（0~360°，1 位小数） |

### 6.2 Python 串口数据格式

**发送格式**（send.py → STM32）：
```
NAV,116.404,39.915\n
```
格式：`NAV,经度,纬度\n`

**接收格式**（STM32 → receive.py）：
```
GPS,纬度,经度,...*校验和\r\n
```
格式：以 `GPS` 开头，逗号分隔，`*` 后为校验和，`\r\n` 结尾

---

## 七、通信时序总结

```
上电 → STM32 初始化 → 4G 模块开机（PA12 时序）
  → AT 指令测试通信 → SIM 卡检查 → 网络注册
  → GPS 开启 → 等待定位
  → MQTT 连接 OneNet
  → 进入主循环:
      读 DHT11 → 读 GPS → 坐标转换 → 生成 JSON → MQTT 发布
      → 等待 1 秒 → 重复
```

整个循环周期约 **1.5~2 秒**（500ms GPS 等待 + 1000ms 延时 + 各步骤处理时间）。
