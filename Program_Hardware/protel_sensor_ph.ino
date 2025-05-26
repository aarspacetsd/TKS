#define PH_PIN 20  // Ganti dengan pin ADC yang sesuai

const float PH_REF_VOLTAGE = 0.140;  // Tegangan saat pH 7
const float PH_REF_VALUE   = 7.0;    // Nilai pH saat tegangan 0.140V
const float PH_SLOPE       = -5.7;   // Asumsi: -5.7 pH/Volt

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // 0-4095 untuk ESP32
}

void loop() {
  int raw = analogRead(PH_PIN);
  float voltage = (raw / 4095.0) * 3.3;

  float ph = PH_REF_VALUE + PH_SLOPE * (voltage - PH_REF_VOLTAGE);

  Serial.print("ADC: ");
  Serial.print(raw);
  Serial.print(" | Volt: ");
  Serial.print(voltage, 3);
  Serial.print(" V | pH (estimasi): ");
  Serial.println(ph, 2);

  delay(1000); // 1 detik
}
