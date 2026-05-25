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
static int s_prevNorthTipX  = -1;
static int s_prevNorthTipY  = -1;
static int s_prevNorthLx    = -1;
static int s_prevNorthLy    = -1;
static int s_prevNorthRx    = -1;
static int s_prevNorthRy    = -1;


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
    const float minClamp = 100.0f;   // minimum zoom
    const float maxClamp = 1000.0f;  // maximum ring size

    if (distM < minClamp)
        return minClamp;

    if (distM > maxClamp)
        return maxClamp;

    return distM;  // linear scaling
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
        _tft.setCursor(sx - 5, sy - 6);
        _tft.print("H");
    }

    drawDistanceRings(clampDist, headingDeg);

    if (!homeSet) {
        return;
    }

    s_prevHX = sx;
    s_prevHY = sy;
}

void Display::drawDistanceRings(float clampDist, float headingDeg) {
#if OSD_SHOW_RINGS == 0
    return;
#endif
        
    // Only redraw rings when clampDist changes significantly
    if (s_prevClampDist > 0 && fabsf(clampDist - s_prevClampDist) < 5.0f) {
        // no redraw needed
    } else {
        // Erase old rings area
        _tft.fillCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, SCREEN_RADIUS, COLOR_BACKGROUND);
    }

    // Re‑draw aircraft (static)
    drawAircraft();
    drawNorth(headingDeg);

    const float ringPercents[3] = {0.25f, 0.60f, 1.0f};

    _tft.setTextColor(COLOR_RING, COLOR_BACKGROUND);
    _tft.setTextSize(1);

    for (int i = 0; i < 3; i++) {
        float pct = ringPercents[i];
        int radius = (int)(SCREEN_RADIUS * pct);

        _tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, radius - 2, COLOR_RING);

        float ringDist = clampDist * pct;

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

    s_prevClampDist = clampDist;
}

void Display::drawNorth(float headingDeg) {
    // --- Draw North arrow on outer ring ---
    float psi = headingDeg * DEG_TO_RAD;

    // North direction in heading‑up display (rotate world by -heading)
    float nx = sinf(-psi);
    float ny = cosf(-psi);

    // Outer ring radius
    int outerR = SCREEN_RADIUS - 2;

    // Tip of the arrow (touching the ring)
    int tipX = SCREEN_CENTER_X + (int)(nx * outerR);
    int tipY = SCREEN_CENTER_Y - (int)(ny * outerR);

    // Base of the arrow (slightly inward)
    int baseR = outerR - 10;
    int baseX = SCREEN_CENTER_X + (int)(nx * baseR);
    int baseY = SCREEN_CENTER_Y - (int)(ny * baseR);

    // Arrow width (perpendicular to North direction)
    float px = -ny;
    float py = nx;
    int w = 6;

    int leftX  = baseX + (int)(px * w);
    int leftY  = baseY - (int)(py * w);
    int rightX = baseX - (int)(px * w);
    int rightY = baseY + (int)(py * w);

    // --- ERASE previous arrow if it exists ---
    if (s_prevNorthTipX >= 0) {
        _tft.fillTriangle(
            s_prevNorthTipX, s_prevNorthTipY,
            s_prevNorthLx,   s_prevNorthLy,
            s_prevNorthRx,   s_prevNorthRy,
            COLOR_BACKGROUND
        );
    }

    // --- DRAW new arrow ---
    _tft.fillTriangle(
        tipX,  tipY,
        leftX, leftY,
        rightX, rightY,
        COLOR_RING
    );

    // Save for next erase
    s_prevNorthTipX = tipX;
    s_prevNorthTipY = tipY;
    s_prevNorthLx   = leftX;
    s_prevNorthLy   = leftY;
    s_prevNorthRx   = rightX;
    s_prevNorthRy   = rightY;
}

void Display::drawDistance(
    float homeLatDeg,
    float homeLonDeg,
    float curLatDeg,
    float curLonDeg
) {
    // ---- TOP-CENTER DISTANCE DISPLAY (always on top) ----
    // Erase previous distance text area
    //_tft.fillRect(SCREEN_CENTER_X - 30, 20, 90, 24, COLOR_BACKGROUND);

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
