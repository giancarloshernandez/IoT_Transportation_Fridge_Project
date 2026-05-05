#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Arduino.h>
#include "Adafruit_SHT31.h"
#include <TinyGPS++.h>
#include <NimBLEDevice.h>
#include <math.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-ab12-ab12-ab12-abcdef012345"

NimBLECharacteristic* pCharacteristic;

unsigned long lastSensorRead = 0;
const long interval = 5000;

int tempAlarm = 0;
int humidAlarm = 0;
int doorAlarm = 0;
int movAlarm = 0;
int systemAlarm = 0;

//Magnet
const int reedPin = 14;
int doorState = 0;
////////////////////////////

//GPS
static const int RXPin = 16, TXPin = 17;
static const uint32_t GPSBaud = 9600;
HardwareSerial ss(2);
TinyGPSPlus gps;

//Temp and Humidity
Adafruit_SHT31 sht31 = Adafruit_SHT31();

//IMU
Adafruit_MPU6050 mpu;

bool mpuFound = false;
bool shtFound = false;

void setup() {
Serial.begin(115200);
delay(2000);

Wire.begin(21, 22);
Wire.setClock(100000);

//Temp and Humidity
if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Warning: SHT31 not found at 0x44");
  } else {
    Serial.println("SHT31 Found!");
  }
//////////////////////////////////////////////////////

//IMU
  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("Warning: MPU6050 not found");
  } 
  else {
    Serial.println("MPU6050 Found!");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }
  ///////////////////////////////////////////////////////////////////////////////////////////
  
  //GPS
  ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);
  Serial.println(F("GT-U7 GPS Data Collection Started"));
  ////////////////////////////////////////////////////////////////////////////////

  //Magnet
  pinMode(reedPin, INPUT_PULLUP);
  ///////////////////////////////
  //BLE
  NimBLEDevice::init("ESP32-Sensor");
  NimBLEDevice::setMTU(512);

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

  unsigned long currentMillis = millis();

  //GPS
  while (ss.available() > 0) {
    gps.encode(ss.read()); 
  }
  ///////////////////////////////////////////////////////////////////////////

  //Temp and Humidity
  if (currentMillis - lastSensorRead >= interval) {
    lastSensorRead = currentMillis;
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();

    if(t > 4.44){ //40*F FDA Danger Zone
      tempAlarm = 1;
      Serial.print("High Temperature: ");
      Serial.print(t);
      Serial.print("*C - ALARM TRIGGERED \t\t");
    }
    else {
      tempAlarm = 0;
      if (! isnan(t)) {  // check if 'is not a number'
      Serial.print("Temp *C = "); Serial.print(t); Serial.print("\t\t");
      } 
      else { 
      Serial.println("Failed to read temperature");
      }
    }

    if(h > 85){ //Too high for meats
      humidAlarm = 1;
      Serial.print("High Humidity: ");
      Serial.print(h);
      Serial.print("% - ALARM TRIGGERED \t\t");
    }
    else{
      humidAlarm = 0;
      if (! isnan(h)) {  // check if 'is not a number'
      Serial.print("Hum. % = "); Serial.println(h);
      } 
      else { 
      Serial.println("Failed to read humidity");
      }
    }
  //////////////////////////////////////////////////////////////////////

  //IMU
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;

    float totalAcc = sqrt(ax * ax + ay * ay + az * az);
    float gForce = totalAcc / 9.81;

    if (gForce > 1.5){
      movAlarm = 1;
      Serial.print("Harsh Movement Detected. Total Acceleration: ");
      Serial.print(gForce);
      Serial.print("g - ALARM TRIGGERED \t\t");
    }

    else{
      movAlarm = 0;
      Serial.print("AccelX:");
      Serial.print(ax);
      Serial.print(",");
      Serial.print("AccelY:");
      Serial.print(ay);
      Serial.print(",");
      Serial.print("AccelZ:");
      Serial.print(az);
      Serial.println("");
    }
    //////////////////////////////////////////////////////////////////////////
    //Magnet
    int reedState = digitalRead(reedPin);
    if (reedState == LOW) {
      Serial.println("Door Closed"); 
      doorState = 0;
      doorAlarm = 0;
    }
    else {
      Serial.println("Door Open - ALARM TRIGGERED \t\t");
      doorState = 1;
      doorAlarm = 1;
    }
    //////////////////////////////
    //BLE
    displayGPSInfo();
    if(tempAlarm == 1 || humidAlarm == 1 || doorAlarm == 1 || movAlarm == 1){
      systemAlarm = 1;
      }

    else{
      systemAlarm = 0;
      }

    String payload = "{";
    payload += "\"t\":" + String(t,1) + ",";
    payload += "\"h\":" + String(h,1) + ",";
    //payload += "\"ax\":" + String(a.acceleration.x,2) + ",";
    //payload += "\"ay\":" + String(a.acceleration.y,2) + ",";
    //payload += "\"az\":" + String(a.acceleration.z,2) + ",";
    //payload += "\"d\":" + String(doorState) + ",";

    if(systemAlarm == 1){
      payload += "\"A\":1,";
      if (gps.location.isValid()) {
        payload += "\"lat\":" + String(gps.location.lat(), 6) + ",";
        payload += "\"lng\":" + String(gps.location.lng(), 6) + ",";
        payload += "\"spd\":" + String(gps.speed.mph(), 1) + ",";
      } 
      else {
        payload += "\"gps\":0,";
      }

      payload += "\"tA\":" + String(tempAlarm) + ",";
      payload += "\"hA\":" + String(humidAlarm) + ",";
      payload += "\"dA\":" + String(doorAlarm) + ",";
      payload += "\"mA\":" + String(movAlarm);
    }

    else{
      payload += "\"A\":" + String(systemAlarm);
    }
      payload += "}";

      pCharacteristic->setValue(payload.c_str());
      pCharacteristic->notify();  // Push to connected clients

      Serial.println("Sent: " + payload);
  }
}


void displayGPSInfo() { // Function for displaying GPS Information
  if (gps.location.isValid()) {
    Serial.print(F("Latitude: "));
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(" Longitude: "));
    Serial.println(gps.location.lng(), 6);
    
    Serial.print(F("Speed (MPH): "));
    Serial.println(gps.speed.mph());
    
    Serial.print(F("Satellites: "));
    Serial.println(gps.satellites.value());
  } else {
    Serial.println(F("Waiting for satellite fix..."));
  }
}