#include <MAX6675.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <PubSubClient.h>

// === WiFi & MQTT ===
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "MQTT_BROKER_IP";
const char* mqtt_topic = "sensor/log";

WiFiClient espClient;
PubSubClient client(espClient);

// === MAX6675 ===
#define MAX6675_CS   10
#define MAX6675_CLK  12
#define MAX6675_MISO 13
MAX6675 thermocouple(MAX6675_CLK, MAX6675_CS, MAX6675_MISO);

// === SD Card SPI ===
#define SD_CS    39
#define SD_MOSI  35
#define SD_MISO  37
#define SD_CLK   36
SPIClass sdSPI(1);

// === RTC ===
#define I2C_SDA 8
#define I2C_SCL 9
TwoWire rtcWire(0);
RTC_DS3231 rtc;

// === GPS ===
#define GPS_RX 15
#define GPS_TX 16
#define GPS_BAUD 9600
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// === Sensor & Tombol ===
#define MOISTURE_PIN     17
#define PH_PIN           20
#define LOG_TRIGGER_PIN  2
#define SEND_TRIGGER_PIN 3

// === LED WiFi Indicator ===
#define WIFI_LED_PIN     25   // Ubah ke pin yang kamu pakai, atau LED_BUILTIN

// === Kalibrasi pH ===
const float PH_REF_VOLTAGE = 0.140;
const float PH_REF_VALUE = 7.0;
const float PH_SLOPE = -5.7;

unsigned long lastLogTime = 0;
const unsigned long logInterval = 3000;

// === Fungsi Koneksi WiFi & MQTT ===
void connectWiFi() {
  Serial.print("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Terhubung!");
    digitalWrite(WIFI_LED_PIN, HIGH);  // LED menyala saat WiFi terkoneksi
  } else {
    Serial.println(" Gagal terhubung ke WiFi.");
    digitalWrite(WIFI_LED_PIN, LOW);   // LED mati jika tidak terhubung
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println(" Terhubung ke broker");
    } else {
      Serial.print(" Gagal, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus. Mencoba reconnect...");
    WiFi.disconnect();
    connectWiFi();
  }
}

// === Fungsi Pembacaan Sensor ===
float readTemperature() {
  return thermocouple.getCelsius();
}

String readDateTime() {
  DateTime now = rtc.now();
  char buf[25];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
          now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return String(buf);
}

bool isSoilWet() {
  return digitalRead(MOISTURE_PIN) == LOW;
}

String readGPS() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isValid()) {
    return "Lat: " + String(gps.location.lat(), 6) + ", Lng: " + String(gps.location.lng(), 6);
  }
  return "GPS belum fix";
}

float readPH() {
  int raw = analogRead(PH_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  float ph = PH_REF_VALUE + PH_SLOPE * (voltage - PH_REF_VOLTAGE);
  return ph;
}

// === SD Card Logging & MQTT Send ===
void saveToSD(const String& dataLine) {
  File file = SD.open("/log.csv", FILE_APPEND);
  if (file) {
    file.println(dataLine);
    file.close();
    Serial.println("Data disimpan: " + dataLine);
  } else {
    Serial.println("Gagal membuka file log.csv");
  }
}

void sendCSVtoMQTT() {
  File file = SD.open("/log.csv");
  if (!file) {
    Serial.println("Gagal membuka file log.csv untuk dibaca");
    return;
  }

  Serial.println("Mengirim file CSV ke MQTT...");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    client.publish(mqtt_topic, line.c_str());
    delay(100);
  }

  file.close();
  Serial.println("Selesai mengirim CSV");
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(MOISTURE_PIN, INPUT);
  pinMode(LOG_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(SEND_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW); // LED off default

  sdSPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD Card tidak terdeteksi!");
  }

  rtcWire.begin(I2C_SDA, I2C_SCL);
  rtc.begin(&rtcWire);
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

  connectWiFi();
  client.setServer(mqtt_server, 1883);
}

// === LOOP ===
void loop() {
  checkWiFiConnection();
  client.loop();

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (digitalRead(SEND_TRIGGER_PIN) == LOW) {
    reconnectMQTT();
    sendCSVtoMQTT();
    delay(1000); // debounce
  }

  if (digitalRead(LOG_TRIGGER_PIN) == LOW) {
    if (millis() - lastLogTime >= logInterval) {
      lastLogTime = millis();

      float suhu = readTemperature();
      String waktu = readDateTime();
      bool tanahBasah = isSoilWet();
      String gpsData = readGPS();
      float ph = readPH();

      String dataLine = waktu + "," + String(suhu) + "," +
                        (tanahBasah ? "Basah" : "Kering") + "," + gpsData + "," +
                        String(ph, 2);

      Serial.println(dataLine);
      saveToSD(dataLine);
    }
  }
}
