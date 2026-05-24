#include "Display.h"
#include <math.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295f
#endif

Display::Display()
    : _tft(TFT_eSPI())
{
}

void Display::begin() {
    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(COLOR_BACKGROUND);
}

void Display::showBootBaud(uint32_t baud) {
    _tft.fillScreen(COLOR_BACKGROUND);
    _tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _tft.setTextSize(2);
    _tft.setCursor(20, 100);
    _tft.print("GPS Baud:");
    _tft.setCursor(20, 130);
    _tft.print(baud);
}

void Display::drawBackground() {
    _tft.fillScreen(COLOR_BACKGROUND);
}

void Display::drawAircraft() {
    _tft.fillTriangle(
        SCREEN_CENTER_X,     SCREEN_CENTER_Y - 10,
        SCREEN_CENTER_X - 6, SCREEN_CENTER_Y + 6,
        SCREEN_CENTER_X + 6, SCREEN_CENTER_Y + 6,
        COLOR_AIRCRAFT
    );
}

void Display::drawSpeedAndSats(float speedMs, uint8_t sats) {
    _tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _tft.setTextSize(2);

    _tft.setCursor(10, SCREEN_HEIGHT - 24);

#if OSD_UNITS == UNITS_METRIC
    _tft.print(speedMs, 1);
    _tft.print(" m/s");
#else
    _tft.print(speedMs * 2.23694f, 0);
    _tft.print(" mph");
#endif

    _tft.setCursor(SCREEN_WIDTH - 80, SCREEN_HEIGHT - 24);
    _tft.print((int)sats);
    _tft.print("^");
}

void Display::drawHomeMarker(
    bool homeSet,
    float homeLatDeg,
    float homeLonDeg,
    float curLatDeg,
    float curLonDeg,
    float headingDeg
) {
    if (!homeSet) return;

    float dLat = (curLatDeg - homeLatDeg) * 110540.0f;
    float dLon = (curLonDeg - homeLonDeg) * 111320.0f * cosf(homeLatDeg * DEG_TO_RAD);

    float theta = -headingDeg * DEG_TO_RAD;

    float xRot = dLon * cosf(theta) - dLat * sinf(theta);
    float yRot = dLon * sinf(theta) + dLat * cosf(theta);

    float dist = sqrtf(xRot * xRot + yRot * yRot);

    float screenX, screenY;

    if (dist <= RADAR_CLAMP_DISTANCE_M || dist == 0.0f) {
        float scale = (float)SCREEN_RADIUS / (float)RADAR_CLAMP_DISTANCE_M;
        screenX = SCREEN_CENTER_X + xRot * scale;
        screenY = SCREEN_CENTER_Y - yRot * scale;
    } else {
        screenX = SCREEN_CENTER_X + (xRot / dist) * SCREEN_RADIUS;
        screenY = SCREEN_CENTER_Y - (yRot / dist) * SCREEN_RADIUS;
    }

    _tft.fillCircle((int)screenX, (int)screenY, HOME_MARKER_RADIUS, COLOR_HOME_CIRCLE);

    _tft.setTextColor(COLOR_HOME_TEXT, COLOR_HOME_CIRCLE);
    _tft.setTextSize(1);
    _tft.setCursor((int)screenX - 3, (int)screenY - 4);
    _tft.print("H");
}

void Display::render(
    bool homeSet,
    float homeLatDeg,
    float homeLonDeg,
    float curLatDeg,
    float curLonDeg,
    float headingDeg,
    float speedMs,
    uint8_t sats
) {
    drawBackground();
    drawAircraft();
    drawSpeedAndSats(speedMs, sats);
    drawHomeMarker(homeSet, homeLatDeg, homeLonDeg, curLatDeg, curLonDeg, headingDeg);
}
