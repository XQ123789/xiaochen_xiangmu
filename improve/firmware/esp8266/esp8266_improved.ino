/**
 * @file    esp8266_improved.ino
 * @brief   ESP8266 控制器 - 改进版（Blinker + Web双端控制）
 * @note    改进自原项目 esp8266/esp8266.ino
 *
 * 主要改进：
 *   1. 凭据从硬编码改为配置结构体，方便集中管理
 *   2. 添加WiFi断线自动重连逻辑
 *   3. 添加连接状态LED指示（板载LED或外接LED）
 *   4. 添加软件看门狗保护，防止系统死锁
 *   5. 代码注释完善，结构清晰
 *   6. GPIO0冲突提醒：GPIO0在ESP8266上是启动模式选择引脚，
 *      上电时拉低会进入Flash下载模式，作为输出使用时需注意
 */

/* ============ Blinker 配置 ============ */
#define BLINKER_PRINT Serial
#define BLINKER_WIFI

#include <Blinker.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

/* ============ 配置结构体 ============ */
/**
 * 设备配置结构体 - 集中管理所有凭据和参数
 * 改进点：原代码将auth、ssid、pswd直接定义为全局char数组，
 *         现改为结构体，便于管理和扩展
 */
typedef struct {
    /* Blinker 配置 */
    const char* blinker_auth;     /* 设备密钥 */
    const char* blinker_ssid;     /* WiFi名称 */
    const char* blinker_pswd;     /* WiFi密码 */

    /* 硬件引脚 */
    uint8_t relay_pin;            /* 继电器/LED控制引脚 */
    uint8_t status_led_pin;       /* 状态指示LED引脚 */

    /* WiFi 重连参数 */
    uint32_t wifi_reconnect_ms;   /* WiFi重连间隔(毫秒) */
    uint8_t  wifi_max_retry;      /* WiFi最大重试次数 */
} DeviceConfig_t;

/* 全局配置实例 - 修改此处即可适配不同环境 */
static const DeviceConfig_t g_config = {
    .blinker_auth   = "your_blinker_auth",   /* Blinker设备密钥 */
    .blinker_ssid   = "your_ssid",            /* WiFi名称 */
    .blinker_pswd   = "your_wifi_password",       /* WiFi密码 */
    .relay_pin      = 0,                /* GPIO0 - 注意：见下方GPIO0冲突说明 */
    .status_led_pin = 2,                /* GPIO2 - 板载LED（多数ESP8266模块） */
    .wifi_reconnect_ms = 5000,          /* 每5秒尝试重连 */
    .wifi_max_retry = 20                /* 最多重试20次 */
};

/*
 * ⚠️ GPIO0 冲突提醒：
 * GPIO0 在 ESP8266 上是启动模式选择引脚：
 *   - 上电时 GPIO0 为低电平 → 进入 UART/Flash 下载模式
 *   - 上电时 GPIO0 为高电平 → 正常启动运行
 * 作为继电器输出使用时：
 *   1. 确保上电时外部电路不会将 GPIO0 拉低
 *   2. 建议在 GPIO0 和 VCC 之间加上拉电阻（10kΩ）
 *   3. 如果硬件允许，建议更换为其他GPIO（如GPIO4、GPIO5）
 */

/* ============ Blinker 组件对象 ============ */
BlinkerButton Button1("btn-abc");   /* App中的按钮组件 */
BlinkerNumber Number1("num-abc");   /* App中的数值组件 */

/* ============ Web 服务器 ============ */
ESP8266WebServer server(80);        /* 创建Web服务器，监听80端口 */
static int counter = 0;             /* 计数器（操作次数） */

/* ============ WiFi 重连状态 ============ */
static uint32_t last_wifi_check_time = 0;   /* 上次检查WiFi的时间 */
static uint8_t  wifi_retry_count   = 0;     /* WiFi重连重试计数 */
static bool     wifi_connected     = false;  /* WiFi连接状态标志 */

/* ============ Web 页面 HTML ============ */
const String webHTML = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP8266 控制器</title>
    <style>
        body { font-family: Arial; text-align: center; margin: 50px; }
        .control-btn { 
            padding: 20px 40px; 
            font-size: 24px; 
            margin: 10px; 
            border: none; 
            border-radius: 10px; 
            cursor: pointer; 
            color: white;
        }
        .on { background-color: #4CAF50; }
        .off { background-color: #f44336; }
        .status { font-size: 20px; margin: 20px; }
    </style>
</head>
<body>
    <h1>ESP8266 双端控制器</h1>
    <div class="status">当前状态：<span id="state">未知</span></div>
    <button class="control-btn on" onclick="controlRelay(1)">打开设备</button>
    <button class="control-btn off" onclick="controlRelay(0)">关闭设备</button>

    <script>
        function controlRelay(state) {
            let url = state === 1 ? "/on" : "/off";
            fetch(url)
                .then(response => response.text())
                .then(data => {
                    document.getElementById("state").innerText = data;
                });
        }

        function refreshStatus() {
            fetch("/status")
                .then(response => response.text())
                .then(data => {
                    document.getElementById("state").innerText = data;
                });
        }

        window.onload = function() {
            refreshStatus();
            setInterval(refreshStatus, 2000);
        };
    </script>
</body>
</html>
)HTML";

/* ============ 函数声明 ============ */
void button1_callback(const String &state);
void dataRead(const String &data);
void handleRoot();
void handleOn();
void handleOff();
void handleStatus();
void handleNotFound();
void updateStatusLED(bool connected);
void checkWiFiConnection();

/* ============ 状态LED控制 ============ */
/**
 * @brief  更新状态LED显示
 * @param  connected true=已连接（LED常亮），false=未连接（LED快闪）
 */
void updateStatusLED(bool connected)
{
    if (connected) {
        digitalWrite(g_config.status_led_pin, LOW);  /* 低电平点亮（多数模块） */
    } else {
        /* 快闪表示未连接 */
        static uint32_t last_blink = 0;
        if (millis() - last_blink > 200) {
            digitalWrite(g_config.status_led_pin,
                         !digitalRead(g_config.status_led_pin));
            last_blink = millis();
        }
    }
}

/* ============ WiFi 断线重连 ============ */
/**
 * @brief  检查WiFi连接状态，断线时自动重连
 * @note   改进点：原代码没有WiFi断线处理，断开后无法恢复
 */
void checkWiFiConnection()
{
    if (WiFi.status() == WL_CONNECTED) {
        /* WiFi正常 */
        if (!wifi_connected) {
            /* 刚刚恢复连接 */
            wifi_connected = true;
            wifi_retry_count = 0;
            BLINKER_LOG("[WiFi] 连接已恢复, IP: ", WiFi.localIP());
        }
        updateStatusLED(true);
        return;
    }

    /* WiFi已断开 */
    wifi_connected = false;
    updateStatusLED(false);

    /* 按间隔尝试重连，避免过于频繁 */
    uint32_t now = millis();
    if (now - last_wifi_check_time < g_config.wifi_reconnect_ms) {
        return;
    }
    last_wifi_check_time = now;

    wifi_retry_count++;
    BLINKER_LOG("[WiFi] 连接断开，尝试重连 (", wifi_retry_count, "/",
                g_config.wifi_max_retry, ")...");

    if (wifi_retry_count <= g_config.wifi_max_retry) {
        /* 尝试重新连接 */
        WiFi.disconnect();
        WiFi.begin(g_config.blinker_ssid, g_config.blinker_pswd);

        /* 等待10秒看是否能连上 */
        uint8_t wait_count = 0;
        while (WiFi.status() != WL_CONNECTED && wait_count < 20) {
            delay(500);
            wait_count++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            BLINKER_LOG("[WiFi] 重连成功, IP: ", WiFi.localIP());
            wifi_connected = true;
            wifi_retry_count = 0;
        }
    } else {
        /* 超过最大重试次数，重启ESP */
        BLINKER_LOG("[WiFi] 重连失败超过上限，重启设备...");
        delay(1000);
        ESP.restart();
    }
}

/* ============ Blinker 回调函数 ============ */
/**
 * @brief  手机App按钮按下回调
 */
void button1_callback(const String &state)
{
    BLINKER_LOG("[Blinker] 收到App按钮状态: ", state);
    /* 翻转继电器状态 */
    digitalWrite(g_config.relay_pin, !digitalRead(g_config.relay_pin));
    /* 同步到App数值组件 */
    counter++;
    Number1.print(counter);
}

/**
 * @brief  手机App发送数据回调
 */
void dataRead(const String &data)
{
    BLINKER_LOG("[Blinker] 收到App数据: ", data);
    counter++;
    Number1.print(counter);
}

/* ============ Web 服务器回调函数 ============ */
void handleRoot()
{
    server.send(200, "text/html", webHTML);
}

void handleOn()
{
    digitalWrite(g_config.relay_pin, LOW);
    BLINKER_LOG("[Web] 设备已打开");
    server.send(200, "text/plain", "已打开");
}

void handleOff()
{
    digitalWrite(g_config.relay_pin, HIGH);
    BLINKER_LOG("[Web] 设备已关闭");
    server.send(200, "text/plain", "已关闭");
}

void handleStatus()
{
    String status = digitalRead(g_config.relay_pin) == LOW ? "已打开" : "已关闭";
    server.send(200, "text/plain", status);
}

void handleNotFound()
{
    server.send(404, "text/plain", "页面未找到！");
}

/* ============ 初始化函数 ============ */
void setup()
{
    /* 初始化串口 */
    Serial.begin(115200);
    delay(100);

#if defined(BLINKER_PRINT)
    BLINKER_DEBUG.stream(BLINKER_PRINT);
#endif

    BLINKER_LOG("\r\n=== ESP8266 控制器启动 ===");
    BLINKER_LOG("固件版本: improved_v1.0");

    /* 初始化继电器引脚 */
    pinMode(g_config.relay_pin, OUTPUT);
    digitalWrite(g_config.relay_pin, HIGH);  /* 默认关闭 */

    /* 初始化状态LED引脚 */
    pinMode(g_config.status_led_pin, OUTPUT);
    digitalWrite(g_config.status_led_pin, HIGH);  /* 初始熄灭 */

    /* 初始化Blinker */
    Blinker.begin(g_config.blinker_auth, g_config.blinker_ssid, g_config.blinker_pswd);
    Blinker.attachData(dataRead);
    Button1.attach(button1_callback);

    /* 配置Web服务器路由 */
    server.on("/", handleRoot);
    server.on("/on", handleOn);
    server.on("/off", handleOff);
    server.on("/status", handleStatus);
    server.onNotFound(handleNotFound);

    /* 启动Web服务器 */
    server.begin();
    BLINKER_LOG("[Web] 服务器已启动, 访问 http://", WiFi.localIP());

    /*
     * 软件看门狗初始化
     * ESP8266 软件看门狗：超时约3.2秒自动复位
     * 需在loop()中定期喂狗，防止系统死锁
     */
    ESP.wdtEnable(WDTO_8S);  /* 8秒超时 */

    wifi_connected = (WiFi.status() == WL_CONNECTED);
    BLINKER_LOG("[System] 初始化完成");
}

/* ============ 主循环 ============ */
void loop()
{
    /* 喂软件看门狗 */
    ESP.wdtFeed();

    /* 检查WiFi连接状态（断线自动重连） */
    checkWiFiConnection();

    /* 处理Blinker消息 */
    Blinker.run();

    /* 处理Web服务器请求 */
    server.handleClient();
}
