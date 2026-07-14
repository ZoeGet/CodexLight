#include "led.h"

#include <FastLED.h>

#include "config.h"

namespace {

using namespace CodexLightConfig;

CRGB redLed[LEDS_PER_CHANNEL];
CRGB greenLed[LEDS_PER_CHANNEL];
CRGB yellowLed[LEDS_PER_CHANNEL];

const CRGB RED_COLOR(RED_COLOR_R, RED_COLOR_G, RED_COLOR_B);
const CRGB GREEN_COLOR(GREEN_COLOR_R, GREEN_COLOR_G, GREEN_COLOR_B);
const CRGB YELLOW_COLOR(YELLOW_COLOR_R, YELLOW_COLOR_G, YELLOW_COLOR_B);

void setLed(CRGB* led, const CRGB& color) {
  led[0] = color;
  FastLED.show();
}

void clearLed(CRGB* led) {
  setLed(led, CRGB::Black);
}

}  // namespace

void LedController::begin() {
  FastLED.addLeds<WS2812B, RED_LED_PIN, GRB>(redLed, LEDS_PER_CHANNEL);
  FastLED.addLeds<WS2812B, GREEN_LED_PIN, GRB>(greenLed, LEDS_PER_CHANNEL);
  FastLED.addLeds<WS2812B, YELLOW_LED_PIN, GRB>(yellowLed, LEDS_PER_CHANNEL);

  FastLED.setBrightness(DEFAULT_BRIGHTNESS);
  allOff();
}

void LedController::redOn() {
  setLed(redLed, RED_COLOR);
}

void LedController::redOff() {
  clearLed(redLed);
}

void LedController::greenOn() {
  setLed(greenLed, GREEN_COLOR);
}

void LedController::greenOff() {
  clearLed(greenLed);
}

void LedController::yellowOn() {
  setLed(yellowLed, YELLOW_COLOR);
}

void LedController::yellowOff() {
  clearLed(yellowLed);
}

void LedController::allOn() {
  redLed[0] = RED_COLOR;
  greenLed[0] = GREEN_COLOR;
  yellowLed[0] = YELLOW_COLOR;
  FastLED.show();
}

void LedController::allOff() {
  redLed[0] = CRGB::Black;
  greenLed[0] = CRGB::Black;
  yellowLed[0] = CRGB::Black;
  FastLED.show();
}

void LedController::setBrightness(uint8_t brightness) {
  FastLED.setBrightness(brightness);
  FastLED.show();
}
