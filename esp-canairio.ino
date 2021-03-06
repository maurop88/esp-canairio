#include <hpma115s0.h>
#include <DHTesp.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <numeric>
#include <vector>
#include "CanAirIoApi.hpp"
#include "MyConfigs.cpp"

/******************************************************************************
*   CONFIGS
******************************************************************************/

boolean DEBUG = true;

const int HPMA_PIN_RX = 12;
const int HPMA_PIN_TX = 13;
const int DHT_PIN = 14;

const int hpmaCheckInterval = 5000;
const int sendCheckInterval = 60000;

unsigned long currentMillis = 0;
unsigned long lastHpmaMillis = 0;
unsigned long lastSendMillis = 0;

std::vector<unsigned int> vectorInstant25;
std::vector<unsigned int> vectorInstant10;
unsigned int averagePm25 = 0;
unsigned int averagePm10 = 0;
unsigned int averagePm1 = 0;
unsigned int instantHumidity = 0;
unsigned int instantTemperature = 0;

CanAirIoApi api(DEBUG);
SoftwareSerial hpmaSerial(HPMA_PIN_RX, HPMA_PIN_TX);
HPMA115S0 hpmaSensor(hpmaSerial);
DHTesp dhtSensor;

/******************************************************************************
*   SETUP
******************************************************************************/

void setup() {
  if (DEBUG) Serial.println("Starting Setup");
  
  delay(1000);

  Serial.begin(115200); Serial.println();
  hpmaSerial.begin(9600); hpmaSerial.println();
  
  wifiInit();
  apiInit();
  sensorsInit();
}

void wifiInit() {
  if (DEBUG) Serial.println("Connecting to " + String(cfgWifiSsid));
  
  WiFi.begin(cfgWifiSsid, cfgWifiPass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (DEBUG) Serial.println(".");
  }

  if (DEBUG) Serial.println("OK! IP: " + String(WiFi.localIP().toString()));
}

void apiInit() {
  if (DEBUG) Serial.println("Connecting to CanAirIoAPI");
  
  api.configure(cfgDeviceName, cfgDeviceId); 
  api.authorize(cfgApiUser, cfgApiPass);
  delay(1000);
  
  if (DEBUG) Serial.println("OK!");
}

void sensorsInit() {
  if (DEBUG) Serial.println("Initializing HPMA and DHT");

  hpmaSensor.stop_autosend();
  hpmaSensor.start_measurement();

  dhtSensor.setup(DHT_PIN, DHTesp::DHT11);
  delay(2000);

  if (DEBUG) Serial.println("OK!");
}

/******************************************************************************
*   MAIN LOOP
******************************************************************************/

void loop() {
  currentMillis = millis();

  if ((unsigned long) currentMillis - lastHpmaMillis >= hpmaCheckInterval) {
    readPM();
    lastHpmaMillis = currentMillis;
  }

  if ((unsigned long) currentMillis - lastSendMillis >= sendCheckInterval) {
    if (calculatePmAverage()) {
      readHT();
      checkWifi();
      sendToApi();
    }
    lastSendMillis = currentMillis;
  }
}

void readPM() {
  float p25;
  float p10;

  if (hpmaSensor.read(&p25,&p10) == 1) {
    vectorInstant25.push_back((int)p25);
    vectorInstant10.push_back((int)p10);
    if (DEBUG) Serial.println("PM25=" + String(p25) + " PM10=" + String(p10));
  } else {
    Serial.println("Measurement fail");
  } 
}

boolean calculatePmAverage() {
  if (vectorInstant25.size() > 0) {
    averagePm25 = accumulate(vectorInstant25.begin(), vectorInstant25.end(), 0.0)/vectorInstant25.size();
    vectorInstant25.clear();
    averagePm10 = accumulate(vectorInstant10.begin(), vectorInstant10.end(), 0.0)/vectorInstant10.size();
    vectorInstant10.clear();
    if (DEBUG) Serial.println("Averages: PM25=" + String(averagePm25) + ", PM10=" + String(averagePm10));
    return true;
  } else {
    if (DEBUG) Serial.println("One of the vectors has 0 read values. Skipping cycle");
    return false;
  }
}

void readHT() {
  instantTemperature = dhtSensor.getTemperature();
  instantHumidity = dhtSensor.getHumidity();
  if (DEBUG) Serial.println("Temperature=" + String(instantTemperature) + " Humidity=" + String(instantHumidity));
}

void checkWifi() {
  if (!WiFi.isConnected()) {
    if (DEBUG) Serial.println("WiFi disconnected!");
    wifiInit();
    apiInit();
  }
}

void sendToApi() {
  if (DEBUG) Serial.println("API writing to " + String(api.url));

  bool status = api.write(averagePm1,averagePm25,averagePm10,instantHumidity,instantTemperature,cfgLatitude,cfgLongitude,cfgAltitude,cfgSpeed,sendCheckInterval / 1000);
  int code = api.getResponse();

  if (status) {
    if (DEBUG) Serial.println("OK! " + String(code));
  } else {
    if (DEBUG) Serial.println("FAIL! " + String(code));
  }
}
