#include <Arduino.h>
#include "config.h"
#include "GPS.h"
#include "Display.h"

// Shared state between cores
volatile bool  g_homeSet      = false;
volatile float g_homeLatDeg   = 0.0f;
volatile float g_homeLonDeg   = 0.0f;

volatile float g_curLatDeg    = 0.0f;
volatile float g_curLonDeg    = 0.0f;
volatile float g_speedMs      = 0.0f;
volatile float g_headingDeg   = 0.0f;
volatile uint8_t g_sats       = 0;

GPS gps;
Display display;

uint32_t g_detectedBaud = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial1.setRX(PIN_GPS_RX);
    Serial1.setTX(PIN_GPS_TX);

    gps.begin(Serial1);
    display.begin();

    const uint32_t baudList[] = {9600, 38400, 57600, 115200};
    g_detectedBaud = 0;

    for (size_t i = 0; i < sizeof(baudList) / sizeof(baudList[0]); i++) {
        if (gps.autodetectBaud(&baudList[i], 1)) {
            g_detectedBaud = baudList[i];
            break;
        }
    }

    if (g_detectedBaud == 0) {
        g_detectedBaud = GPS_DEFAULT_BAUD;
    }

    display.showBootBaud(g_detectedBaud);
    delay(BOOT_BAUD_DISPLAY_MS);
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

        display.render(homeSet, homeLat, homeLon, curLat, curLon, heading, speed, sats);
    }
}

// Core 1 on RP2040 (Arduino core supports loop1)
void loop1() {
    while (true) {
        gps.update();

        if (gps.isHealthy()) {
            int32_t lat1e7 = gps.getLatitude();
            int32_t lon1e7 = gps.getLongitude();

            float latDeg = lat1e7 * 1e-7f;
            float lonDeg = lon1e7 * 1e-7f;

            float speedMs = gps.getGroundSpeed() * 0.01f; // cm/s → m/s
            float headingDeg = gps.getCourse() * 0.1f;    // deg*10 → deg
            uint8_t sats = gps.getSatCount();

            if (gps.hasFix() && !g_homeSet) {
                g_homeLatDeg = latDeg;
                g_homeLonDeg = lonDeg;
                g_homeSet = true;
            }

            g_curLatDeg  = latDeg;
            g_curLonDeg  = lonDeg;
            g_speedMs    = speedMs;
            g_headingDeg = headingDeg;
            g_sats       = sats;
        }

        delay(5);
    }
}
