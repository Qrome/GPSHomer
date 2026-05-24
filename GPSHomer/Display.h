#pragma once

#include <Arduino.h>
#include "config.h"
#include <TFT_eSPI.h>

class Display {
public:
    Display();

    void begin();

    // Show boot splash with detected baud
    void showBootBaud(uint32_t baud);

    // Render radar frame
    void render(
        bool homeSet,
        float homeLatDeg,
        float homeLonDeg,
        float curLatDeg,
        float curLonDeg,
        float headingDeg,
        float speedMs,
        uint8_t sats
    );

private:
    TFT_eSPI _tft;

    void drawBackground();
    void drawDistanceRings(float clampDist);
    float computeDynamicClamp(float distM);
    void drawAircraft();
    void drawSpeedAndSats(float speedMs, uint8_t sats);
    float drawHomeMarker(
        bool homeSet,
        float homeLatDeg,
        float homeLonDeg,
        float curLatDeg,
        float curLonDeg,
        float headingDeg
    );
};
