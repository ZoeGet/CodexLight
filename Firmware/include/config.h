#ifndef CODEXLIGHT_CONFIG_H
#define CODEXLIGHT_CONFIG_H

#include <Arduino.h>

namespace CodexLightConfig {

constexpr uint8_t RED_LED_PIN = 7;
constexpr uint8_t GREEN_LED_PIN = 6;
constexpr uint8_t YELLOW_LED_PIN = 5;

constexpr uint8_t LEDS_PER_CHANNEL = 1;
constexpr uint8_t DEFAULT_BRIGHTNESS = 64;

constexpr uint8_t RED_COLOR_R = 255;
constexpr uint8_t RED_COLOR_G = 0;
constexpr uint8_t RED_COLOR_B = 0;

constexpr uint8_t GREEN_COLOR_R = 0;
constexpr uint8_t GREEN_COLOR_G = 255;
constexpr uint8_t GREEN_COLOR_B = 0;

constexpr uint8_t YELLOW_COLOR_R = 255;
constexpr uint8_t YELLOW_COLOR_G = 180;
constexpr uint8_t YELLOW_COLOR_B = 0;

}  // namespace CodexLightConfig

#endif  // CODEXLIGHT_CONFIG_H
