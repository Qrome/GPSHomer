#include "Display.h"
#include "config.h"
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
static float s_lastValidHeading = 0.0f;



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
    const float maxClamp = 1609.35f;  // maximum ring size

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
    _tft.print(speedMs / 1000.0f, 1);
    _tft.print(" kph");
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
    float headingDeg,
    float speedMs
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

    // --- Sticky heading logic ---
    // headingDeg is only valid when moving; freeze when stopped
    float speedThreshold = 0.5f;  // m/s, adjust as needed

    // --- Sticky heading logic ---
    // GPS heading is only valid when moving; freeze when stopped
    const float motionThreshold = 0.5f;  // m/s

    if (speedMs > motionThreshold) {
        // Update last valid heading only when actually moving
        s_lastValidHeading = headingDeg;
    }

    // Use the retained heading when stopped
    float psi = s_lastValidHeading * DEG_TO_RAD;

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

#if ENABLE_TRAIL
    drawTrail(homeLatDeg, homeLonDeg, headingDeg, clampDist);
#endif

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
    float psi = s_lastValidHeading * DEG_TO_RAD;

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
    // Compute distance from home (meters)
    float dLat = (curLatDeg - homeLatDeg) * 110540.0f;
    float dLon = (homeLonDeg - curLonDeg) * 111320.0f * cosf(curLatDeg * DEG_TO_RAD);
    float distM = sqrtf(dLat * dLat + dLon * dLon);

    // --- Smooth distance display ---
    static float filtDist = 0;
    const float alphaDist = 0.15f;
    filtDist = filtDist + alphaDist * (distM - filtDist);

    // Erase previous distance text area
    //_tft.fillRect(SCREEN_CENTER_X - 40, 26, 90, 20, TFT_BLUE);

    _tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _tft.setTextSize(2);
    _tft.setCursor(SCREEN_CENTER_X - 40, 30);

#if OSD_UNITS == UNITS_METRIC
    // --- METRIC MODE ---
    if (filtDist < 1609.35f) {
        // Show meters
        _tft.print((int)filtDist);
        _tft.print(" m ");
    } else {
        // Show kilometers
        float km = filtDist / 1000.0f;
        _tft.print(km, 2);   // one decimal place
        _tft.print(" km ");
    }

#else
    // --- IMPERIAL MODE ---
    float distFt = filtDist * 3.28084f;

    if (distFt < 5280.0f) {
        // Show feet
        _tft.print((int)distFt);
        _tft.print(" ft ");
    } else {
        // Show miles
        float miles = distFt / 5280.0f;
        _tft.print(miles, 2);  // one decimal place
        _tft.print(" mi ");
    }
#endif
}

// --- Small summary icons ---
void Display::drawIconRadar(int x, int y, uint16_t color) {
    _tft.drawCircle(x, y, 6, color);
    _tft.drawCircle(x, y, 3, color);
    _tft.drawLine(x, y, x + 6, y - 6, color);
}

void Display::drawIconSpeedometer(int x, int y, uint16_t color) {
    _tft.drawCircle(x, y, 6, color);
    _tft.drawLine(x, y, x + 4, y - 4, color);
    _tft.drawLine(x, y, x + 6, y, color);
}

void Display::drawIconPath(int x, int y, uint16_t color) {
    _tft.fillCircle(x, y, 2, color);
    _tft.drawLine(x + 2, y, x + 10, y, color);
    _tft.fillCircle(x + 12, y, 2, color);
}

void Display::drawIconGauge(int x, int y, uint16_t color) {
    _tft.drawCircle(x, y, 6, color);
    _tft.drawLine(x, y, x + 4, y - 4, color);
}

void Display::drawIconClock(int x, int y, uint16_t color) {
    _tft.drawCircle(x, y, 6, color);
    _tft.drawLine(x, y, x, y - 4, color);
    _tft.drawLine(x, y, x + 3, y, color);
}


void Display::showSummary(float maxDist, float maxSpeed, float totalDist, float avgSpeed, uint32_t flightStartMs) {
    _tft.fillScreen(TFT_BLACK);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.setTextSize(2);

    _tft.setCursor(75, 20);
    _tft.print("SUMMARY");

    float maxDistOut   = maxDist;
    float totalDistOut = totalDist;
    const char* distUnitShort = nullptr;
    const char* distUnitLong  = nullptr;
    float maxSpeedOut = 0.0f;
    float avgSpeedOut = 0.0f;
    const char* speedUnit = nullptr;

    // how many decimals to show for distances
    int maxDistPrec   = 0;
    int totalDistPrec = 0;

#if OSD_UNITS == UNITS_METRIC
    distUnitShort = "m";
    distUnitLong  = "km";

    // Switch to km if either value exceeds 1000 m
    bool useKm = (totalDist > 1000.0f) || (maxDist > 1000.0f);

    if (useKm) {
        maxDistOut   = maxDist / 1000.0f;
        totalDistOut = totalDist / 1000.0f;
        distUnitShort = distUnitLong;
        maxDistPrec   = 2;
        totalDistPrec = 2;
    } else {
        maxDistPrec   = 0;
        totalDistPrec = 0;
    }

    maxSpeedOut = maxSpeed * 3.6f;
    avgSpeedOut = avgSpeed * 3.6f;
    speedUnit   = "km/h";

#else
    float maxDistFt   = maxDist * 3.28084f;
    float totalDistFt = totalDist * 3.28084f;

    maxDistOut   = maxDistFt;
    totalDistOut = totalDistFt;
    distUnitShort = "ft";
    distUnitLong  = "mi";

    // Switch to miles if either value exceeds 5280 ft
    bool useMiles = (totalDistFt > 5280.0f) || (maxDistFt > 5280.0f);

    if (useMiles) {
        maxDistOut   = maxDistFt / 5280.0f;
        totalDistOut = totalDistFt / 5280.0f;
        distUnitShort = distUnitLong;
        maxDistPrec   = 2;
        totalDistPrec = 2;
    } else {
        maxDistPrec   = 0;
        totalDistPrec = 0;
    }

    maxSpeedOut = maxSpeed * 2.23694f;
    avgSpeedOut = avgSpeed * 2.23694f;
    speedUnit   = "mph";
#endif

    // --- Overflow protection (prevents "ovf") ---
    if (!isfinite(maxDistOut)   || maxDistOut   > 999999.0f) maxDistOut   = 999999.0f;
    if (!isfinite(totalDistOut) || totalDistOut > 999999.0f) totalDistOut = 999999.0f;

    // Flight time
    uint32_t elapsedMs = millis() - flightStartMs;
    uint32_t totalSec  = elapsedMs / 1000;
    uint32_t hours     = totalSec / 3600;
    uint32_t minutes   = (totalSec % 3600) / 60;
    uint32_t seconds   = totalSec % 60;

    char timeBuf[16];
    if (hours > 0)
        snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u:%02u", hours, minutes, seconds);
    else
        snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", minutes, seconds);

    uint16_t iconColor  = TFT_GREEN;
    uint16_t valueColor = TFT_YELLOW;

    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.setTextSize(2);

    // Max Distance
    drawIconRadar(40, 55, iconColor);
    _tft.setCursor(50, 50);
    _tft.print("M Dst:");
    _tft.setTextColor(valueColor, TFT_BLACK);
    _tft.print(maxDistOut, maxDistPrec);
    _tft.print(" ");
    _tft.print(distUnitShort);

    // Max Speed
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawIconSpeedometer(20, 85, iconColor);
    _tft.setCursor(30, 80);
    _tft.print("M Spd:");
    _tft.setTextColor(valueColor, TFT_BLACK);
    _tft.print(maxSpeedOut, 1);
    _tft.print(" ");
    _tft.print(speedUnit);

    // Total Distance
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawIconPath(10, 115, iconColor);
    _tft.setCursor(30, 110);
    _tft.print("T Dst:");
    _tft.setTextColor(valueColor, TFT_BLACK);
    _tft.print(totalDistOut, totalDistPrec);
    _tft.print(" ");
    _tft.print(distUnitShort);

    // Avg Speed
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawIconGauge(20, 145, iconColor);
    _tft.setCursor(30, 140);
    _tft.print("A Spd:");
    _tft.setTextColor(valueColor, TFT_BLACK);
    _tft.print(avgSpeedOut, 1);
    _tft.print(" ");
    _tft.print(speedUnit);

    // Flight Time
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawIconClock(40, 175, iconColor);
    _tft.setCursor(50, 170);
    _tft.print("Time:");
    _tft.setTextColor(valueColor, TFT_BLACK);
    _tft.print(timeBuf);
}


void Display::drawTrail(float homeLat, float homeLon, float headingDeg, float clampDist) {

#if ENABLE_TRAIL == 0
    return;
#else

    if (g_trailCount < 2) return;

    int prevX = 0, prevY = 0;
    bool havePrev = false;

    for (int i = 0; i < g_trailCount; i++) {

        float lat = g_trail[i].lat;
        float lon = g_trail[i].lon;

        // Convert to meters relative to home
        float dLat = (lat - homeLat) * 110540.0f;
        float dLon = (lon - homeLon) * 111320.0f * cosf(lat * DEG_TO_RAD);

        // Rotate around aircraft heading
        float psi = headingDeg * DEG_TO_RAD;
        float xr = dLon * cosf(psi) - (-dLat) * sinf(psi);
        float yr = dLon * sinf(psi) + (-dLat) * cosf(psi);

        // Cull points outside clamp distance
        float dist = sqrtf(xr*xr + yr*yr);
        if (dist > clampDist) {
            havePrev = false;
            continue;
        }

        // Scale to pixels using same logic as home marker
        float edge = SCREEN_RADIUS - 4;
        float scale = edge / clampDist;

        int sx = SCREEN_CENTER_X + xr * scale;
        int sy = SCREEN_CENTER_Y - yr * scale;

        // Draw line segment
        if (havePrev) {
            _tft.drawLine(prevX, prevY, sx, sy, TFT_DARKGREY);
        }

        prevX = sx;
        prevY = sy;
        havePrev = true;
    }
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

    drawDistance(homeLatDeg, homeLonDeg, curLatDeg, curLonDeg);

    drawHomeMarker(
        homeSet, homeLatDeg, homeLonDeg, curLatDeg, curLonDeg, headingDeg, speedMs
    );

    drawSpeedAndSats(speedMs, sats);
    _tft.endWrite();
}
