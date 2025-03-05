#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <MQUnifiedsensor.h>

// Konfigurasi WiFi
char ssid[] = "SKRIPSII";
char pass[] = "12345678";

// Konfigurasi Blynk
#define BLYNK_TEMPLATE_ID "TMPL6EDKy-lll"
#define BLYNK_TEMPLATE_NAME "Kandang Ayam IoT"
#define BLYNK_AUTH_TOKEN "6o2f5BNd2SHn5PaBgBHpHdIvTjMdZbkx"

// URL Google Sheets
const char* googleScriptUrl = "https://script.google.com/macros/s/AKfycby5uy__TpcLNjJuSIsyRpSZd6yrmDszXcaa6n9oafMbsiAVASk8YDP7MlXBVm9Ms8nj/exec";

// Konfigurasi Sensor
#define DHT_PIN 23
#define DHT_TYPE DHT22
#define TRIG_PIN 18
#define ECHO_PIN 19
#define IR_PIN 25  // Pin sensor IR

// Konfigurasi MQ-135
#define placa "ESP32"
#define Voltage_Resolution 3.3
#define pin 34
#define type "MQ-135"
#define ADC_Bit_Resolution 12
#define RatioMQ135CleanAir 3.6

// Konstanta untuk NH4
#define MQ135_A 102.2
#define MQ135_B -2.473

// Relay dan Servo
#define RELAY1_PIN 5
#define RELAY2_PIN 26
#define RELAY3_PIN 27
#define RELAY4_PIN 15
#define SERVO360PIN 14

// Virtual Pin Blynk
#define VPIN_TEMPERATURE V1
#define VPIN_HUMIDITY V2
#define VPIN_MQ135 V3
#define VPIN_DISTANCE V4
#define VPIN_IR_SENSOR V0
#define VPIN_RELAY1 V5
#define VPIN_RELAY2 V6
#define VPIN_RELAY3 V7
#define VPIN_RELAY4 V8
#define VPIN_IR_STATUS V10
#define VPIN_SERVO_CONTROL V9

DHT dht(DHT_PIN, DHT_TYPE);
Servo servo360;
bool servoState = false;

MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, pin, type);

void setup() {
  Serial.begin(115200);
  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);

  servo360.attach(SERVO360PIN);
  servo360.write(90);
  delay(500);
  servo360.detach();

  MQ135.setRegressionMethod(1);
  MQ135.setA(MQ135_A);
  MQ135.setB(MQ135_B);
  MQ135.init();

  Serial.print("Calibrating MQ-135, please wait.");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println(" done!");

  if (isinf(calcR0) || calcR0 == 0) {
    Serial.println("Error: Check wiring or supply for MQ-135.");
    while (1);
  }

  MQ135.serialDebug(false);
}

void sendDataToGoogleSheets(int temperature, int humidity, int mq135, int distance, const String& irStatus, int relay2, int relay3, int relay4, int servoState) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(googleScriptUrl);

    String jsonData = String("{") +
                      "\"temperature\":" + temperature + "," +
                      "\"humidity\":" + humidity + "," +
                      "\"mq135\":" + mq135 + "," +
                      "\"distance\":" + distance + "," +
                      "\"irSensor\":\"" + irStatus + "\"," +
                      "\"relay2\":" + relay2 + "," +
                      "\"relay3\":" + relay3 + "," +
                      "\"relay4\":" + relay4 + "," +
                      "\"servo\":" + servoState +
                      "}";

    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonData);
    Serial.printf("Google Sheets Response: %d\n", httpResponseCode);
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void loop() {
  Blynk.run();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  MQ135.update();
  float mq135Raw = MQ135.readSensor();
  int mq135Value = round(mq135Raw);

  long duration;
  int distance;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = round(duration * 0.034 / 2);

  int irValue = digitalRead(IR_PIN);  // Nilai sensor IR (1 atau 0)
  String irStatus = (irValue == 0) ? "Ada Makanan" : "Tidak Ada Makanan";

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  int roundedTemperature = round(temperature);
  int roundedHumidity = round(humidity);

  // Menampilkan data di Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(roundedTemperature);
  Serial.println(" °C");
  Serial.print("Kelembapan: ");
  Serial.print(roundedHumidity);
  Serial.println(" %");
  Serial.print("Gas (NH4): ");
  Serial.print(mq135Value);
  Serial.println(" PPM");
  Serial.print("Jarak: ");
  Serial.print(distance);
  Serial.println(" cm");
  Serial.print("IR Sensor: ");
  Serial.println(irStatus);

  // Mengirim data ke Blynk
  Blynk.virtualWrite(VPIN_TEMPERATURE, roundedTemperature);
  Blynk.virtualWrite(VPIN_HUMIDITY, roundedHumidity);
  Blynk.virtualWrite(VPIN_MQ135, mq135Value);
  Blynk.virtualWrite(VPIN_DISTANCE, distance);
  Blynk.virtualWrite(VPIN_IR_SENSOR, irValue);  
  Blynk.virtualWrite(VPIN_IR_STATUS, irStatus);  

  // Mengirim data ke Google Sheets
  sendDataToGoogleSheets(roundedTemperature, roundedHumidity, mq135Value, distance, irStatus,
                         digitalRead(RELAY2_PIN) == LOW ? 1 : 0,
                         digitalRead(RELAY3_PIN) == LOW ? 1 : 0,
                         digitalRead(RELAY4_PIN) == LOW ? 1 : 0,
                         servoState ? 1 : 0);

  delay(1000);  // Delay untuk pembacaan sensor
}

// BLYNK_WRITE untuk kontrol relay
BLYNK_WRITE(VPIN_RELAY1) {
  int relayState = param.asInt();
  digitalWrite(RELAY1_PIN, !relayState);
}

BLYNK_WRITE(VPIN_RELAY2) {
  int relayState = param.asInt();
  digitalWrite(RELAY2_PIN, !relayState);
}

BLYNK_WRITE(VPIN_RELAY3) {
  int relayState = param.asInt();
  digitalWrite(RELAY3_PIN, !relayState);
}

BLYNK_WRITE(VPIN_RELAY4) {
  int relayState = param.asInt();
  digitalWrite(RELAY4_PIN, !relayState);
}

// BLYNK_WRITE untuk kontrol servo menggunakan button
BLYNK_WRITE(VPIN_SERVO_CONTROL) {
  int switchState = param.asInt();  
  if (switchState == 1) {
    servo360.attach(SERVO360PIN); 
    servo360.write(90);           
    Serial.println("Servo bergerak");
    delay(4000);                  

    servo360.write(180);          
    delay(4000);                  
    servo360.detach();            
    Serial.println("Servo berhenti");
    servoState = true; 
  } else {
    servoState = false;
  }
}