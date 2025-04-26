#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>

const char* ssid = "ESP32-CAM-AP"; // JMENO WIFI    -    JMENO WIFI    -    JMENO WIFI    -    JMENO WIFI
const char* password = "123456789"; // HESLO    -HESLO    -HESLO    -HESLO    -HESLO    -HESLO    -

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

void setupCamera() {
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
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x", err);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n\nStarting...");

  setupCamera();

  // Start Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Server Handlers
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Content-Type", "text/html");
    server.send(200, "text/html", 
      "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
      "<style>body{margin:0; background:#000;} img{width:100%; height:auto;}</style></head>"
      "<body><img src='/stream'></body></html>");
  });

  server.on("/stream", HTTP_GET, []() {
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println();

    while (client.connected()) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Frame capture failed");
        break;
      }

      client.print(
        "--frame\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: " + String(fb->len) + "\r\n\r\n");
      client.write(fb->buf, fb->len);
      client.print("\r\n");
      
      esp_camera_fb_return(fb);
      delay(1);  // Adjust frame rate
    }
    Serial.println("Client disconnected");
  });

  server.begin();
}

void loop() {
  server.handleClient();
  delay(1);
}