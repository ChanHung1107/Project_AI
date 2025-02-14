#include <esp_camera.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// Thông tin WiFi
const char* ssid = "duy";
const char* password = "25092003";

// Cấu hình chân camera ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);

// Trang HTML để hiển thị video
const char STREAM_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Streaming</title>
    <style>
        body { 
            text-align: center; 
            background-color: #333; 
            color: white; 
            font-family: Arial; 
        }
        img { 
            max-width: 100%; 
            margin-top: 20px; 
            border: 3px solid white; 
        }
    </style>
</head>
<body>
    <h1>ESP32-CAM Live Stream</h1>
    <img src="/stream" id="stream">
    <script>
        var view = document.getElementById('stream');
        view.src = "/stream";
    </script>
</body>
</html>
)rawliteral";

// Hàm xử lý stream video
void handleStream() {
  WiFiClient client = server.client();
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("Cache-Control: no-cache");
  client.println("Pragma: no-cache");
  client.println();

  while(true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    client.println("--frame");
    client.println("Content-Type: image/jpeg");
    client.print("Content-Length: ");
    client.println(fb->len);
    client.println();
    client.write(fb->buf, fb->len);
    client.println();

    esp_camera_fb_return(fb);

    // Điều chỉnh delay để kiểm soát tốc độ khung hình
    delay(100);  // Khoảng 10 fps
  }
}

void setup() {
  Serial.begin(115200);

  // Cấu hình camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Cấu hình độ phân giải
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA; // 640x480
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF; // 400x296
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Khởi tạo camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Lỗi khởi tạo camera: 0x%x\n", err);
    return;
  }

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  int wifiTryCount = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTryCount < 30) { // Retry trong 30 lần
    delay(500);
    Serial.print(".");
    wifiTryCount++;
  }
  
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Kết nối WiFi thất bại");
    return;
  }

  Serial.println("\nKết nối WiFi thành công");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());

  // Đảm bảo IP của ESP32-CAM là 192.168.0.153
  if (WiFi.localIP() != IPAddress(192, 168, 0, 153)) {
    Serial.println("IP không đúng, vui lòng đảm bảo ESP32-CAM có IP: 192.168.0.153");
  }

  // Cấu hình Web Server
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", STREAM_HTML);
  });

  // Route cho stream video
  server.on("/stream", HTTP_GET, handleStream);

  // Khởi động server
  server.begin();
  Serial.println("Web server đã được khởi động");
}

void loop() {
  // Xử lý các yêu cầu từ client
  server.handleClient();
}
