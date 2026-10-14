# 02 - 代码阅读指南

本文按执行顺序逐行讲解核心代码，帮助你理解"程序是怎么跑起来的"。建议对照源码文件一起阅读。

---

## 一、main.c 逐行讲解（80 行）

### 1.1 头文件与变量声明（第 1~6 行）

```c
#include "main.h"

int main(void)
{
    double lat, lon, bd_lat, bd_lon;
    double alt = 0.0, speed = 0.0, dir = 0.0;
```

- `main.h` 汇总了所有模块的头文件声明
- `lat/lon`：GPS 原始 WGS84 坐标（纬度/经度）
- `bd_lat/bd_lon`：转换后的 GCJ02 坐标（高德坐标系）
- `alt`：海拔高度（米）
- `speed`：移动速度（米/秒）
- `dir`：运动方向角度（0~360°）

### 1.2 系统初始化阶段（第 8~36 行）

```c
delay_init();           // 初始化延时系统（SysTick 定时器）
NVIC_Configuration();   // 配置中断优先级分组
CSTX_4GCTR_Init();      // 初始化 4G 模块控制引脚（PA12），执行开机时序
```

**串口初始化**（按顺序逐个初始化 3 路串口）：
```c
uart_init(115200);      // UART1 - 调试输出
Uart1_SendStr("UART1 Init Successful\r\n");
delay_ms(1200);

LED_Init();              // LED 引脚初始化（PB3/PB4/PB5）

uart2_init(115200);      // UART2 - BC20 4G 模块
Uart2_SendStr("UART2 Init Successful\r\n");
delay_ms(1200);

uart3_init(115200);      // UART3 - 树莓派通信
Uart3_SendStr("UART3 Init Successful\r\n");
```

**看门狗初始化**：
```c
IWDG_Init(6, 500);  // 预分频 64，重装值 500，防止程序跑飞
```

**4G 模块初始化与云平台注册**：
```c
CSTX_4G_Init();           // AT 指令测试通信、检查 SIM 卡、网络附着、开启 GPS
CSTX_4G_RegALIYUNIOT();   // 连接 OneNet MQTT 服务器
```

**其他初始化**：
```c
Clear_Buffer_UART1();     // 清空 UART1 缓冲区
DHT11_Init();             // DHT11 传感器引脚初始化（PA11）
Get_4GIMEI_NUM();         // 获取 4G 模块 IMEI 号
```

**复位原因检测**（第 34~36 行）：
```c
if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST)!=RESET)  // 看门狗复位
if(RCC_GetFlagStatus(RCC_FLAG_PORRST)!=RESET)    // 上电复位
if(RCC_GetFlagStatus(RCC_FLAG_PINRST)!=RESET)    // NRST 引脚复位
```
这些代码帮助调试时判断"为什么芯片重启了"。

### 1.3 主循环详解（第 38~79 行）

主循环是一个无限循环 `while(1)`，每次循环完成一次"采集→转换→上传"的完整流程。

**步骤 1：喂狗**
```c
IWDG_Feed();  // 告诉看门狗"我还活着"
```

**步骤 2：读取温湿度**
```c
DHT11_Read_TempAndHumidity();  // 读取结果存入 DHT11_Data 结构体
```

**步骤 3：读取 GPS 数据**
```c
Clear_Buffer();                           // 清空 UART2 缓冲区
Uart2_SendStr("AT+QGPSLOC=2\r\n");       // 向 BC20 发送 GPS 读取指令
delay_ms(500);                            // 等待模块响应
IWDG_Feed();                              // 再喂一次狗（防止等待超时）
```

**步骤 4：解析 GPS 数据**
```c
if (strstr((char*)buf_uart2.buf, "+QGPSLOC:"))  // 检查响应中是否包含定位数据
{
    char *gps_start = strstr((char*)buf_uart2.buf, "+QGPSLOC:");
    // sscanf 从响应字符串中提取经纬度、海拔、速度、方向
    if (sscanf(gps_start + 9, "%*[^,],%lf,%lf,%*f,%lf,%*[^,],%lf,%lf",
               &lat, &lon, &alt, &speed, &dir) >= 2)
    {
        wgs84_to_amap(lon, lat, &bd_lon, &bd_lat);  // WGS84 → GCJ02
    }
    else { lat = 0; lon = 0; }  // 解析失败
}
else { lat = 0; lon = 0; }  // 未获取到 GPS 信号
```

> `sscanf` 中的 `%*[^,]` 表示跳过该字段（读取但不存储），`%lf` 表示读取 double 值。

**步骤 5：上传数据到 OneNet**
```c
CSTX_4G_ALYIOTSenddata(
    DHT11_Data.temp_int,   // 温度整数部分
    DHT11_Data.humi_int,   // 湿度整数部分
    bd_lat, bd_lon,        // 转换后的坐标
    alt, speed, dir        // 海拔、速度、方向
);
```

**步骤 6：等待下一轮**
```c
IWDG_Feed();
delay_ms(1000);  // 间隔 1 秒后开始下一轮
```

---

## 二、4G.c 驱动代码阅读要点

### 2.1 初始化流程（CSTX_4G_Init）

初始化是一个"带重试的 AT 指令序列"，每一步都有 `while` 循环重试直到成功：

1. **ATE1** → 测试与模块的串口通信
2. **ATI** → 查询模块型号
3. **AT+CIMI** → 检查 SIM 卡（响应包含 "460" 表示中国移动）
4. **AT+CGATT?** → 检查网络附着（`+CGATT: 1` 表示已注册网络）
5. **AT+CSQ** → 查询信号质量
6. **AT+QGPS=1** → 开启 GPS 模块
7. **AT+QGPS?** → 轮询等待 GPS 定位成功（最多重试 10 次）

### 2.2 MQTT 注册流程（CSTX_4G_RegALIYUNIOT）

```
AT+QMTDISC=0        → 断开旧连接
AT+QMTCLOSE=0       → 关闭旧会话
AT+QMTCFG="version" → 设置 MQTT 版本
AT+QMTOPEN=0,...    → 打开到 mqtts.heclouds.com:1883 的连接
AT+QMTCONN=0,...    → 用设备名/产品Key/密钥登录
```

### 2.3 数据上传流程（CSTX_4G_ALYIOTSenddata）

1. 调用 `MakeOnenetPayload()` 生成 JSON 数据
2. `AT+QMTSUB` → 订阅属性设置 Topic（接收云端指令）
3. `AT+QMTPUBEX` → 准备发布数据到属性上报 Topic
4. 等待模块返回 `>` 提示符
5. 发送 JSON 数据 + `0x1A`（Ctrl+Z，表示发送结束）
6. 验证 `+QMTPUBEX: 0,0,0` 确认发送成功

---

## 三、led.c 中的坐标转换代码

> 注意：坐标转换代码放在了 `HARDWARE/LED/led.c` 中，这是历史原因。

### 3.1 核心函数

```c
void wgs84_to_amap(double wgs_lon, double wgs_lat, 
                   double *amap_lon, double *amap_lat)
{
    wgs84_to_gcj02(wgs_lon, wgs_lat, amap_lon, amap_lat);
}
```

`wgs84_to_amap` 是对 `wgs84_to_gcj02` 的简单封装。

### 3.2 转换算法

1. 计算经度/纬度相对于中国中心（105°E, 35°N）的偏移
2. 使用复杂的多项式 + 三角函数计算偏移量（`get_delta` 函数）
3. 迭代 3 次精化结果
4. 最后加上固定的东南偏移量（`EAST_OFFSET=0.000365`, `NORTH_OFFSET=0.000275`）

---

## 四、ESP8266 代码结构（esp8266.ino，169 行）

### 4.1 代码结构

```
├── 配置区：Blinker 密钥、WiFi 信息、HTML 页面
├── 函数声明
├── Blinker 回调函数（button1_callback, dataRead）
├── Web 服务器回调函数（handleRoot, handleOn, handleOff, handleStatus）
├── setup()：初始化串口、GPIO、Blinker、Web 服务器
└── loop()：Blinker.run() + server.handleClient()
```

### 4.2 关键逻辑

- `setup()` 中同时初始化了 Blinker 和 Web 服务器
- `loop()` 只有两行：Blinker 处理 + Web 客户端处理
- Web 页面通过 JavaScript `fetch()` 发送 HTTP 请求控制继电器
- 页面每 2 秒自动刷新设备状态

---

## 五、Python 脚本说明

### 5.1 send.py（发送 GPS 数据）
- 通过串口向 STM32 发送模拟 GPS 数据
- 数据格式：`NAV,经度,纬度\n`
- 每 3 秒发送一次

### 5.2 receive.py（接收 GPS 数据）
- 从串口接收 STM32 发来的 GPS 帧
- 按 `\r\n` 分割帧，解析以 `GPS` 开头且包含 `*` 校验的数据
- 输出经度和纬度

---

## 六、关键数据结构

### 6.1 DHT11 数据结构
```c
typedef struct {
    uint8_t humi_int;    // 湿度整数
    uint8_t humi_deci;   // 湿度小数
    uint8_t temp_int;    // 温度整数
    uint8_t temp_deci;   // 温度小数
    uint8_t check_sum;   // 校验和
} DHT11_Data_TypeDef;
```

### 6.2 4G 模块状态结构
```c
typedef struct {
    uint8_t CSQ;          // 信号质量
    uint8_t Socketnum;    // Socket 连接数
    uint8_t reclen;       // 接收数据长度
    uint8_t netstatus;    // 网络状态指示
    uint8_t recdata[100]; // 接收数据缓冲
} CSTX_4G;
```

### 6.3 串口缓冲区
```c
// 每个串口有一个缓冲区结构
typedef struct {
    uint8_t buf[BUFLEN];  // 数据缓冲
    uint16_t index;       // 当前写入位置
} Buffer_TypeDef;
```

---

## 七、推荐阅读顺序

建议按以下顺序阅读代码，由浅入深：

1. **main.c** → 了解整体流程
2. **wdg.c** → 最简单的驱动，理解看门狗
3. **led.c** → GPIO 操作 + 坐标转换算法
4. **dht11.c** → 单总线协议，理解时序驱动编写
5. **4G.c** → AT 指令交互，理解 MQTT 通信
6. **usart.c** → 串口中断接收机制
7. **esp8266.ino** → WiFi + Web 服务器
8. **send.py / receive.py** → Python 串口通信
