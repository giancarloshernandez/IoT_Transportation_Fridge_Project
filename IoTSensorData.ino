#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Arduino.h>
#include "Adafruit_SHT31.h"
#include <TinyGPS++.h>

unsigned long lastSensorRead = 0;
const long interval = 1000;

//Magnet
const int reedPin = 14;
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
}


void loop() {

  //Temp and Humidity
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= interval) {
    lastSensorRead = currentMillis;
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();

    if (! isnan(t)) {  // check if 'is not a number'
      Serial.print("Temp *C = "); Serial.print(t); Serial.print("\t\t");
    } else { 
      Serial.println("Failed to read temperature");
    }
    
    if (! isnan(h)) {  // check if 'is not a number'
      Serial.print("Hum. % = "); Serial.println(h);
    } else { 
      Serial.println("Failed to read humidity");
    }
  //////////////////////////////////////////////////////////////////////

  //IMU
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    Serial.print("AccelX:");
    Serial.print(a.acceleration.x);
    Serial.print(",");
    Serial.print("AccelY:");
    Serial.print(a.acceleration.y);
    Serial.print(",");
    Serial.print("AccelZ:");
    Serial.print(a.acceleration.z);
    Serial.print(", ");
    Serial.print("GyroX:");
    Serial.print(g.gyro.x);
    Serial.print(",");
    Serial.print("GyroY:");
    Serial.print(g.gyro.y);
    Serial.print(",");
    Serial.print("GyroZ:");
    Serial.print(g.gyro.z);
    Serial.println("");
    //////////////////////////////////////////////////////////////////////////
    //Magnet
    int reedState = digitalRead(reedPin);
    if (reedState == LOW) {
      Serial.println("Door Closed"); 
    }
    else {
      Serial.println("Door Open");
    }
    //////////////////////////////
  }
  //GPS
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      displayGPSInfo();
    }
  }
  ///////////////////////////////////////////////////////////////////////////
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
