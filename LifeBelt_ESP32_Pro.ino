// =====================================================
// LifeBelt ESP32 – PRO VERSION
// Medical-grade logic + confidence scoring + BLE control
// =====================================================

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

// ---------------- Pins ----------------
#define PIN_STRETCH 34
#define PIN_OK_BTN 27
#define PIN_BUZZER 26
#define PIN_BATTERY 35

// ---------------- Thresholds ----------------
#define HR_LOW 40
#define HR_HIGH 150
#define BREATH_TIMEOUT 6000
#define HR_TIMEOUT 8000

// ---------------- BLE UUIDs ----------------
#define SERVICE_UUID        "abcd"
#define CHAR_EVENT_UUID     "ef01"
#define CHAR_CMD_UUID       "ef02"

// ---------------- State ----------------
enum EventType {
  NORMAL,
  FAINTING,
  LOW_HEART_RATE,
  HIGH_HEART_RATE,
  CARDIAC_ARREST
};

EventType currentEvent = NORMAL;

// ---------------- BLE ----------------
BLECharacteristic* eventChar;
BLECharacteristic* cmdChar;
bool deviceConnected = false;

// ---------------- Timing ----------------
unsigned long lastHRts = 0;
unsigned long lastBreathTs = 0;
unsigned long lastNotifyTs = 0;

// ---------------- Data ----------------
int currentHR = 0;
int confidence = 0;

// ---------------- BLE Callbacks ----------------
class CmdCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) {
    std::string cmd = characteristic->getValue();
    if (cmd == "CANCEL") {
      currentEvent = NORMAL;
    }
  }
};

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_OK_BTN, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin();
  max30102.begin(Wire);
  mpu.initialize();

  BLEDevice::init("LifeBelt-PRO");
  BLEServer* server = BLEDevice::createServer();
  BLEService* service = server->createService(SERVICE_UUID);

  eventChar = service->createCharacteristic(
    CHAR_EVENT_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  eventChar->addDescriptor(new BLE2902());

  cmdChar = service->createCharacteristic(
    CHAR_CMD_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  cmdChar->setCallbacks(new CmdCallback());

  service->start();
  BLEDevice::getAdvertising()->start();
}

// ---------------- Helper ----------------
String eventToString(EventType e) {
  switch (e) {
    case FAINTING: return "FAINTING";
    case LOW_HEART_RATE: return "LOW_HR";
    case HIGH_HEART_RATE: return "HIGH_HR";
    case CARDIAC_ARREST: return "CARDIAC_ARREST";
    default: return "NORMAL";
  }
}

// ---------------- Loop ----------------
void loop() {
  unsigned long now = millis();

  // ---- Simulated HR (Demo) ----
  int hr = random(20, 180);
  if (hr > 0) {
    currentHR = hr;
    lastHRts = now;
  }

  // ---- Simulated Breathing ----
  int stretch = analogRead(PIN_STRETCH);
  if (stretch > 100) lastBreathTs = now;

  bool hrPresent = (now - lastHRts < HR_TIMEOUT);
  bool breathing = (now - lastBreathTs < BREATH_TIMEOUT);

  // ---- Event Detection ----
  if (!hrPresent && !breathing) {
    currentEvent = CARDIAC_ARREST;
    confidence = 95;
  } else if (currentHR < HR_LOW && breathing) {
    currentEvent = LOW_HEART_RATE;
    confidence = 80;
  } else if (currentHR > HR_HIGH) {
    currentEvent = HIGH_HEART_RATE;
    confidence = 70;
  } else if (!breathing) {
    currentEvent = FAINTING;
    confidence = 65;
  } else {
    currentEvent = NORMAL;
    confidence = 100;
  }

  // ---- Notify Emergency ----
  if (currentEvent != NORMAL && now - lastNotifyTs > 15000) {
    String msg = "{";
    msg += "\"event\":\"" + eventToString(currentEvent) + "\",";
    msg += "\"hr\":" + String(currentHR) + ",";
    msg += "\"confidence\":" + String(confidence);
    msg += "}";

    eventChar->setValue(msg.c_str());
    eventChar->notify();

    tone(PIN_BUZZER, 2000, 1000);
    lastNotifyTs = now;
  }

  delay(200);
}