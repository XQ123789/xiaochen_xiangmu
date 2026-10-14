# GPS实时定位项目 — 进阶改进与扩展规划

## 一、当前项目状态评估

### 1.1 第一版改进已完成工作

项目已完成从原型到工程化的基础改进（详见 `improve/` 目录），核心成果包括：

- **状态机架构**：`main.c` 引入 `INIT→GPS_READ→GPS_PROCESS→DATA_UPLOAD→IDLE→ERROR_RECOVERY` 六态状态机，替代原项目的线性轮询结构
- **错误处理与超时保护**：`bc20_improved.c` 中 `AT_SendAndWait()` 实现带超时重试的AT指令框架，消除原 `4G.c` 中 `while` 无限循环死锁风险
- **配置集中化**：`config.h` 统一管理服务器地址、UART波特率、缓冲区大小、超时参数等，消除魔数散落问题
- **凭据安全隔离**：OneNET密钥、WiFi凭据从源码移至 `config.h`（配合 `.gitignore`）
- **GPS无效数据保护**：GPS解析失败时不再发送零坐标 `(0,0)`，避免污染云端数据
- **ESP8266改进**：WiFi断线自动重连、状态LED指示、软件看门狗保护

### 1.2 当前仍存在的不足

| 维度 | 具体问题 |
|------|----------|
| 数据质量 | GPS原始坐标无滤波，存在3-10m跳变噪声 |
| 通信可靠性 | MQTT QoS 0不保证送达，无心跳保活，断线无感知 |
| 通信效率 | UART字节中断CPU开销大（3路UART约34500次/秒中断）；STM32↔树莓派文本协议带宽浪费严重 |
| 安全性 | MQTT明文传输（端口1883），无TLS加密 |
| 电源管理 | 全速运行无休眠，固定1秒采样率，无法适配电池供电 |
| 功能单一 | 无地理围栏、无路径规划、无本地存储、无远程命令控制 |
| 架构局限 | 单层FSM难以扩展；坐标转换仍在 `led.c` 中（改进版未迁移）；阻塞式 `delay_ms()` 仍大量使用 |

---

## 二、可引入的算法

### 2.1 GPS数据滤波算法

#### 2.1.1 中值滤波（推荐首先实现）

**原理**：维护一个固定大小的滑动窗口，对窗口内的坐标值排序后取中值，有效消除脉冲型跳变噪声。

**为什么适合GPS数据**：GPS多径效应和信号遮挡会产生离群跳变点（如瞬间偏移50m），中值滤波对离群值天然免疫，不会像均值滤波那样被极端值拉偏。

**实现复杂度**：低（约30行C代码）  
**资源需求**：约100字节RAM（窗口大小5-7个double坐标点）  
**预期效果**：完全消除单点GPS跳变，定位轨迹平滑度提升60%以上

**代码示例**：
```c
/* gps_filter.h */
#define MEDIAN_WINDOW_SIZE  5

typedef struct {
    double lat_buf[MEDIAN_WINDOW_SIZE];
    double lon_buf[MEDIAN_WINDOW_SIZE];
    uint8_t index;
    uint8_t count;
} MedianFilter_t;

void median_filter_update(MedianFilter_t *f, double lat, double lon);
double median_filter_get_lat(MedianFilter_t *f);
double median_filter_get_lon(MedianFilter_t *f);

/* gps_filter.c */
static double median_sort(double *arr, uint8_t len) {
    /* 冒泡排序（窗口小，无需复杂排序） */
    for (uint8_t i = 0; i < len - 1; i++)
        for (uint8_t j = 0; j < len - 1 - i; j++)
            if (arr[j] > arr[j+1]) {
                double t = arr[j]; arr[j] = arr[j+1]; arr[j+1] = t;
            }
    return arr[len / 2];
}

void median_filter_update(MedianFilter_t *f, double lat, double lon) {
    f->lat_buf[f->index] = lat;
    f->lon_buf[f->index] = lon;
    f->index = (f->index + 1) % MEDIAN_WINDOW_SIZE;
    if (f->count < MEDIAN_WINDOW_SIZE) f->count++;
}

double median_filter_get_lat(MedianFilter_t *f) {
    double tmp[MEDIAN_WINDOW_SIZE];
    memcpy(tmp, f->lat_buf, sizeof(double) * f->count);
    return median_sort(tmp, f->count);
}
```

**集成位置**：在 `main.c` 的 `STATE_GPS_PROCESS` 状态中，`wgs84_to_amap()` 之后调用滤波，再送入上传状态。

#### 2.1.2 卡尔曼滤波（推荐第二步实现）

**原理**：基于2D位置+速度的状态空间模型，通过预测-更新两步递推，在噪声观测值中估计出最优轨迹。状态向量为 `[lat, lon, v_lat, v_lon]`，利用GPS提供的速度观测值辅助位置平滑。

**为什么最适合GPS轨迹平滑**：中值滤波只能去跳变，无法消除连续噪声；卡尔曼滤波利用运动模型预测下一时刻位置，结合观测值加权融合，可在保留轨迹真实走向的同时将噪声从3-10m降至1-3m。

**实现复杂度**：中等（需矩阵运算，约120行C代码）  
**资源需求**：约2KB RAM（状态矩阵4×4 + 协方差矩阵），约4KB Flash  
**预期效果**：GPS噪声降至1-3m，轨迹平滑度显著提升

**代码示例**：
```c
/* kalman_gps.h */
typedef struct {
    double x[4];       /* 状态: [lat, lon, v_lat, v_lon] */
    double P[4][4];    /* 协方差矩阵 */
    double Q[4][4];    /* 过程噪声 */
    double R[2][2];    /* 观测噪声 */
    uint32_t last_time;
    uint8_t initialized;
} KalmanGPS_t;

void kalman_gps_init(KalmanGPS_t *k);
void kalman_gps_update(KalmanGPS_t *k, double lat, double lon,
                       double v_lat, double v_lon, uint32_t dt_ms);

/* kalman_gps.c - 核心预测+更新 */
void kalman_gps_update(KalmanGPS_t *k, double lat, double lon,
                       double v_lat, double v_lon, uint32_t dt_ms)
{
    double dt = dt_ms / 1000.0;

    if (!k->initialized) {
        k->x[0] = lat; k->x[1] = lon;
        k->x[2] = v_lat; k->x[3] = v_lon;
        /* 初始化P为单位阵 * 100 */
        for (int i = 0; i < 4; i++) k->P[i][i] = 100.0;
        k->initialized = 1;
        return;
    }

    /* 预测步骤：x = F*x, P = F*P*F' + Q */
    double x_pred[4];
    x_pred[0] = k->x[0] + k->x[2] * dt;
    x_pred[1] = k->x[1] + k->x[3] * dt;
    x_pred[2] = k->x[2];
    x_pred[3] = k->x[3];

    /* 更新步骤：用观测值修正预测 */
    double y_lat = lat - x_pred[0];
    double y_lon = lon - x_pred[1];
    double S = k->P[0][0] + k->R[0][0];  /* 简化：仅位置观测 */
    double K_lat = k->P[0][0] / S;
    double K_lon = k->P[1][1] / (k->P[1][1] + k->R[1][1]);

    k->x[0] = x_pred[0] + K_lat * y_lat;
    k->x[1] = x_pred[1] + K_lon * y_lon;
    k->x[2] = x_pred[2] + K_lat * y_lat / dt;
    k->x[3] = x_pred[3] + K_lon * y_lon / dt;
}
```

#### 2.1.3 滑动平均滤波

**原理**：对最近N个GPS坐标取算术平均值，实现简单但对持续漂移的抑制效果有限。适合作为中值滤波的补充，进一步平滑输出。  
**资源需求**：与中值滤波类似，约100字节RAM  
**建议**：可先用滑动平均快速验证效果，再过渡到中值滤波+卡尔曼滤波方案。

### 2.2 路径规划算法（树莓派端）

#### 2.2.1 A*算法（推荐）

**原理**：A*算法是一种启发式搜索算法，通过评估函数 `f(n) = g(n) + h(n)` 选择最优路径节点，其中 `g(n)` 为起点到当前节点的实际代价，`h(n)` 为当前节点到终点的估计代价（通常用欧几里得距离或曼哈顿距离）。

**适用场景**：项目已有航点导航系统（见 `树莓派导航操作流程.txt` 中预存的23个路径坐标点），A*可在栅格地图或航点图上规划最优路径。

**为什么在树莓派上运行**：STM32F103ZET6仅有64KB RAM + 512KB Flash，无法承载地图数据和搜索开销；树莓派4B拥有2-8GB RAM和完整Linux环境，可运行OSMnx/OpenStreetMap进行实时路径规划。

**与项目集成方式**：树莓派通过UART3（PB10/PB11）将规划好的航点序列下发给STM32，STM32按航点逐点导航。

**Python伪代码**：
```python
import heapq
import math

def astar(grid, start, goal, obstacle_cells):
    """栅格地图A*路径规划"""
    open_set = [(0, start)]
    came_from = {}
    g_score = {start: 0}

    while open_set:
        _, current = heapq.heappop(open_set)
        if current == goal:
            return reconstruct_path(came_from, current)

        for neighbor in get_neighbors(current, grid):
            if neighbor in obstacle_cells:
                continue
            tentative_g = g_score[current] + distance(current, neighbor)
            if tentative_g < g_score.get(neighbor, float('inf')):
                came_from[neighbor] = current
                g_score[neighbor] = tentative_g
                f = tentative_g + heuristic(neighbor, goal)
                heapq.heappush(open_set, (f, neighbor))
    return None

def heuristic(a, b):
    return math.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)
```

#### 2.2.2 Dijkstra算法

与A*相比，Dijkstra不使用启发式函数，搜索范围更大，效率较低。适合节点数较少（<100航点）的简单场景。项目中航点导航可直接使用Dijkstra作为A*的降级方案。

#### 2.2.3 D* Lite（动态环境重规划）

**适用场景**：当小车在行驶过程中发现新的障碍物（通过超声波/摄像头检测到），D* Lite可在已有路径上局部重规划，无需全局重新搜索。适合室外动态环境。

### 2.3 定位增强算法

#### 2.3.1 航位推算（Dead Reckoning）

**原理**：利用GPS有效时获取的速度和方向数据（`AT+QGPSLOC=2` 返回的 `speed` 和 `direction` 字段），在GPS信号短暂丢失时通过积分推算当前位置。

**实现方式**：现有硬件即可实现，无需额外传感器。公式：`lat_new = lat_old + speed * cos(direction) * dt / R_earth`。  
**有效时长**：5-30秒短时推算，误差随时间累积（约每秒增加1-2m）。  
**集成位置**：`main.c` 的 `STATE_GPS_READ` 中，GPS无响应时切换为航位推算模式。

#### 2.3.2 地图匹配（Map Matching）

**原理**：将GPS原始坐标吸附到最近的道路上，消除道路外的漂移。  
**实现方式**：树莓派端使用OSMnx加载OpenStreetMap路网数据，对每个GPS点查找最近道路线段并投影。  
**预期效果**：在城市道路场景中，定位精度可提升至5m以内（沿道路方向）。

#### 2.3.3 基站定位备用

**原理**：利用EC200U-CN的 `AT+QENG="servingcell"` 指令获取当前服务基站的CID/LAC/TAC信息，通过基站定位API获取粗略位置。  
**精度**：约100-500m（城市环境），适合GPS完全不可用时的应急定位。  
**实现**：在 `bc20_improved.c` 中添加基站定位查询函数，GPS连续失败超过10次时自动启用。

### 2.4 数据压缩算法

#### 2.4.1 Douglas-Peucker轨迹压缩

**原理**：递归地将轨迹点序列简化——找到距离首尾连线最远的点，若距离小于阈值则丢弃中间所有点，否则保留最远点并递归处理两侧子段。

**效果**：可减少60-80%的数据点，大幅降低MQTT上传载荷和云端存储开销。  
**资源需求**：STM32上可实现50点滑动窗口，约2KB RAM。

**代码示例**：
```c
/* douglas_peucker.h */
typedef struct { double lat, lon; } GeoPoint_t;

double perpendicular_distance(GeoPoint_t p, GeoPoint_t line_start, GeoPoint_t line_end);
int douglas_peucker(GeoPoint_t *points, int n, double epsilon, uint8_t *keep);

/* douglas_peucker.c */
int douglas_peucker(GeoPoint_t *points, int n, double epsilon, uint8_t *keep) {
    if (n < 3) { for (int i = 0; i < n; i++) keep[i] = 1; return n; }

    double max_dist = 0;
    int max_idx = 0;
    for (int i = 1; i < n - 1; i++) {
        double d = perpendicular_distance(points[i], points[0], points[n-1]);
        if (d > max_dist) { max_dist = d; max_idx = i; }
    }

    if (max_dist > epsilon) {
        int left = douglas_peucker(points, max_idx + 1, epsilon, keep);
        int right = douglas_peucker(points + max_idx, n - max_idx, epsilon, keep + max_idx);
        return left + right - 1;  /* max_idx点被计算两次 */
    } else {
        keep[0] = 1; keep[n-1] = 1;
        for (int i = 1; i < n-1; i++) keep[i] = 0;
        return 2;
    }
}
```

#### 2.4.2 差分编码

**原理**：发送相邻坐标点的增量（Δlat, Δlon）而非绝对值。GPS坐标double需16字节，差分后通常只需4-6字节（int16/int32），可减少50-70%载荷大小。  
**实现**：STM32端编码，树莓派/云端解码还原。

### 2.5 电源管理算法

#### 自适应采样率

**原理**：根据运动状态动态调整GPS采样频率：

| 运动状态 | 判定条件 | 采样间隔 | 功耗占比 |
|----------|----------|----------|----------|
| 静止 | speed < 0.5 m/s 持续30s | 30秒/次 | ~10% |
| 低速移动 | 0.5 ≤ speed < 3 m/s | 5秒/次 | ~40% |
| 高速移动 | speed ≥ 3 m/s | 1-2秒/次 | ~100% |

**预期效果**：综合节省40-50%功耗，显著延长电池供电时间。  
**集成位置**：`main.c` 的 `STATE_IDLE` 中，根据当前 `speed` 值动态设置 `LOOP_INTERVAL_MS`。

### 2.6 异常检测算法

#### GPS欺骗检测

- **位置跳变检测**：相邻两次定位距离差 > 100m 时标记为可疑
- **速度一致性校验**：GPS速度与航位推算速度偏差 > 50% 时告警
- **信号强度分析**：通过 `AT+QGPS` 查询卫星信噪比，SNR异常低时标记

#### 传感器故障检测

- **DHT11数据范围校验**：温度 -20~60°C、湿度 20-95% 为有效范围
- **变化率限制**：相邻两次温度变化 > 5°C/s 视为异常，保留上次有效值

---

## 三、可添加的新硬件模块

### 3.1 传感器类

| 模块 | 功能 | 用途 | 接口 | 难度 | 资源需求 |
|------|------|------|------|------|----------|
| MPU6050/BMI160 | 加速度+陀螺仪 | 航位推算、姿态检测、卡尔曼滤波融合 | I2C/SPI | 中等 | ~1KB RAM |
| HC-SR04 | 超声波测距(2-400cm) | 避障 | GPIO(TIM捕获) | 低 | 极少 |
| 红外避障模块 | 近距离障碍物检测 | 避障辅助 | GPIO | 低 | 极少 |
| 光敏/雨量传感器 | 环境光/天气检测 | 环境感知、自动启停 | ADC/GPIO | 低 | 极少 |

**引脚分配建议**：STM32F103ZET6当前已使用PA2/PA3(UART2)、PA9/PA10(UART1)、PA11(DHT11)、PB10/PB11(UART3)。MPU6050可接PB6/PB7(I2C1)，HC-SR04可接PA8(TIM1_CH1)。

### 3.2 存储与通信类

| 模块 | 功能 | 用途 | 接口 | 难度 |
|------|------|------|------|------|
| SD卡模块 | 本地数据日志 | 断网数据缓存、轨迹记录回放 | SPI | 中等 |
| LoRa(SX1278) | 远距离低功耗通信(>5km) | 4G不可用时的备用通信 | SPI | 中等 |
| NFC/RFID(RC522) | 近场通信 | 身份验证、打卡定位 | SPI | 低 |

### 3.3 控制类

| 模块 | 功能 | 用途 | 难度 |
|------|------|------|------|
| 电机编码器 | 测量转速和圈数 | 精确运动控制、航位推算 | 中等 |
| HMC5883L电子罗盘 | 航向角测量 | 精确方向感知（补充GPS方向） | 低 |

**参考**：项目已有CH32V307VCT6的编码器引脚定义（PA2/PA3左编码器、PA6/PA7右编码器），可直接对接TB6612电机驱动实现闭环控制。

### 3.4 视觉类

| 模块 | 功能 | 用途 | 运行端 | 难度 |
|------|------|------|--------|------|
| DECXIN双目摄像头(已有) | 深度感知、障碍物检测 | 视觉避障、SLAM建图 | 树莓派(USB) | 高 |
| YOLO目标检测 | 实时目标识别 | 路况识别、交通标志检测 | 树莓派 | 高 |

项目已有DECXIN-1M-SM-2296V1双目摄像头（见项目文件），可通过 `fswebcam` 采集图像，配合OpenCV/YOLO在树莓派端实现视觉功能。

---

## 四、可添加的新功能特性

### 4.1 地理围栏（Geofencing）

设定多边形电子围栏区域，小车进出围栏时触发报警。实现核心为射线法多边形点包含测试（Ray Casting Algorithm），在STM32上约40行代码即可实现。围栏坐标可通过MQTT远程配置。

### 4.2 远程命令控制

通过MQTT订阅 `$sys/{product}/{device}/cmd` 主题，接收远程命令（重启、配置修改、状态查询、采样率调整等）。当前 `bc20_improved.c` 已实现 `CSTX_4G_SubscribeOnce()` 订阅属性设置Topic，可扩展为命令通道。

### 4.3 OTA固件升级

通过4G网络远程更新STM32固件。方案：外部SPI Flash（W25Q64）存储新固件 → STM32自定义Bootloader校验CRC → 跳转执行。需实现固件分包传输（每包256字节）和断点续传。

### 4.4 本地数据断点续传

网络中断时将GPS数据写入SD卡或内部Flash（STM32F103ZET6有512KB Flash，可用最后几页存储），网络恢复后自动补传。结合Douglas-Peucker压缩，可缓存数小时轨迹数据。

### 4.5 多传感器数据融合显示

通过0.96寸OLED屏（I2C接口，SSD1306驱动）实时显示：GPS坐标、信号质量（卫星数/SNR）、温湿度、运动状态、通信状态。便于现场调试和状态监控。

### 4.6 实时视频流

树莓派 + DECXIN双目摄像头，通过WebRTC或RTSP推流至Web页面，远程查看路况。项目已有摄像头硬件基础（`fswebcam` 可拍照），需升级为视频流方案。

### 4.7 语音报警

SYN6288语音合成模块（UART接口），实现围栏越界报警、低电量提醒、GPS信号丢失提示等语音播报功能。

### 4.8 多机协同

多台小车同时追踪，在OneNET平台上以不同 `DEVICE_NAME` 注册，Web端多图标显示。高级功能：编队行驶、任务分配。

---

## 五、现有代码深度改进方向

### 5.1 架构层面

#### 层次化状态机（HSM）

当前 `main.c` 为单层FSM（6个状态），随着功能增加（避障、围栏、OTA等）状态数将爆炸式增长。建议重构为层次化状态机：

- **顶层状态**：NORMAL（正常工作）、RECOVERY（故障恢复）、SAFE_MODE（安全模式）
- **NORMAL子状态**：NAVIGATING、IDLE、UPLOADING、SENSING
- **RECOVERY子状态**：RECONNECT_4G、REINIT_GPS、SYSTEM_RESET

#### 服务层抽象

引入服务层解耦应用逻辑和硬件驱动：
- `gps_service.c`：封装GPS读取、滤波、坐标转换、航位推算
- `comm_service.c`：封装MQTT连接管理、数据发送、断线重连
- `sensor_service.c`：封装DHT11/IMU/超声波等多传感器统一读取接口

#### 中断优先级重新分配

当前USART2/USART3/TIM3共享优先级0（最高），存在中断嵌套冲突风险：

| 外设 | 当前优先级 | 建议优先级 | 理由 |
|------|-----------|-----------|------|
| USART2(4G模块) | 0 | 1 | AT指令响应需及时处理 |
| USART3(树莓派) | 0 | 2 | 命令接收，允许轻微延迟 |
| TIM3(PWM/定时) | 0 | 0 | 电机控制/系统时基，最高优先 |
| USART1(调试) | 未配置 | 3 | 调试输出，最低优先 |
| EXTI(避障传感器) | 未配置 | 1 | 避障中断需快速响应 |

### 5.2 通信层面

#### UART DMA + 环形缓冲区

当前3路UART均使用字节中断接收，115200波特率下每秒约34500次中断。改用DMA + 环形缓冲区可将CPU占用降低80%以上。STM32F103ZET6的DMA1支持USART1_RX(DMA1_Ch5)、USART2_RX(DMA1_Ch6)、USART3_RX(DMA1_Ch3)。

#### MQTT QoS提升

当前 `AT+QMTPUBEX` 中QoS参数为0（最多一次送达）。GPS定位数据建议改为QoS 1（至少一次送达），确保关键数据不丢失。修改 `bc20_improved.c` 中 `MakeOnenetPayload` 后的发布指令参数即可。

#### MQTT心跳保活

当前MQTT连接无keepalive配置，长时间无数据交互可能被服务器断开。建议：
- 连接时配置 `keepalive=60` 秒
- 监听 `+QMTDISC` 断开通知URC
- 发布前检查连接状态（`AT+QMTSTAT?`）

#### STM32↔树莓派二进制协议

当前UART3使用文本协议（`printf` 格式），每帧坐标约80字节。改为二进制帧格式：

```
[0xAA] [0x55] [CMD] [LEN] [PAYLOAD...] [CRC8]
```

坐标数据帧：CMD=0x01, PAYLOAD = lat(int32) + lon(int32) + speed(int16) + dir(int16) = 12字节，总帧16字节，减少约80%带宽。

### 5.3 安全层面

#### MQTT TLS加密

EC200U-CN支持TLS（通过 `AT+QSSLCFG` 配置SSL上下文），将MQTT连接从端口1883切换到8883，防止坐标数据和凭据被窃听。

#### 设备认证增强

- Token定期轮换（每30天更新 `DEVICE_SECRET`）
- 一机一密：每台设备使用独立凭据，而非共享产品密钥

#### 安全启动

- STM32读保护RDP Level 2（防止Flash内容被读出）
- Bootloader对应用固件做CRC32校验，防止篡改

### 5.4 可靠性层面

#### 分级错误恢复

| 级别 | 错误类型 | 恢复策略 | 示例 |
|------|---------|---------|------|
| L1 | 单次AT超时 | 重试当前指令 | GPS查询超时 |
| L2 | 连续3次失败 | 清空缓冲区重试 | GPS连续解析失败 |
| L3 | 模块无响应 | 硬件复位（PA12拉低→拉高） | 4G模块死机 |
| L4 | 重连失败 | 重新初始化整个4G模块 | SIM卡异常 |
| L5 | 多次L4失败 | 系统软复位（NVIC_SystemReset） | 不可恢复故障 |
| L6 | 反复软复位 | 进入安全模式，仅保持基本功能 | 硬件损坏 |

#### 固件版本管理

在 `config.h` 中定义版本号，MQTT上报时携带版本信息：
```c
#define FW_VERSION_MAJOR  2
#define FW_VERSION_MINOR  0
#define FW_VERSION_PATCH  0
#define FW_VERSION_STRING "2.0.0"
```

### 5.5 可测试性

#### 单元测试

坐标转换算法（`wgs84_to_gcj02`）和JSON生成（`MakeOnenetPayload`）是核心算法，需在PC端验证正确性。建议使用Unity测试框架，以已知坐标对（如北京天安门 WGS84→GCJ02）作为测试用例。

#### 硬件抽象层（HAL）

设计UART接口抽象，便于模拟测试和硬件替换：
```c
typedef struct {
    void (*init)(uint32_t baud);
    void (*send)(const char *data, uint16_t len);
    uint8_t (*available)(void);
    uint8_t (*read)(char *buf, uint16_t max_len);
} UART_HAL_t;
```

---

## 六、行业最佳实践差距分析

| 对比维度 | 商业GPS追踪器标准 | 嵌入式IoT行业标准 | 智能小车典型架构 | 当前项目现状 | 差距等级 |
|----------|------------------|-------------------|-----------------|-------------|---------|
| GPS滤波 | 卡尔曼+中值双重滤波 | 至少中值滤波 | 卡尔曼+IMU融合 | 无滤波 | 大 |
| 采样策略 | 自适应（静止/运动） | 自适应或远程可调 | 固定1-5秒 | 固定1秒 | 中 |
| 通信协议 | MQTT QoS 1 + TLS | MQTT QoS 1 | MQTT QoS 0-1 | QoS 0, 无TLS | 大 |
| 断网处理 | 本地缓存+补传 | SD卡/Flash缓存 | 无 | 无 | 大 |
| 错误恢复 | 分级恢复+远程诊断 | 看门狗+自动重连 | 看门狗 | 基础状态机恢复 | 中 |
| 安全 | TLS+一机一密+安全启动 | TLS+Token认证 | 基本无 | 明文MQTT | 大 |
| 路径规划 | 云端实时规划 | N/A | A*/D* + 避障 | 预存航点 | 中 |
| 功耗管理 | 深度休眠+自适应采样 | STOP/STANDBY模式 | 电机PWM节能 | 全速运行 | 大 |
| OTA升级 | 支持 | Bootloader+外部Flash | 部分支持 | 不支持 | 大 |
| 多传感器融合 | GPS+IMU+轮速计 | GPS+IMU(EKF) | 多传感器融合 | 仅GPS+DHT11 | 中 |

---

## 七、实施路线图

### 第一阶段：算法基础（1-2周）

| 任务 | 文件 | 预期效果 |
|------|------|----------|
| 中值滤波实现 | 新建 `gps_filter.c/h` | 消除GPS跳变噪声 |
| GPS数据质量校验 | `main.c` STATE_GPS_PROCESS | 增加距离跳变检测 |
| 自适应采样率 | `main.c` STATE_IDLE | 节省40-50%功耗 |
| DHT11故障检测 | `main.c` STATE_GPS_PROCESS | 异常数据不再上报 |

### 第二阶段：通信与可靠性（2-3周）

| 任务 | 文件 | 预期效果 |
|------|------|----------|
| UART环形缓冲区 + DMA | `usart.c` 重写 | CPU占用降低80% |
| MQTT QoS 1 + 心跳保活 | `bc20_improved.c` | 数据不丢失，断线可感知 |
| 分级错误恢复（L1-L6） | `main.c` 状态机扩展 | 系统可应对各类故障 |
| TLS加密 | `bc20_improved.c` | 通信数据安全 |

### 第三阶段：高级算法（3-4周）

| 任务 | 文件 | 预期效果 |
|------|------|----------|
| 卡尔曼滤波 | 新建 `kalman_gps.c/h` | GPS噪声降至1-3m |
| Douglas-Peucker轨迹压缩 | 新建 `trajectory.c/h` | 减少60-80%数据量 |
| 航位推算 | `gps_service.c` | GPS丢失时短时定位 |
| STM32↔树莓派二进制协议 | 新建 `protocol.c/h` | 带宽减少80% |

### 第四阶段：新功能（4-6周）

| 任务 | 运行端 | 预期效果 |
|------|--------|----------|
| 地理围栏 | STM32 | 进出区域报警 |
| SD卡数据日志 | STM32 | 断网缓存+轨迹记录 |
| 远程命令控制 | STM32 + OneNET | 远程配置和操控 |
| A*路径规划 | 树莓派 | 实时路径规划 |
| 基站定位备用 | STM32 | GPS不可用时的应急定位 |

### 第五阶段：长期演进（持续）

| 任务 | 说明 |
|------|------|
| FreeRTOS迁移 | 多任务并行，GPS采集/通信/传感器独立运行 |
| OTA固件升级 | 远程更新，Bootloader + 外部Flash |
| 多传感器融合（EKF） | GPS + IMU + 轮速计扩展卡尔曼滤波 |
| 视觉功能 | YOLO目标检测 + 双目深度感知（树莓派端） |
| 多机协同 | 多车追踪、编队行驶 |

---

## 八、推荐资源

### 算法学习

| 资源 | 类型 | 说明 |
|------|------|------|
| 《卡尔曼滤波原理及应用》 | 书籍 | 卡尔曼滤波入门经典，含MATLAB/C代码 |
| 《嵌入式系统GPS定位技术》 | 书籍 | GPS数据处理的工程实践 |
| B站：卡尔曼滤波从入门到放弃 | 视频 | 通俗易懂的卡尔曼滤波讲解 |
| A* Pathfinding Tutorial (GeeksforGeeks) | 文章 | A*算法原理与实现 |

### 嵌入式技术

| 资源 | 类型 | 说明 |
|------|------|------|
| STM32 HAL库DMA教程 | 文档 | ST官方参考手册 + 正点原子教程 |
| FreeRTOS官方文档 | 文档 | 实时操作系统移植和使用 |
| MQTT协议规范 v3.1.1 | 标准 | OASIS官方MQTT协议文档 |
| OneNET开发者文档 | 文档 | 平台API和MQTT接入指南 |

### 树莓派/路径规划

| 资源 | 类型 | 说明 |
|------|------|------|
| OSMnx Python库文档 | 文档 | 基于OpenStreetMap的路网分析和路径规划 |
| ROS Navigation Stack | 框架 | 机器人导航框架，含A*/D*实现 |
| OpenCV + YOLOv5 部署教程 | 文章 | 树莓派上部署轻量目标检测模型 |
