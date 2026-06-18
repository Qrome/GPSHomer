#pragma once

// ===== Active Definitions Extracted from User_Setup =====
#define USER_SETUP_LOADED

// --- Driver & Display ---
#define USER_SETUP_INFO "User_Setup"
#define GC9A01_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240
#define TFT_RGB_ORDER TFT_BGR
#define SMOOTH_FONT
#define RP2040_PIO_SPI

// --- Pins (RP2040 Zero – Qrome) ---
#define TFT_MOSI 3
#define TFT_SCLK 2
#define TFT_CS   4
#define TFT_DC   5
#define TFT_RST  6
#define TFT_BL   -1

// --- Fonts ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

// --- SPI Frequency ---
#define SPI_FREQUENCY 60000000

#include <TFT_eSPI.h>
#include <Arduino.h>
#include "config.h"

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
