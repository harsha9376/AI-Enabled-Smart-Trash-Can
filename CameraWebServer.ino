#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "HX711.h"
#include <ESP32Servo.h>

// ===================== CAMERA MODEL =====================
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

// ===================== WIFI =====================
const char* ssid = "IQOO Z9";
const char* password = "1234567890";

// ===================== THINGSPEAK =====================
String apiKey = "8VY9XD42QH9E12DQ";
const char* server = "http://api.thingspeak.com/update";

// ===================== SENSOR PINS =====================
#define IR_PIN 13
#define SERVO_PIN 15
#define HX711_DT 14
#define HX711_SCK 12

// ===================== GLOBAL OBJECTS =====================
HX711 scale;
Servo myServo;

// Counters
int recyclableCount = 0;
int nonRecyclableCount = 0;

// Function prototype for web server (defined in app_httpd.cpp)
void startCameraServer();

// ===================== SEND DATA TO THINGSPEAK =====================
void sendToThingSpeak(long weight, bool recyclable) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String postData = "api_key=" + apiKey +
                      "&field1=" + String(weight) +
                      "&field2=" + String(recyclable ? 1 : 0) +
                      "&field3=" + String(recyclableCount) +
                      "&field4=" + String(nonRecyclableCount);

    http.begin(server);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(postData);
    if (code > 0) Serial.println("✅ Data sent to ThingSpeak");
    else Serial.printf("❌ HTTP error %d\n", code);
    http.end();
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");

  // IR Sensor
  pinMode(IR_PIN, INPUT);

  // Servo
  myServo.attach(SERVO_PIN);
  myServo.write(90);

  // HX711
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale(-7050);
  scale.tare();

  // ==== WiFi Connection ====
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.println("Connecting to WiFi...");
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);

unsigned long startAttemptTime = millis();
const unsigned long wifiTimeout = 20000;  // 20 sec

while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
  Serial.print(".");
  delay(500);
}

if (WiFi.status() == WL_CONNECTED) {
  Serial.println("\n✅ WiFi connected!");
  Serial.print("📡 ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
} else {
  Serial.println("\n❌ WiFi connection failed. Restarting...");
  delay(5000);
  ESP.restart();
}

  // ==== Camera Config ====
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed 0x%x\n", err);
    return;
  }

  // Start web stream server
  startCameraServer();
  Serial.println("📸 Camera ready! Open the following in browser:");
  Serial.print("http://");
  Serial.println(WiFi.localIP());
}

// ===================== LOOP =====================
void loop() {
  // IR object detection
  if (digitalRead(IR_PIN) == LOW) {
    Serial.println("📦 Object detected!");
    long weight = scale.get_units(5);
    Serial.printf("Measured weight: %ld g\n", weight);

    // Dummy classification (replace later with actual logic)
    bool recyclable = (weight % 2 == 0);
    if (recyclable) {
      recyclableCount++;
      myServo.write(45);  // left
    } else {
      nonRecyclableCount++;
      myServo.write(45); // right
    }
    delay(1000);
    myServo.write(90); // center again

    sendToThingSpeak(weight, recyclable);
    delay(2000); // avoid multiple triggers
  }
  delay(100);
}
