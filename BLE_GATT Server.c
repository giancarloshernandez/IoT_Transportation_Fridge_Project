#include <NimBLEDevice.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-ab12-ab12-ab12-abcdef012345"

NimBLECharacteristic* pCharacteristic;

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("ESP32-Sensor");

  NimBLEServer* pServer = NimBLEDevice::createServer();
  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  pService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("BLE advertising started");
}

void loop() {
  // Replace with real sensor reading
  float temperature = 23.5 + random(-10, 10) / 10.0;
  String payload = "{\"temp\":" + String(temperature, 1) + "}";

  pCharacteristic->setValue(payload.c_str());
  pCharacteristic->notify();  // Push to connected clients

  Serial.println("Sent: " + payload);
  delay(2000);
}
