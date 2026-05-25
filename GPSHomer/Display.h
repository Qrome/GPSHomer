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
    void drawBackground();
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

    void drawDistance(
        float homeLatDeg,
        float homeLonDeg,
        float curLatDeg,
        float curLonDeg
    );
    void drawDistanceRings(float clampDist);
    float computeDynamicClamp(float distM);
    void drawAircraft();
    void drawSpeedAndSats(float speedMs, uint8_t sats);
    void drawHomeMarker(
        bool homeSet,
        float homeLatDeg,
        float homeLonDeg,
        float curLatDeg,
        float curLonDeg,
        float headingDeg
    );
};
