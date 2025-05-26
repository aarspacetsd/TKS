#include <MAX6675.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <TinyGPS++.h>

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
SPIClass sdSPI(1); // HSPI = SPIClass(1)

// === RTC (DS3231) ===
#define I2C_SDA 8
#define I2C_SCL 9
TwoWire rtcWire(0);
RTC_DS3231 rtc;

// === GPS di Serial2 (UART2) ===
#define GPS_RX 15
#define GPS_TX 16
#define GPS_BAUD 9600
HardwareSerial gpsSerial(2);  // UART2
TinyGPSPlus gps;

// === Moisture Sensor ===
#define MOISTURE_PIN 17

// === Sensor pH ===
#define PH_PIN 20  // Ganti dengan pin ADC yang sesuai
const float PH_REF_VOLTAGE = 0.140;  // Tegangan saat pH 7
const float PH_REF_VALUE   = 7.0;    // Nilai pH saat tegangan 0.140V
const float PH_SLOPE       = -5.7;   // Asumsi: -5.7 pH/Volt

unsigned long lastLogTime = 0;
const unsigned long logInterval = 3000;

// ==============================
// ==== FUNGSI-FUNGSI MODUL ====
// ==============================

float readTemperature() {
  return thermocouple.getCelsius();  // FIXED
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
    return "Lat: " + String(gps.location.lat(), 6) +
           ", Lng: " + String(gps.location.lng(), 6);
  }
  return "GPS belum fix";
}

float readPH() {
  int raw = analogRead(PH_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  float ph = PH_REF_VALUE + PH_SLOPE * (voltage - PH_REF_VOLTAGE);

  Serial.print("PH ADC: ");
  Serial.print(raw);
  Serial.print(" | Volt: ");
  Serial.print(voltage, 3);
  Serial.print(" V | pH: ");
  Serial.println(ph, 2);

  return ph;
}

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

// =======================
// ==== SETUP & LOOP  ====
// =======================

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // 0-4095 untuk ESP32

  // Inisialisasi SD card
  sdSPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD Card tidak terdeteksi!");
  } else {
    Serial.println("SD Card siap");
  }

  // Inisialisasi RTC
  rtcWire.begin(I2C_SDA, I2C_SCL);
  rtc.begin(&rtcWire);
  if (rtc.lostPower()) {
    Serial.println("RTC kehilangan daya, menyetel waktu...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Inisialisasi GPS Serial2
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS Serial2 dimulai");

  pinMode(MOISTURE_PIN, INPUT);
}

void loop() {
  // Selalu proses data GPS
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // Logging setiap 3 detik
  if (millis() - lastLogTime >= logInterval) {
    lastLogTime = millis();

    float suhu = readTemperature();
    String waktu = readDateTime();
    bool tanahBasah = isSoilWet();
    String gpsData = readGPS();
    float ph = readPH();

    String dataLine = waktu + "," + String(suhu) + "," + (tanahBasah ? "Basah" : "Kering") + "," + gpsData + "," + String(ph, 2);

    Serial.println(dataLine);
    saveToSD(dataLine);
  }
}
