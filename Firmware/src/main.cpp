#include <Arduino.h>

#include "led.h"

LedController leds;

void setup() {
  leds.begin();
  leds.allOn();
}

void loop() {
}
