#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Wi-Fi & Firebase Config
#define WIFI_SSID "Xiaomi 11T Pro"
#define WIFI_PASSWORD "8rymrw8maiqv6t4"
#define API_KEY "AIzaSyDbvEZK6x7M9txEGE8MzjXilyyjOMTBsWY"
#define DATABASE_URL "https://s-e-d-e-x-testing-hub-jl38fz-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Sensor Pins
#define ONE_WIRE_BUS 8
#define TURBIDITY_PIN 5
#define TDS_SENSOR_PIN 7
#define PH_PIN 4
#define TRIG_PIN 10
#define ECHO_PIN 3

// Globals
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temperatureC = 0;
float tdsValue = 0;
float phValue = 0;
float distance = 0;
int read_ADC;
int ntu;

// TDS Config
#define VREF 3.3
#define SCOUNT 30
int tdsAnalogBuffer[SCOUNT];
int tdsTempBuffer[SCOUNT];
int tdsIndex = 0;

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
int getMedian(int arr[], int len) {
  int sorted[len];
  memcpy(sorted, arr, len * sizeof(int));
  for (int i = 0; i < len - 1; i++) {
    for (int j = i + 1; j < len; j++) {
      if (sorted[i] > sorted[j]) {
        int temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }
  return (len % 2 == 0) ? (sorted[len / 2] + sorted[len / 2 - 1]) / 2 : sorted[len / 2];
}
void sort(int *arr, int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (arr[i] > arr[j]) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }
}

void setup() {
Serial.begin(115200);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("signUp OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Sensors
  sensors.begin();
  pinMode(TDS_SENSOR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  analogReadResolution(12);
  pinMode(TURBIDITY_PIN, INPUT);
}

void loop() {
  // Temperature
  sensors.requestTemperatures();
  temperatureC = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.println(temperatureC);

  // Turbidity
  read_ADC = analogRead(TURBIDITY_PIN);
  if (read_ADC > 208) read_ADC = 208;
  ntu = map(read_ADC, 0, 208, 300, 0);
  Serial.print("Turbidity: ");
  Serial.print(ntu);
  Serial.println(" NTU ");
  delay(200);
  
  // TDS
  static unsigned long lastSample = millis();
  if (millis() - lastSample > 40) {
    lastSample = millis();
    tdsAnalogBuffer[tdsIndex] = analogRead(TDS_SENSOR_PIN);
    tdsIndex = (tdsIndex + 1) % SCOUNT;
  }
  for (int i = 0; i < SCOUNT; i++) {
    tdsTempBuffer[i] = tdsAnalogBuffer[i];
  }
  float tdsVoltage = getMedian(tdsTempBuffer, SCOUNT) * VREF / 4095.0;
  float compensationVoltage = tdsVoltage / (1.0 + 0.02 * (temperatureC - 25.0));
  float tdsRaw = 133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage;
  tdsValue = tdsRaw * (3.3 / 5.0) * 0.5;
  Serial.print("TDS: ");
  Serial.println(tdsValue, 0);

  // pH
  int phBuffer[10];
  for (int i = 0; i < 10; i++) {
    phBuffer[i] = analogRead(PH_PIN);
    delay(30);
  }
  sort(phBuffer, 10);
  int phADC = 0;
  for (int i = 2; i < 8; i++) phADC += phBuffer[i];
  phADC /= 6;
  float phVoltage = phADC * 3.3 / 4095.0;
  float phCalibration = 21.34 - 0.7;
  phValue = -5.70 * phVoltage + phCalibration;
  Serial.print("pH: ");
  Serial.println(phValue, 2);

  // Ultrasonic
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    Serial.println("Ultrasonic: Out of range");
  } else {
    distance = (duration / 2.0) * 0.0343 / 10;
    Serial.print("Distance: ");
    Serial.println(distance, 2);
  }

  // Firebase Upload
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    Firebase.RTDB.setFloat(&fbdo, "/Sensor/temperature", temperatureC);
    Firebase.RTDB.setFloat(&fbdo, "/Sensor/tds", tdsValue);
    Firebase.RTDB.setFloat(&fbdo, "/Sensor/ph", phValue);
    Firebase.RTDB.setFloat(&fbdo, "/Sensor/distance", distance);
    Firebase.RTDB.setFloat(&fbdo, "/Sensor/turbidity", ntu);
  }

  Serial.println("----------------------");
  delay(700);
}
