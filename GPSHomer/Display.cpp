#include "Display.h"
#include <math.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295f
#endif

// --- Local state for previous dynamic elements (no header changes needed) ---
static int   s_prevHX         = -1;
static int   s_prevHY         = -1;
static float s_prevClampDist  = -1.0f;
static float s_prevSpeedMs    = -999.0f;
static uint8_t s_prevSats     = 255;

Display::Display()
    : _tft(TFT_eSPI())
{
}

void Display::begin() {
    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(COLOR_BACKGROUND);

    // Static aircraft symbol – draw once
    drawAircraft();
}

void Display::showBootBaud(uint32_t baud) {
    _tft.fillScreen(COLOR_BACKGROUND);
    _tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _tft.setTextSize(2);
    _tft.setCursor(20, 100);
    _tft.print("GPSHomer by Qrome");
    // _tft.setCursor(20, 130);
    // _tft.print(baud);
}

float Display::computeDynamicClamp(float distM) {
    const float minClamp = 100.0f;
    const float maxClamp = 5000.0f;

    float d = fmaxf(distM, 1.0f);
    float ratio = d / minClamp;
    float zoom = minClamp * powf(ratio, 0.5f);

    if (zoom > maxClamp) zoom = maxClamp;
    return zoom;
}

// Background is static now – no per‑frame fillScreen()
void Display::drawBackground() {
    // Intentionally empty
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
    // Only update if changed enough to matter
    if (fabsf(speedMs - s_prevSpeedMs) < 0.1f && sats == s_prevSats) {
        // return;
    } else {
        // Erase old speed/sats area
        //_tft.fillRect(70, SCREEN_HEIGHT - 40, 140, 22, COLOR_BACKGROUND);
        //_tft.fillRect(SCREEN_WIDTH - 95, SCREEN_HEIGHT - 40, 90, 22, COLOR_BACKGROUND);
    }

    _tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _tft.setTextSize(2);

    _tft.setCursor(70, SCREEN_HEIGHT - 40);
#if OSD_UNITS == UNITS_METRIC
    _tft.print(speedMs, 1);
    _tft.print(" m/s");
#else
    _tft.print(speedMs * 2.23694f, 0);
    _tft.print(" mph");
#endif

    _tft.setTextSize(1);
    _tft.setCursor(SCREEN_WIDTH - 85, SCREEN_HEIGHT - 40);
    _tft.print((int)sats);
    _tft.print(" sat");
    
    s_prevSpeedMs = speedMs;
    s_prevSats    = sats;
}

void Display::drawHomeMarker(
    bool homeSet,
    float homeLatDeg,
    float homeLonDeg,
    float curLatDeg,
    float curLonDeg,
    float headingDeg
) {
    if (!homeSet) {
        // Erase any previous H if we had one
        if (s_prevHX >= 0) {
            _tft.fillCircle(s_prevHX, s_prevHY, HOME_MARKER_RADIUS + 5, COLOR_BACKGROUND);
            s_prevHX = -1;
            s_prevHY = -1;
        }
    }

    // Correct deltas: home minus current
    float dLat = (curLatDeg - homeLatDeg) * 110540.0f;
    float dLon = (curLonDeg - homeLonDeg) * 111320.0f * cosf(curLatDeg * DEG_TO_RAD);

    // Compute ENU deltas (North, East)
    float north = (homeLatDeg - curLatDeg) * 110540.0f;
    float east  = (homeLonDeg - curLonDeg) * 111320.0f * cosf(curLatDeg * DEG_TO_RAD);

    // Convert ENU → aircraft body frame
    float psi = headingDeg * DEG_TO_RAD;

    // Forward/back (X), Right/left (Y)
    float xRot =  north * cosf(psi) + east * sinf(psi);
    float yRot = -north * sinf(psi) + east * cosf(psi);

    // --- Smooth H marker movement ---
    static bool first = true;
    static float filtX = 0;
    static float filtY = 0;
    const float alpha = 0.2f;

    if (first) {
        filtX = xRot;
        filtY = yRot;
        first = false;
    } else {
        filtX = filtX + alpha * (xRot - filtX);
        filtY = filtY + alpha * (yRot - filtY);
    }

    float dist = sqrtf(filtX * filtX + filtY * filtY);
    float clampDist = computeDynamicClamp(dist);

    float screenX, screenY;

    if (dist <= clampDist || dist == 0.0f) {
        float edge = SCREEN_RADIUS - HOME_MARKER_RADIUS;
        float scale = edge / clampDist;

        screenX = SCREEN_CENTER_X + filtY * scale;
        screenY = SCREEN_CENTER_Y - filtX * scale;
    } else {
        float edge = SCREEN_RADIUS - HOME_MARKER_RADIUS;
        screenX = SCREEN_CENTER_X + (filtY / dist) * edge;
        screenY = SCREEN_CENTER_Y - (filtX / dist) * edge;
    }

    int sx = (int)screenX;
    int sy = (int)screenY;

    // Erase old H + distance text area
    if (s_prevHX >= 0) {
        _tft.fillCircle(s_prevHX, s_prevHY, HOME_MARKER_RADIUS + 2, COLOR_BACKGROUND);
    }

    if (homeSet) {
        // Draw new H circle
        _tft.fillCircle(sx, sy, HOME_MARKER_RADIUS, COLOR_HOME_CIRCLE);
        _tft.drawCircle(sx, sy, HOME_MARKER_RADIUS, TFT_RED);

        // Draw the H
        _tft.setTextColor(COLOR_HOME_TEXT, COLOR_HOME_CIRCLE);
        _tft.setTextSize(2);
        _tft.setCursor(sx - 4, sy - 6);
        _tft.print("H");
    }

    drawDistanceRings(clampDist);

    if (!homeSet) {
        return;
    }

    s_prevHX = sx;
    s_prevHY = sy;
}

void Display::drawDistanceRings(float clampDist) {
#if OSD_SHOW_RINGS == 0
    return;
#endif

    // Enforce minimum outer ring distance of 100 meters
    float effectiveClamp = clampDist;
    if (effectiveClamp < 100.0f) {
        effectiveClamp = 100.0f;
    }

    // Maximum ring radius = 1000 m (or 3280 ft)
    if (effectiveClamp > 1000.0f) {
        effectiveClamp = 1000.0f;
    }
        
    // Only redraw rings when clampDist changes significantly
    if (s_prevClampDist > 0 && fabsf(effectiveClamp - s_prevClampDist) < 5.0f) {
        // no redraw needed
    } else {
        // Erase old rings area
        _tft.fillCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, SCREEN_RADIUS, COLOR_BACKGROUND);
    }

    // Re‑draw aircraft (static)
    drawAircraft();

    const float ringPercents[3] = {0.25f, 0.60f, 1.0f};

    _tft.setTextColor(COLOR_RING, COLOR_BACKGROUND);
    _tft.setTextSize(1);

    for (int i = 0; i < 3; i++) {
        float pct = ringPercents[i];
        int radius = (int)(SCREEN_RADIUS * pct);

        _tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, radius - 2, COLOR_RING);

        float ringDist = effectiveClamp * pct;

#if OSD_UNITS == UNITS_METRIC
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)ringDist);
#else
        int ft = (int)(ringDist * 3.28084f);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", ft);
#endif

        int xText = SCREEN_CENTER_X + radius + 4;
        if (i == 2) {
            xText = 8;
        }

        _tft.setCursor(xText, SCREEN_CENTER_Y - 4);
        _tft.print(buf);
    }

    s_prevClampDist = effectiveClamp;
}

void Display::drawDistance(
    float homeLatDeg,
    float homeLonDeg,
    float curLatDeg,
    float curLonDeg
) {
    // ---- TOP-CENTER DISTANCE DISPLAY (always on top) ----
    // Erase previous distance text area
    _tft.fillRect(SCREEN_CENTER_X - 30, 20, 90, 24, COLOR_BACKGROUND);

    // Compute distance from home
    float dLat = (curLatDeg - homeLatDeg) * 110540.0f;
    float dLon = (homeLonDeg - curLonDeg) * 111320.0f * cosf(curLatDeg * DEG_TO_RAD);
    float distM = sqrtf(dLat * dLat + dLon * dLon);

    // --- Smooth distance display ---
    static float filtDist = 0;
    const float alphaDist = 0.15f;

    filtDist = filtDist + alphaDist * (distM - filtDist);

    // Draw new distance text
    _tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _tft.setTextSize(2);
    _tft.setCursor(SCREEN_CENTER_X - 30, 30);

    #if OSD_UNITS == UNITS_METRIC
        _tft.print((int)filtDist);
        _tft.print(" m");
    #else
        int distFt = (int)(filtDist * 3.28084f);
        _tft.print(distFt);
        _tft.print(" ft");
    #endif
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
    _tft.startWrite();

    drawHomeMarker(
        homeSet, homeLatDeg, homeLonDeg, curLatDeg, curLonDeg, headingDeg
    );

    drawDistance(homeLatDeg, homeLonDeg, curLatDeg, curLonDeg);
    drawSpeedAndSats(speedMs, sats);
    _tft.endWrite();
}
