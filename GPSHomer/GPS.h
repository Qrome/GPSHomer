#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

#define NMEA_BUF_LEN 256

class GPS {
public:
    GPS();

    void begin(HardwareSerial &port);
    bool autodetectBaud(const uint32_t *baudList, size_t baudCount,
                        uint32_t perBaudTimeoutMs);
    void update();

    bool hasFix() const;
    bool isHealthy() const;
    uint8_t getFixType() const;
    uint8_t getSatCount() const;
    uint32_t lastUpdateMs() const;

    int32_t getLatitude() const;
    int32_t getLongitude() const;
    int32_t getAltitudeMSL() const;

    uint16_t getGroundSpeed() const;
    uint16_t getCourse() const;

    uint32_t getUnixTime() const;

private:
    HardwareSerial *_port;

    // NMEA state
    char _nmeaBuf[NMEA_BUF_LEN];
    uint16_t _nmeaPos;

    // UBX state
    uint8_t _ubxState;
    uint8_t _ubxClass;
    uint8_t _ubxID;
    uint16_t _ubxLen;
    uint16_t _ubxIndex;
    uint8_t _ubxPayload[100];
    uint8_t _ubxCKA;
    uint8_t _ubxCKB;

    // Protocol mode
    bool _useUBX;

    // GPS data
    bool _healthy;
    bool _hasFix;
    uint8_t _fixType;
    uint8_t _satCount;
    int32_t _lat;
    int32_t _lon;
    int32_t _altMSL;
    uint16_t _groundSpeed;
    uint16_t _course;
    uint32_t _lastUpdateMs;
    uint32_t _unixTime;

    void resetState();
    void processByte(char c);

    // NMEA
    void processSentence(const char *s);
    bool validChecksum(const char *s);
    void parseGGA(const char *s);
    void parseRMC(const char *s);

    // UBX
    void parseUBX_NAV_PVT(const uint8_t *p);

    // Helpers
    bool parseLatLon(const char *field, char hemi, int32_t &out1e7);
    int32_t parseInt(const char *s);
    uint32_t parseUInt(const char *s);
    uint16_t parseUInt16(const char *s);
    float parseFloat(const char *s);
};

#endif
