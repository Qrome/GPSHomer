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

float Display::computeDynamicClamp(float distM) {
    // Minimum and maximum zoom levels
    const float minClamp = 100.0f;    // closest zoom
    const float maxClamp = 5000.0f;   // farthest zoom

    // Avoid log(0)
    float d = fmaxf(distM, 1.0f);

    // Logarithmic interpolation:
    // clamp = minClamp * (d / minClamp)^0.5
    // This gives smooth, natural zooming
    float ratio = d / minClamp;
    float zoom = minClamp * powf(ratio, 0.5f);  // sqrt curve

    // Clamp to max
    if (zoom > maxClamp) zoom = maxClamp;

    return zoom;
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

float Display::drawHomeMarker(
    bool homeSet,
    float homeLatDeg,
    float homeLonDeg,
    float curLatDeg,
    float curLonDeg,
    float headingDeg
) {
    if (!homeSet) {
        return 100.0f;   // safe default clamp distance
    }

    float dLat = (curLatDeg - homeLatDeg) * 110540.0f;
    float dLon = (curLonDeg - homeLonDeg) * 111320.0f * cosf(homeLatDeg * DEG_TO_RAD);

    float theta = -headingDeg * DEG_TO_RAD;

    float xRot = dLon * cosf(theta) - dLat * sinf(theta);
    float yRot = dLon * sinf(theta) + dLat * cosf(theta);

    float dist = sqrtf(xRot * xRot + yRot * yRot);

    float clampDist = computeDynamicClamp(dist);

    float screenX, screenY;

    if (dist <= clampDist || dist == 0.0f) {
        float scale = (float)SCREEN_RADIUS / clampDist;
        screenX = SCREEN_CENTER_X + xRot * scale;
        screenY = SCREEN_CENTER_Y - yRot * scale;
    } else {
        screenX = SCREEN_CENTER_X + (xRot / dist) * SCREEN_RADIUS;
        screenY = SCREEN_CENTER_Y - (yRot / dist) * SCREEN_RADIUS;
    }

    // Draw the filled circle for H
    _tft.fillCircle((int)screenX, (int)screenY, HOME_MARKER_RADIUS, COLOR_HOME_CIRCLE);

    // Draw the H
    _tft.setTextColor(COLOR_HOME_TEXT, COLOR_HOME_CIRCLE);
    _tft.setTextSize(1);
    _tft.setCursor((int)screenX - 3, (int)screenY - 4);
    _tft.print("H");

    // ---- NEW: Draw distance next to the H marker ----
    float distM = dist; // already in meters

    char buf[16];

#if OSD_UNITS == UNITS_METRIC
    snprintf(buf, sizeof(buf), "%dm", (int)distM);
#else
    int distFt = (int)(distM * 3.28084f);
    snprintf(buf, sizeof(buf), "%dft", distFt);
#endif

    _tft.setTextColor(TFT_SKYBLUE, COLOR_BACKGROUND);
    _tft.setTextSize(1);

    int textX = (int)screenX + HOME_MARKER_RADIUS + 4;  // right of H
    int textY;

    // If H is below center → print above
    if (screenY > SCREEN_CENTER_Y) {
        textY = (int)screenY - HOME_MARKER_RADIUS - 10;
    }
    // If H is above center → print below
    else {
        textY = (int)screenY + HOME_MARKER_RADIUS + 2;
    }

    _tft.setCursor(textX, textY);
    _tft.print(buf);

    return clampDist;
}

void Display::drawDistanceRings(float clampDist) {
#if OSD_SHOW_RINGS == 0
    return;
#endif

    // Ring radii as percentages of clamp distance
    const float ringPercents[3] = {0.25f, 0.50f, 1.0f};

    _tft.setTextColor(COLOR_RING, COLOR_BACKGROUND);
    _tft.setTextSize(1);

    for (int i = 0; i < 3; i++) {
        float pct = ringPercents[i];
        int radius = (int)(SCREEN_RADIUS * pct);

        // Draw the ring
        _tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, radius, COLOR_RING);

        // Label the ring
        float ringDist = clampDist * pct;

#if OSD_UNITS == UNITS_METRIC
        char buf[16];
        snprintf(buf, sizeof(buf), "%dm", (int)ringDist);
#else
        int ft = (int)(ringDist * 3.28084f);
        char buf[16];
        snprintf(buf, sizeof(buf), "%dft", ft);
#endif

        // Draw label slightly above the ring
        _tft.setCursor(SCREEN_CENTER_X + radius + 4, SCREEN_CENTER_Y - 4);
        _tft.print(buf);
    }
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

    float clampDist = drawHomeMarker(
        homeSet, homeLatDeg, homeLonDeg, curLatDeg, curLonDeg, headingDeg
    );

    drawDistanceRings(clampDist);
}

