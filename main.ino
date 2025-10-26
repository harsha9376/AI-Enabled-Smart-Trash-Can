#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
#include <ESP32Servo.h>
#include <EloquentTinyML.h>
#include "waste_model.h"   // Your exported TinyML model

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ---------------- WIFI ----------------
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// ---------------- ThingSpeak ----------------
const char* apiKey = "YOUR_THINGSPEAK_API_KEY";
const char* thingSpeakServer = "api.thingspeak.com";

// ---------------- Pins ----------------
#define LOADCELL_DOUT_PIN  13
#define LOADCELL_SCK_PIN   12
#define SERVO_PIN 14
#define IR_SENSOR_PIN 15

// ---------------- ML Config ----------------
#define N_INPUTS 64 * 64
#define N_OUTPUTS 2
#define TENSOR_ARENA_SIZE 20 * 1024

Eloquent::TinyML::TensorFlowLite<N_INPUTS, N_OUTPUTS, TENSOR_ARENA_SIZE> ml;

// ---------------- Globals ----------------
HX711 scale;
Servo binServo;
WebServer serverWeb(80);
bool objectDetected = false;
float lastWeight = 0.0;
String lastClassification = "None";
bool lastRecyclable = false;
camera_fb_t *fb = NULL;

// =======================================================
// SETUP
// =======================================================
void setup() {
  Serial.begin(115200);
  pinMode(IR_SENSOR_PIN, INPUT);

  setupWiFi();
  setupCamera();
  setupServer();

  // HX711 setup
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(-96650);
  scale.tare();

  // Servo setup
  binServo.attach(SERVO_PIN);
  binServo.write(90);

  // Load TinyML model
  ml.begin(waste_model_tflite);
  Serial.println("✅ Model loaded successfully!");

  Serial.println("🚀 Smart Bin Ready!");
}

// =======================================================
// LOOP
// =======================================================
void loop() {
  serverWeb.handleClient();

  // IR Sensor detection
  if (digitalRead(IR_SENSOR_PIN) == HIGH && !objectDetected) {
    objectDetected = true;
    Serial.println("📦 Object detected! Waiting for capture...");
  }
  delay(10);
}

// =======================================================
// WIFI SETUP
// =======================================================
void setupWiFi() {
  Serial.println("📡 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("🌐 WiFi connected! IP: ");
  Serial.println(WiFi.localIP());
}

// =======================================================
// CAMERA SETUP
// =======================================================
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
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_64X64;
  config.fb_count = 1;

  esp_camera_init(&config);
  Serial.println("📷 Camera initialized!");
}

// =======================================================
// WEB SERVER SETUP
// =======================================================
void setupServer() {
  serverWeb.on("/", handleRoot);
  serverWeb.on("/capture", handleCapture);
  serverWeb.begin();
  Serial.println("🌍 HTTP Server started!");
}

// =======================================================
// WEB HANDLERS
// =======================================================
void handleRoot() {
  String html = R"rawliteral(
  <html>
  <head>
  <title>Smart Bin Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; background: #f4fdf4; text-align: center; padding: 20px; }
    button { padding: 10px 20px; background: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; }
    button:hover { background: #45a049; }
  </style>
  </head>
  <body>
  <h1>♻️ Smart Waste Bin</h1>
  <p><img src="/capture" width="320"></p>
  <button onclick="location.reload()">Capture</button>
  <h3>Last Classification: <b id="class">-</b></h3>
  <h3>Weight: <b id="weight">-</b> g</h3>
  <script>
    async function fetchData() {
      const res = await fetch('/data');
      const data = await res.json();
      document.getElementById('class').textContent = data.classification;
      document.getElementById('weight').textContent = data.weight;
    }
    setInterval(fetchData, 3000);
    fetchData();
  </script>
  </body>
  </html>
  )rawliteral";
  serverWeb.send(200, "text/html", html);
}

// =======================================================
// CAPTURE + CLASSIFY
// =======================================================
void handleCapture() {
  fb = esp_camera_fb_get();
  if (!fb) {
    serverWeb.send(500, "text/plain", "Camera capture failed!");
    return;
  }

  // Prepare ML input (normalize grayscale values)
  float input[N_INPUTS];
  for (int i = 0; i < N_INPUTS; i++) {
    input[i] = fb->buf[i] / 255.0f;
  }

  float output[N_OUTPUTS];
  ml.predict(input, output);

  int pred = (output[0] > output[1]) ? 0 : 1;  // 0 = recyclable, 1 = non-recyclable
  lastClassification = (pred == 0) ? "Recyclable" : "Non-Recyclable";
  lastRecyclable = (pred == 0);

  lastWeight = readWeight();
  controlServo(lastRecyclable);
  sendToThingSpeak(lastWeight, lastClassification);

  serverWeb.send(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// =======================================================
// SERVO CONTROL
// =======================================================
void controlServo(bool recyclable) {
  Serial.printf("🔄 Flipping for %s\n", recyclable ? "Recyclable" : "Non-Recyclable");
  binServo.write(recyclable ? 0 : 180);
  delay(1500);
  binServo.write(90);
}

// =======================================================
// WEIGHT READING
// =======================================================
float readWeight() {
  if (scale.is_ready()) {
    float w = scale.get_units(5);
    return w > 0 ? w : 0.0;
  }
  return 0.0;
}

// =======================================================
// SEND DATA TO THINGSPEAK
// =======================================================
void sendToThingSpeak(float weight, String classification) {
  WiFiClient client;
  if (client.connect(thingSpeakServer, 80)) {
    String url = "/update?api_key=" + String(apiKey);
    url += "&field1=" + String(weight);
    url += "&field2=" + classification;

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.thingspeak.com\r\n" +
                 "Connection: close\r\n\r\n");
  }
  client.stop();
}
