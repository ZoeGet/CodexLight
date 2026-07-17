#include "led.h"

#include <Adafruit_NeoPixel.h>

#include "config.h"

namespace {

using namespace CodexLightConfig;

Adafruit_NeoPixel redLed(LEDS_PER_CHANNEL, RED_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel greenLed(LEDS_PER_CHANNEL, GREEN_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel yellowLed(LEDS_PER_CHANNEL, YELLOW_LED_PIN, NEO_GRB + NEO_KHZ800);

uint8_t brightness = DEFAULT_BRIGHTNESS;

void setPixel(Adafruit_NeoPixel& led, uint8_t red, uint8_t green, uint8_t blue) {
  led.setPixelColor(0, led.Color(red, green, blue));
}

void refreshAll() {
  redLed.show();
  greenLed.show();
  yellowLed.show();
}

void setState(
    uint8_t redR,
    uint8_t redG,
    uint8_t redB,
    uint8_t greenR,
    uint8_t greenG,
    uint8_t greenB,
    uint8_t yellowR,
    uint8_t yellowG,
    uint8_t yellowB) {
  setPixel(redLed, redR, redG, redB);
  setPixel(greenLed, greenR, greenG, greenB);
  setPixel(yellowLed, yellowR, yellowG, yellowB);
  refreshAll();
}

}  // namespace

void LedController::begin() {
  redLed.begin();
  greenLed.begin();
  yellowLed.begin();
  setBrightness(DEFAULT_BRIGHTNESS);
  allOff();
}

void LedController::showRed() {
  setState(RED_COLOR_R, RED_COLOR_G, RED_COLOR_B, 0, 0, 0, 0, 0, 0);
}

void LedController::showGreen() {
  setState(0, 0, 0, GREEN_COLOR_R, GREEN_COLOR_G, GREEN_COLOR_B, 0, 0, 0);
}

void LedController::showYellow() {
  setState(0, 0, 0, 0, 0, 0, YELLOW_COLOR_R, YELLOW_COLOR_G, YELLOW_COLOR_B);
}

void LedController::allOff() {
  setState(0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void LedController::setBrightness(uint8_t value) {
  brightness = value;
  redLed.setBrightness(brightness);
  greenLed.setBrightness(brightness);
  yellowLed.setBrightness(brightness);
  refreshAll();
}
