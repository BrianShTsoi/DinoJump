#include "DualVNH5019MotorShield.h"

DualVNH5019MotorShield md;

#define START_PIN A5
#define ANALOG_THRESHOLD 250

bool spin = false;

void setup()
{
  pinMode(START_PIN, INPUT);
  
  md.init();

  Serial.begin(9600);
}

void loop()
{
  while (!spin) {
    int reading = analogRead(START_PIN);
    if (reading > ANALOG_THRESHOLD) {
      spin = true;
    }
  }
  
  while (spin) {
    md.setM1Speed(129);
  }
}