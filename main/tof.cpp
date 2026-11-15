// tof.cpp
#include "tof.h"
#include "config.h"          // XSHUT_*, I2C_*, endereços
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

struct Cal { float c2, c1, c0; };   // real = c2*m*m + c1*m + c0
static const Cal cal[TOF_COUNT] = {
  { -5.962247e-04f, 1.186703f, -34.6185f },   // left  (curva)
  {  0.000000e+00f, 0.946124f, -12.6348f },   // top   (reta)
  {  0.000000e+00f, 0.994827f, -28.5091f },   // right (reta)
};

struct ToFSensor { int xshut; int addr; Adafruit_VL53L0X dev; };
static ToFSensor sensors[TOF_COUNT] = {
  { XSHUT_LEFT,  TOF_LEFT_ADDR  },
  { XSHUT_TOP,   TOF_TOP_ADDR   },
  { XSHUT_RIGHT, TOF_RIGHT_ADDR },
};

static void initTof(TofId id) {
  digitalWrite(sensors[id].xshut, HIGH);
  delay(10);

  if (!sensors[id].dev.begin(sensors[id].addr)) {
    Serial.println("Error initializing ToF");
    while (1);
  }
}

void printDist(int dist){
  if(dist >= 0) Serial.printf("Distance (mm): %d|\n", dist);
  else Serial.print(" out of range ");
}

void initAllTof(){
  int id;
  
  Wire.begin(I2C_SDA, I2C_SCL);
  for (id = 0; id < TOF_COUNT; id++){
    pinMode      (sensors[id].xshut, OUTPUT);
    digitalWrite (sensors[id].xshut, LOW);
  }
  for (id = 0; id < TOF_COUNT; id++)
    initTof((TofId)id);

}

// int readTof(TofId id) {
//   VL53L0X_RangingMeasurementData_t measure;
//   sensors[id].dev.rangingTest(&measure, false); 
//
//   return (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
// }

int readTof(TofId id) {
  VL53L0X_RangingMeasurementData_t measure;
  sensors[id].dev.rangingTest(&measure, false);
  if (measure.RangeStatus == 4) return -1;
  float m = measure.RangeMilliMeter;
  const Cal &c = cal[id];
  return max(0, (int)lroundf(c.c2 * m * m + c.c1 * m + c.c0));
}
