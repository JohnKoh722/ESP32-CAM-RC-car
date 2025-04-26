#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <esp_camera.h>

#define MotorPin1 12
#define MotorPin2 13
#define ServoPin 15
#define LEDPin 2

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

WebSocketsServer webSocket = WebSocketsServer(81);
Servo servo;
WebServer server(80);

int motorSpeed = 400;
#define Angle 90 // default servo pisition
int AngleRequest = 0; // variabilni servo position
#define LEDnum 8

Adafruit_NeoPixel strip(LEDnum, LEDPin, NEO_GRB + NEO_KHZ800);

bool isForward = false; //smer motoru
bool isBackward = false;

bool isWebSocketConnected = false; //conection false ledky

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

void updateMotors() {
  Serial.println(motorSpeed); //smazat
  if (isForward && !isBackward) {
    ledcWriteChannel(1, motorSpeed);
    ledcWriteChannel(2, 0);
  } else if (isBackward && !isForward) {
    ledcWriteChannel(1, 0);
    ledcWriteChannel(2, motorSpeed);
  } else {
    ledcWriteChannel(1, 0);
    ledcWriteChannel(2, 0);
  }
}

void DrivingON(char Direction) {
  switch (Direction) {
    case 'F':
      isForward = true;
      updateMotors();
      break;
    case 'B':
      isBackward = true;
      updateMotors();
      break;
    case 'R':
      servo.write(Angle + AngleRequest + 10);
      break;
    case 'L':
      servo.write(Angle - AngleRequest - 10);
      break;
    default:
      break;
  }
}

void DrivingOFF(char Direction) {
  switch (Direction) {
    case 'F':
      isForward = false;
      break;
    case 'B':
      isBackward = false;
      break;
    case 'R':
      servo.write(Angle);
      break;
    case 'L':
      servo.write(Angle);
      break;
    case 'S':
      isForward = false;
      isBackward = false;
      ledcWriteChannel(1, 0);
      ledcWriteChannel(2, 0);
    default:
      break;
  }
  updateMotors();
}

void Connected() {
  for (int i = 0; i < LEDnum; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0)); //pomalu pres cas zapne ledky
    strip.show();
    delay(200);
  }
  strip.clear();
  strip.show();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_TEXT: {
      String message = String((char*)payload);
      Serial.printf("Received message: %s\n", message.c_str());
      if (message.startsWith("MOVE:") || message.startsWith("STOP:")) { // POHYB START
        char direction = message[5];
        if (message.startsWith("MOVE:")) {
          DrivingON(direction);
        } else {
          DrivingOFF(direction);
        }
      } else if (message.startsWith("NUM")) { // CISLA
        message = message.substring(3);
        if (message.startsWith("A:")) { //  1-UHEL
          message = message.substring(2);
          message.trim();
          AngleRequest = message.toInt();
        } else if (message.startsWith("S:")) {
          message = message.substring(2); // rychlost motoru
          motorSpeed = message.toInt();
          Serial.println(motorSpeed); // smazat
          updateMotors();
        }
      } else if (message.startsWith("COMMAND:")) {
        message = message.substring(8);
        if (message.startsWith("ON")) {
          for (int i = 0; i < LEDnum; i++) {
            strip.setPixelColor(i, strip.Color(random(256), random(256), random(256)));
          }
          strip.show();
        } else {
          strip.clear();
          strip.show();
        }
      }
    break;
    }
    case WStype_DISCONNECTED: {
      isWebSocketConnected = false;
      ledcWriteChannel(1, 0);
      ledcWriteChannel(2, 0);           //LEDC FINISH
    break;
    }
    case WStype_CONNECTED:
      isWebSocketConnected = true;
      isForward = false;
      isBackward = false;
      updateMotors();
      Connected();
    break;
  }
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println("\n\nStarting...");

    setupCamera();

    pinMode(MotorPin1, OUTPUT);
    pinMode(MotorPin2, OUTPUT);
    ledcAttachChannel(MotorPin1, 5000, 8, 1); //2^8
    ledcAttachChannel(MotorPin2, 5000, 8, 2); // pin/freq/resolution/channel
    delay(200);
    ledcWriteChannel(1, 0);
    ledcWriteChannel(2, 0);
    delay(100);

    servo.attach(ServoPin);
    delay(200);
    servo.write(Angle);
    delay(100);

    strip.begin();
    strip.show();
    delay(100);

    WiFi.softAP("RC-Auticko", "123456789"); // wifi
    Serial.println("IP Address: " + WiFi.softAPIP().toString());

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
        webSocket.loop();
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
        delay(1);  // Adjust frame rate here
      }
      Serial.println("Client disconnected");
    });

    webSocket.begin();
    delay(100);
    server.begin();
    webSocket.onEvent(webSocketEvent);
}

void loop() {
  server.handleClient();
  if (!isWebSocketConnected) {
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    delay(100);
    strip.clear();
    strip.show();
    delay(500);
  }
  webSocket.loop();
}