#include <Arduino.h>
#include "config.h"
#include "GPS.h"
#include "Display.h"
#include "LED.h"

// Shared state between cores
volatile bool  g_homeSet      = false;
volatile float g_homeLatDeg   = 0.0f;
volatile float g_homeLonDeg   = 0.0f;

volatile float g_curLatDeg    = 0.0f;
volatile float g_curLonDeg    = 0.0f;
volatile float g_speedMs      = 0.0f;
volatile float g_headingDeg   = 0.0f;
volatile uint8_t g_sats       = 0;

volatile float g_maxDistance = 0.0f;
volatile float g_maxSpeed = 0.0f;
volatile float g_totalDistance = 0.0f;
volatile float g_avgSpeed = 0.0f;
volatile uint32_t g_flightStartMs = 0;
bool g_showingSummary = false;
static float fs_lastLat = 0;
static float fs_lastLon = 0;
static bool  fs_first   = true;



GPS gps;
Display display;
StatusLED led;

uint32_t g_detectedBaud = 0;
bool g_baudReported = false;
uint32_t g_lastGpsDataMs = 0;
bool rcHighDetected = false;

void resetFlightSummary() {
    g_maxDistance = 0;
    g_maxSpeed = 0;
    g_totalDistance = 0;
    g_avgSpeed = 0;
    g_flightStartMs = millis();
}

unsigned long readRcPwm() {
    unsigned long pulse = pulseIn(RC_PWM_PIN, HIGH, 25000);  // 25ms timeout

    // No signal or floating pin → return 0
    if (pulse < 900 || pulse > 2200) {
        return 0;
    }

    return pulse;
}


void checkRcResetTrigger() {
    static bool prevHigh = false;
    static unsigned long highStart = 0;
    static unsigned long lastClickTime = 0;
    static int clickCount = 0;

    unsigned long pwm = readRcPwm();
    bool pwmValid = (pwm >= 900 && pwm <= 2200);

    // ----------------------------------------------------
    // NO RC SIGNAL → disable summary mode completely
    // ----------------------------------------------------
    if (!pwmValid) {
        g_showingSummary = false;
        prevHigh = false;
        clickCount = 0;
        return;
    }

    bool isHigh = pwm > RC_THRESHOLD;
    unsigned long now = millis();

    // ----------------------------------------------------
    // SUMMARY MODE: ignore ALL click logic
    // ----------------------------------------------------
    if (g_showingSummary) {

        // Exit summary when PWM goes LOW
        if (!isHigh && prevHigh) {
            g_showingSummary = false;
            display.drawBackground();
        }

        prevHigh = isHigh;
        return;   // <<< prevents click detection
    }

    // ----------------------------------------------------
    // Detect rising edge (LOW → HIGH)
    // ----------------------------------------------------
    if (isHigh && !prevHigh) {
        highStart = now;
    }

    // ----------------------------------------------------
    // HIGH hold → enter summary mode
    // ----------------------------------------------------
    if (isHigh && (now - highStart >= RC_SUMMARY_HOLD_MS)) {
        g_showingSummary = true;

        float maxD, maxS, totD, avgS;
        uint32_t startMs;

        noInterrupts();
        maxD    = g_maxDistance;
        maxS    = g_maxSpeed;
        totD    = g_totalDistance;
        avgS    = g_avgSpeed;
        startMs = g_flightStartMs;
        interrupts();

        display.showSummary(maxD, maxS, totD, avgS, startMs);

        prevHigh = isHigh;
        return;
    }

    // ----------------------------------------------------
    // Detect falling edge (HIGH → LOW) → click detection
    // ----------------------------------------------------
    if (!isHigh && prevHigh) {

        if (now - lastClickTime <= RC_DOUBLE_TAP_WINDOW_MS) {
            clickCount++;
        } else {
            clickCount = 1;
        }

        lastClickTime = now;

        // Double tap detected → reset home
        if (clickCount == 2) {
            clickCount = 0;
            resetHomePosition();
        }
    }

    prevHigh = isHigh;
}


void resetHomePosition() {
    if (!gps.hasFix()) {
        return;   // do NOT reset home without a valid fix
    }

    // GPS library returns 1e-7 scaled integers
    int32_t lat1e7 = gps.getLatitude();
    int32_t lon1e7 = gps.getLongitude();

    float lat = lat1e7 * 1e-7f;
    float lon = lon1e7 * 1e-7f;

    // Reject invalid coordinates
    if (!isfinite(lat) || !isfinite(lon)) return;
    if (lat == 0.0f && lon == 0.0f) return;
    if (fabs(lat) > 90.0f || fabs(lon) > 180.0f) return;

    g_homeLatDeg = lat;
    g_homeLonDeg = lon;
    g_homeSet = true;

    resetFlightSummary();
    fs_first = true;
}




void updateFlightSummary(float latDeg, float lonDeg, float speedMs) {

    if (g_showingSummary) {
        return;   // freeze summary values while summary screen is active
    }

    if (!g_homeSet) {
        fs_first = true;
        return;
    }

    float dLat = (latDeg - g_homeLatDeg) * 110540.0f;
    float dLon = (lonDeg - g_homeLonDeg) * 111320.0f * cosf(latDeg * DEG_TO_RAD);
    float distM = sqrtf(dLat*dLat + dLon*dLon);

    if (distM > g_maxDistance)
        g_maxDistance = distM;

    if (speedMs > g_maxSpeed)
        g_maxSpeed = speedMs;

    if (!fs_first) {
        float dLat2 = (latDeg - fs_lastLat) * 110540.0f;
        float dLon2 = (lonDeg - fs_lastLon) * 111320.0f * cosf(latDeg * DEG_TO_RAD);
        float segment = sqrtf(dLat2*dLat2 + dLon2*dLon2);
        g_totalDistance += segment;
    }

    fs_first = false;
    fs_lastLat = latDeg;
    fs_lastLon = lonDeg;

    uint32_t elapsed = millis() - g_flightStartMs;
    if (elapsed > 0) {
        g_avgSpeed = g_totalDistance / (elapsed * 0.001f);
    }
}


void setup() {
    Serial.begin(115200);
    delay(500);

    led.begin();
    led.blue();   // Boot color

    pinMode(RC_PWM_PIN, INPUT);

    display.begin();

    // Show boot baud (will be updated once setup1 detects real baud)
    display.showBootBaud(g_detectedBaud);
    delay(BOOT_BAUD_DISPLAY_MS);
    display.drawBackground();
}


void loop() {
    static uint32_t lastFrameMs = 0;
    uint32_t now = millis();
    uint32_t frameInterval = 1000 / RADAR_DRAW_FPS;

    if (now - lastFrameMs >= frameInterval) {
        lastFrameMs = now;

        bool homeSet;
        float homeLat, homeLon, curLat, curLon, speed, heading;
        uint8_t sats;

        noInterrupts();
        homeSet = g_homeSet;
        homeLat = g_homeLatDeg;
        homeLon = g_homeLonDeg;
        curLat  = g_curLatDeg;
        curLon  = g_curLonDeg;
        speed   = g_speedMs;
        heading = g_headingDeg;
        sats    = g_sats;
        interrupts();

        checkRcResetTrigger();
        if (!g_showingSummary) {
            display.render(homeSet, homeLat, homeLon, curLat, curLon, heading, speed, sats);
        }
    }
}

// Core 1 setup
void setup1() {
    // Wait for USB Serial to be ready
    delay(5000);
    
    // Assign GPS UART pins
    Serial1.setRX(PIN_GPS_RX);
    Serial1.setTX(PIN_GPS_TX);

    // Start GPS UART
    gps.begin(Serial1);

    delay(1500); // wait for GPS to sink up

    // Baud autodetection
    g_detectedBaud = 0;

    for (size_t i = 0; i < sizeof(baudList) / sizeof(baudList[0]); i++) {
        if (gps.autodetectBaud(&baudList[i], 1, 1000)) {
            g_detectedBaud = baudList[i];
            break;
        }
        delay(500);
    }

    if (g_detectedBaud == 0) {
        g_detectedBaud = GPS_DEFAULT_BAUD;
    }

    g_lastGpsDataMs = millis();   // initialize GPS heartbeat
}


// Core 1 on RP2040 (Arduino core supports loop1)
void loop1() {
    static uint32_t lastUpdate = 0;
    static uint32_t lastSerial = 0;

    const uint32_t UPDATE_INTERVAL_MS = 5;
    const uint32_t SERIAL_INTERVAL_MS = 2000;

    while (true) {
        uint32_t now = millis();

        // Run GPS update every 5ms
        if (now - lastUpdate < UPDATE_INTERVAL_MS) {
            tight_loop_contents();
            continue;
        }
        lastUpdate = now;

        gps.update();

        // Track GPS communication health
        if (gps.isHealthy()) {
            g_lastGpsDataMs = now;  // GPS is talking to us

            int32_t lat1e7 = gps.getLatitude();
            int32_t lon1e7 = gps.getLongitude();

            float latDeg = lat1e7 * 1e-7f;
            float lonDeg = lon1e7 * 1e-7f;

            float speedMs = gps.getGroundSpeed() * 0.01f; // cm/s → m/s
            float headingDeg = gps.getCourse() * 0.1f;    // deg*10 → deg
            uint8_t sats = gps.getSatCount();

            // AUTO‑SET HOME ON FIRST VALID FIX
            if (!g_homeSet && gps.hasFix() && sats >= 6) {

                int32_t lat1e7 = gps.getLatitude();
                int32_t lon1e7 = gps.getLongitude();

                float lat = lat1e7 * 1e-7f;
                float lon = lon1e7 * 1e-7f;

                // Validate coordinates
                if (lat != 0.0f && lon != 0.0f &&
                    isfinite(lat) && isfinite(lon) &&
                    fabs(lat) <= 90.0f && fabs(lon) <= 180.0f)
                {
                    led.green();
                    g_homeLatDeg = lat;
                    g_homeLonDeg = lon;
                    g_homeSet = true;

                    resetFlightSummary();
                    fs_first = true;
                }
            }


            g_curLatDeg  = latDeg;
            g_curLonDeg  = lonDeg;
            g_speedMs    = speedMs;
            g_headingDeg = headingDeg;
            g_sats       = sats;

            updateFlightSummary(latDeg, lonDeg, speedMs);
        }

        // GPS communication timeout → LED amber
        if (now - g_lastGpsDataMs > 2000) {
            led.amber();
        }

        // Serial monitoring every 1 second
        if (now - lastSerial >= SERIAL_INTERVAL_MS) {
            lastSerial = now;

            Serial.print("Sats: ");
            Serial.print(g_sats);
            Serial.print(" | Fix: ");
            Serial.print(gps.hasFix() ? "YES" : "NO");
            Serial.print(" | HomeSet: ");
            Serial.print(g_homeSet ? "YES" : "NO");
            Serial.print(" | Baud: ");
            Serial.print(g_detectedBaud);
            Serial.print(" | Speed: ");
            Serial.print(g_speedMs);
            Serial.print(" | Lat: ");
            Serial.print(g_curLatDeg, 7);
            Serial.print(" | Lon: ");
            Serial.println(g_curLonDeg, 7);
        }
    }
}



