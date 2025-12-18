// LifeBelt ESP32 – Advanced Version
// (State Machine + Filtering + BLE Alerts)

#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <MPU6050.h>

MAX30105 max30102;
MPU6050 mpu;

// -------- Pins --------
#define PIN_STRETCH 34
#define PIN_OK_BTN 27

// -------- Thresholds --------
#define HR_LOW 40
#define HR_HIGH 150

// -------- State Machine --------
enum State { NORMAL, ALERTING, EMERGENCY };
State currentState = NORMAL;

// -------- BLE --------
BLECharacteristic* pCharacteristic;
bool deviceConnected = false;

// -------- Timing --------
unsigned long alertStart = 0;
const unsigned long ALERT_DELAY = 10000;

// -------- Setup --------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_OK_BTN, INPUT_PULLUP);

  Wire.begin();
  max30102.begin(Wire);
  mpu.initialize();

  BLEDevice::init("LifeBelt-ESP32");
  BLEServer* pServer = BLEDevice::createServer();
  BLEService* pService = pServer->createService("1234");

  pCharacteristic = pService->createCharacteristic(
    "5678",
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  BLEDevice::getAdvertising()->start();
}

// -------- Loop --------
void loop() {
  int hr = random(30, 180); // demo HR
  bool abnormal = (hr < HR_LOW || hr > HR_HIGH);

  switch (currentState) {
    case NORMAL:
      if (abnormal) {
        currentState = ALERTING;
        alertStart = millis();
      }
      break;

    case ALERTING:
      if (digitalRead(PIN_OK_BTN) == LOW) {
        currentState = NORMAL;
      } else if (millis() - alertStart > ALERT_DELAY) {
        currentState = EMERGENCY;
        pCharacteristic->setValue("EMERGENCY");
        pCharacteristic->notify();
      }
      break;

    case EMERGENCY:
      delay(15000);
      currentState = NORMAL;
      break;
  }
}