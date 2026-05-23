#pragma once
#include <Arduino.h>

class GPS {
public:
    GPS();

    // Attach a UART port (e.g. Serial1)
    void begin(HardwareSerial &port);

    // Try multiple baud rates until valid NMEA is seen
    bool autodetectBaud(const uint32_t *baudList, size_t baudCount,
                        uint32_t perBaudTimeoutMs = 250);

    // Call frequently (e.g. in loop1)
    void update();

    // Status
    bool hasFix() const;
    bool isHealthy() const;
    uint8_t getFixType() const;     // 0 = none, 2 = 2D, 3 = 3D
    uint8_t getSatCount() const;
    uint32_t lastUpdateMs() const;

    // Position (1e-7 degrees)
    int32_t getLatitude() const;
    int32_t getLongitude() const;

    // Altitude (cm, MSL)
    int32_t getAltitudeMSL() const;

    // Velocity
    uint16_t getGroundSpeed() const; // cm/s
    uint16_t getCourse() const;      // deg * 10

    // Time (optional, Unix or 0 if unknown)
    uint32_t getUnixTime() const;

private:
    HardwareSerial *_port;
    bool _healthy;
    bool _hasFix;
    uint8_t _fixType;
    uint8_t _satCount;
    int32_t _lat;        // 1e-7 deg
    int32_t _lon;        // 1e-7 deg
    int32_t _altMSL;     // cm
    uint16_t _groundSpeed; // cm/s
    uint16_t _course;      // deg * 10
    uint32_t _lastUpdateMs;
    uint32_t _unixTime;

    // NMEA line buffer
    static const uint16_t NMEA_BUF_LEN = 96;
    char _nmeaBuf[NMEA_BUF_LEN];
    uint16_t _nmeaPos;

    void resetState();
    void processByte(char c);
    void processSentence(const char *s);

    void parseGGA(const char *s);
    void parseRMC(const char *s);

    static bool parseLatLon(const char *field, char hemi, int32_t &out1e7);
    static int32_t parseInt(const char *s);
    static uint32_t parseUInt(const char *s);
    static uint16_t parseUInt16(const char *s);
    static float parseFloat(const char *s);
};
