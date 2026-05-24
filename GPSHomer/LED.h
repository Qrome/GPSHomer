#pragma once
#include <Adafruit_NeoPixel.h>

#define LED_PIN 16
#define LED_COUNT 1

class StatusLED {
public:
    StatusLED() : strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800) {}

    void begin() {
        strip.begin();
        strip.show(); // Initialize to off
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        strip.setPixelColor(0, strip.Color(r, g, b));
        strip.show();
    }

    void off() {
        setColor(0, 0, 0);
    }

    // Convenience colors
    void blue()  { setColor(0, 0, 20); }
    void green() { setColor(0, 20, 0); }
    void red()   { setColor(20, 0, 0); }
    void amber() { setColor(20, 10, 0); }

private:
    Adafruit_NeoPixel strip;
};
