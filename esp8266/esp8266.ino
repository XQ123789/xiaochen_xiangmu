#define BLINKER_PRINT Serial
#define BLINKER_WIFI

#include <Blinker.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===================== Blinker 配置 =====================
char auth[] = "your_blinker_auth";  // 设备密钥
char ssid[] = "your_ssid";           // WiFi 名称
char pswd[] = "your_wifi_password";      // WiFi 密码

// Blinker 组件对象
BlinkerButton Button1("btn-abc");  // App 中的按钮组件
BlinkerNumber Number1("num-abc"); 

// ===================== Web 服务器配置 =====================
ESP8266WebServer server(80);  // 创建 Web 服务器，监听 80 端口
const int relayPin = 0;       // 继电器/LED 控制引脚
int counter = 0;              // 计数变量

// Web 页面 HTML 代码
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
        .on { background-color: #4CAF50; }  /* 绿色：打开 */
        .off { background-color: #f44336; } /* 红色：关闭 */
        .status { font-size: 20px; margin: 20px; }
    </style>
</head>
<body>
    <h1>ESP8266 双端控制器</h1>
    <div class="status">当前状态：<span id="state">未知</span></div>
    <button class="control-btn on" onclick="controlRelay(1)">打开设备</button>
    <button class="control-btn off" onclick="controlRelay(0)">关闭设备</button>

    <!-- JavaScript：实现按钮控制和状态刷新 -->
    <script>
        // 控制继电器（向 ESP8266 发送 HTTP 请求）
        function controlRelay(state) {
            let url = state === 1 ? "/on" : "/off";
            fetch(url)
                .then(response => response.text())
                .then(data => {
                    document.getElementById("state").innerText = data;
                });
        }

        // 定时刷新当前状态（每 2 秒查询一次）
        function refreshStatus() {
            fetch("/status")
                .then(response => response.text())
                .then(data => {
                    document.getElementById("state").innerText = data;
                });
        }

        // 页面加载时立即刷新状态，之后每 2 秒刷新一次
        window.onload = function() {
            refreshStatus();
            setInterval(refreshStatus, 2000);
        };
    </script>
</body>
</html>
)HTML";

// ===================== 函数声明 =====================
void button1_callback(const String &state);  // Blinker 按钮回调函数
void dataRead(const String &data);           // Blinker 数据接收回调函数
void handleRoot();                           // Web 根目录请求处理
void handleOn();                             // Web 打开设备请求处理
void handleOff();                            // Web 关闭设备请求处理
void handleStatus();                         // Web 状态查询请求处理
void handleNotFound();                       // Web 404 错误处理

// ===================== Blinker 回调函数 =====================
// 手机 App 中按钮按下时触发
void button1_callback(const String &state) {
    BLINKER_LOG("收到 App 按钮状态：", state);
    // 翻转继电器状态
    digitalWrite(relayPin, !digitalRead(relayPin));
    // 同步状态到 App 数值组件
    counter++;
    Number1.print(counter);
}

// 手机 App 发送数据时触发
void dataRead(const String &data) {
    BLINKER_LOG("收到 App 数据：", data);
    counter++;
    Number1.print(counter);
}

// ===================== Web 服务器回调函数 =====================
void handleRoot() {
    server.send(200, "text/html", webHTML);
}

// 浏览器访问 /on 路径时，打开设备
void handleOn() {
    digitalWrite(relayPin, LOW);
    server.send(200, "text/plain", "已打开");
}

// 浏览器访问 /off 路径时，关闭设备
void handleOff() {
    digitalWrite(relayPin, HIGH);  // GPIO0 高电平：关闭设备
    server.send(200, "text/plain", "已关闭");
}

// 浏览器访问 /status 路径时，返回当前设备状态
void handleStatus() {
    String status = digitalRead(relayPin) == LOW ? "已打开" : "已关闭";
    server.send(200, "text/plain", status);
}

// 浏览器访问不存在的路径时，返回 404 错误
void handleNotFound() {
    server.send(404, "text/plain", "页面未找到！");
}

// ===================== 初始化函数 =====================
void setup() {
    // 初始化串口
    Serial.begin(115200);
    #if defined(BLINKER_PRINT)
        BLINKER_DEBUG.stream(BLINKER_PRINT);
    #endif

    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, HIGH);

    Blinker.begin(auth, ssid, pswd);
    Blinker.attachData(dataRead);     
    Button1.attach(button1_callback);

    server.on("/", handleRoot);         // 根目录
    server.on("/on", handleOn);         // 打开设备
    server.on("/off", handleOff);       // 关闭设备
    server.on("/status", handleStatus); // 查询状态
    server.onNotFound(handleNotFound);  // 404 错误

    // 启动 Web 服务器
    server.begin();
    BLINKER_LOG("Web 服务器已启动，访问 http://", WiFi.localIP());  // 打印 ESP8266 的 IP 地址
}

// ===================== 主循环 =====================
void loop() {
    Blinker.run();
    server.handleClient();
}
