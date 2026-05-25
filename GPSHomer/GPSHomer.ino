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

GPS gps;
Display display;
StatusLED led;

uint32_t g_detectedBaud = 0;
bool g_baudReported = false;
uint32_t g_lastGpsDataMs = 0;
bool rcHighDetected = false;

unsigned long readRcPwm() {
    // Reads HIGH pulse width in microseconds
    return pulseIn(RC_PWM_PIN, HIGH, 25000);  // 25ms timeout
}

void checkRcResetTrigger() {
    unsigned long pwm = readRcPwm();
    if (pwm == 0) return;  // no signal or timeout

    // Rising edge: above threshold
    if (!rcHighDetected && pwm > RC_THRESHOLD) {
        rcHighDetected = true;
    }

    // Falling edge: below threshold AFTER being high
    if (rcHighDetected && pwm < RC_THRESHOLD) {
        rcHighDetected = false;
        resetHomePosition();   // <-- your function
    }
}

void resetHomePosition() {
    if (gps.hasFix()) {
        g_homeLatDeg = gps.getLatitude();
        g_homeLonDeg = gps.getLongitude();
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
        display.render(homeSet, homeLat, homeLon, curLat, curLon, heading, speed, sats);
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

            if (sats >= 6 && !g_homeSet) {
                led.green();
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



