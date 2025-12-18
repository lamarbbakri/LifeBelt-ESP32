
/*
 LifeBelt – ESP32 Firmware (Arduino / C++)
*/

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

#define PIN_STRETCH 34
#define PIN_OK_BTN  27

static const char* SERVICE_UUID        = "c0de0001-1111-2222-3333-444455556666";
static const char* CHARACTERISTIC_UUID = "c0de0002-1111-2222-3333-444455556666";

BLECharacteristic* pCharacteristic;
bool deviceConnected = false;

int HR_LOW_BPM  = 40;
int HR_HIGH_BPM = 150;

int stretchBaseline = 0;
int stretchNoiseBand = 40;

unsigned long lastBreathingTs = 0;
unsigned long lastMotionTs    = 0;
unsigned long lastHRts        = 0;

int currentHR = 0;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) { deviceConnected = true; }
  void onDisconnect(BLEServer*) { deviceConnected = false; }
};

void setup() {
  Serial.begin(115200);
  pinMode(PIN_OK_BTN, INPUT_PULLUP);
  Wire.begin(21, 22);

  BLEDevice::init("LifeBelt-ESP32");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService* service = server->createService(SERVICE_UUID);

  pCharacteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  service->start();
  BLEDevice::getAdvertising()->start();

  max30102.begin(Wire);
  mpu.initialize();

  long sum = 0;
  for(int i=0;i<100;i++){ sum += analogRead(PIN_STRETCH); delay(10); }
  stretchBaseline = sum / 100;
}

void loop() {
  int stretch = analogRead(PIN_STRETCH);
  if (abs(stretch - stretchBaseline) > stretchNoiseBand) lastBreathingTs = millis();

  int hr = checkForBeat(max30102.getIR()) ? 60 : 0;
  if (hr > 0) { currentHR = hr; lastHRts = millis(); }

  unsigned long now = millis();
  bool breathing = (now - lastBreathingTs < 8000);
  bool hrPresent = (now - lastHRts < 8000);

  String event = "NORMAL";
  if (!hrPresent && !breathing) event = "CARDIAC_ARREST_SUSPECTED";
  else if (currentHR < HR_LOW_BPM && breathing) event = "SYNCOPE_SUSPECTED";
  else if (currentHR > HR_HIGH_BPM) event = "HEART_ATTACK_SUSPECTED";

  if (event != "NORMAL" && deviceConnected) {
    String msg = "{\"event\":\"" + event + "\"}";
    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();
    delay(15000);
  }

  delay(100);
}
