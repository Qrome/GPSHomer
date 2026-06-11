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
    void showSummary(float maxDist, float maxSpeed, float totalDist, float avgSpeed, uint32_t flightStartMs);
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
    void drawDistanceRings(float clampDist, float headingDeg);
    void drawNorth(float headingDeg);
    float computeDynamicClamp(float distM);
    void drawAircraft();
    void drawSpeedAndSats(float speedMs, uint8_t sats);
    void drawHomeMarker(
        bool homeSet,
        float homeLatDeg,
        float homeLonDeg,
        float curLatDeg,
        float curLonDeg,
        float headingDeg,
        float speedMs
    );
    void drawIconRadar(int x, int y, uint16_t color);
    void drawIconSpeedometer(int x, int y, uint16_t color);
    void drawIconPath(int x, int y, uint16_t color);
    void drawIconGauge(int x, int y, uint16_t color);
    void drawIconClock(int x, int y, uint16_t color);
    void drawTrail(float homeLat, float homeLon, float headingDeg, float clampDist);
};
