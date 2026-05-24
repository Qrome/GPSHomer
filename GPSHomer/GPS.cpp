#include "GPS.h"
#include <string.h>

GPS::GPS()
    : _port(nullptr),
      _healthy(false),
      _hasFix(false),
      _fixType(0),
      _satCount(0),
      _lat(0),
      _lon(0),
      _altMSL(0),
      _groundSpeed(0),
      _course(0),
      _lastUpdateMs(0),
      _unixTime(0),
      _nmeaPos(0)
{
    memset(_nmeaBuf, 0, sizeof(_nmeaBuf));
}

void GPS::begin(HardwareSerial &port) {
    _port = &port;
    resetState();
}

bool GPS::autodetectBaud(const uint32_t *baudList, size_t baudCount,
                         uint32_t perBaudTimeoutMs) {
    if (!_port) {
        Serial.println("[GPS] no Serial1 Port!");
        return false;
    }

    for (size_t i = 0; i < baudCount; i++) {
        uint32_t baud = baudList[i];
        Serial.print("[GPS] Trying baud: ");
        Serial.println(baud);

        _port->begin(baud);
        resetState();

        uint32_t start = millis();
        while (millis() - start < perBaudTimeoutMs) {
            while (_port->available()) {
                char c = _port->read();
                processByte(c);

                if (_healthy) {
                    Serial.print("[GPS] Lock acquired at baud: ");
                    Serial.println(baud);
                    return true;
                }
            }
        }
    }
    return false;
}

void GPS::update() {
    if (!_port) return;
    while (_port->available()) {
        char c = _port->read();
        processByte(c);
    }
}

bool GPS::hasFix() const        { return _hasFix; }
bool GPS::isHealthy() const     { return _healthy; }
uint8_t GPS::getFixType() const { return _fixType; }
uint8_t GPS::getSatCount() const { return _satCount; }
uint32_t GPS::lastUpdateMs() const { return _lastUpdateMs; }

int32_t GPS::getLatitude() const  { return _lat; }
int32_t GPS::getLongitude() const { return _lon; }
int32_t GPS::getAltitudeMSL() const { return _altMSL; }

uint16_t GPS::getGroundSpeed() const { return _groundSpeed; }
uint16_t GPS::getCourse() const      { return _course; }

uint32_t GPS::getUnixTime() const { return _unixTime; }

void GPS::resetState() {
    _healthy = false;
    _hasFix = false;
    _fixType = 0;
    _satCount = 0;
    _lat = 0;
    _lon = 0;
    _altMSL = 0;
    _groundSpeed = 0;
    _course = 0;
    _lastUpdateMs = 0;
    _unixTime = 0;
    _nmeaPos = 0;
    memset(_nmeaBuf, 0, sizeof(_nmeaBuf));
}

void GPS::processByte(char c) {
    if (c == '$') {
        _nmeaPos = 0;
        _nmeaBuf[_nmeaPos++] = c;
        return;
    }

    if (_nmeaPos == 0) {
        return; // ignore until '$'
    }

    if (c == '\n' || c == '\r') {
        _nmeaBuf[_nmeaPos] = '\0';
        if (_nmeaPos > 6) {
            processSentence(_nmeaBuf);
        }
        _nmeaPos = 0;
        return;
    }

    if (_nmeaPos < NMEA_BUF_LEN - 1) {
        _nmeaBuf[_nmeaPos++] = c;
    }
}

void GPS::processSentence(const char *s) {
    // s starts with '$'
    if (strstr(s, "GGA") == s + 3) {
        parseGGA(s);
    } else if (strstr(s, "RMC") == s + 3) {
        parseRMC(s);
    }
}

void GPS::parseGGA(const char *s) {
    // $GxGGA,time,lat,NS,lon,EW,fix,sats,hdop,alt,M,...
    const char *p = s;
    int field = 0;
    char latStr[16] = {0};
    char lonStr[16] = {0};
    char ns = 0, ew = 0;
    uint8_t fix = 0;
    uint8_t sats = 0;
    float alt = 0.0f;

    char buf[20];
    int bufPos = 0;

    auto flushField = [&](void) {
        buf[bufPos] = '\0';
        switch (field) {
            case 2: strncpy(latStr, buf, sizeof(latStr) - 1); break;
            case 3: ns = buf[0]; break;
            case 4: strncpy(lonStr, buf, sizeof(lonStr) - 1); break;
            case 5: ew = buf[0]; break;
            case 6: fix = (uint8_t)parseUInt(buf); break;
            case 7: sats = (uint8_t)parseUInt(buf); break;
            case 9: alt = parseFloat(buf); break;
            default: break;
        }
        bufPos = 0;
        field++;
    };

    while (*p) {
        char c = *p++;
        if (c == ',' || c == '*') {
            flushField();
            if (c == '*') break;
        } else if (bufPos < (int)sizeof(buf) - 1) {
            buf[bufPos++] = c;
        }
    }

    int32_t lat1e7 = 0, lon1e7 = 0;
    if (parseLatLon(latStr, ns, lat1e7) && parseLatLon(lonStr, ew, lon1e7)) {
        _lat = lat1e7;
        _lon = lon1e7;
        _altMSL = (int32_t)(alt * 100.0f); // m → cm
        _fixType = fix;
        _satCount = sats;
        _hasFix = (fix >= 2);
        _healthy = true;
        _lastUpdateMs = millis();
    }
}

void GPS::parseRMC(const char *s) {
    // $GxRMC,time,status,lat,NS,lon,EW,sog,cog,date,...
    const char *p = s;
    int field = 0;

    char sogStr[16] = {0};
    char cogStr[16] = {0};

    char buf[20];
    int bufPos = 0;

    auto flushField = [&](void) {
        buf[bufPos] = '\0';
        switch (field) {
            case 7: strncpy(sogStr, buf, sizeof(sogStr) - 1); break;
            case 8: strncpy(cogStr, buf, sizeof(cogStr) - 1); break;
            default: break;
        }
        bufPos = 0;
        field++;
    };

    while (*p) {
        char c = *p++;
        if (c == ',' || c == '*') {
            flushField();
            if (c == '*') break;
        } else if (bufPos < (int)sizeof(buf) - 1) {
            buf[bufPos++] = c;
        }
    }

    float sogKnots = parseFloat(sogStr); // knots
    float cogDeg   = parseFloat(cogStr); // degrees

    // knots → m/s → cm/s
    float sogMs = sogKnots * 0.514444f;
    _groundSpeed = (uint16_t)(sogMs * 100.0f);
    _course = (uint16_t)(cogDeg * 10.0f);
    _healthy = true;
    _lastUpdateMs = millis();
}

bool GPS::parseLatLon(const char *field, char hemi, int32_t &out1e7) {
    if (!field || !*field) return false;

    // ddmm.mmmm or dddmm.mmmm
    float v = parseFloat(field);
    if (v == 0.0f) return false;

    int deg = (int)(v / 100.0f);
    float minutes = v - (deg * 100.0f);
    float degFloat = (float)deg + minutes / 60.0f;

    if (hemi == 'S' || hemi == 'W') {
        degFloat = -degFloat;
    }

    out1e7 = (int32_t)(degFloat * 1e7f);
    return true;
}

int32_t GPS::parseInt(const char *s) {
    if (!s || !*s) return 0;
    return (int32_t)strtol(s, nullptr, 10);
}

uint32_t GPS::parseUInt(const char *s) {
    if (!s || !*s) return 0;
    return (uint32_t)strtoul(s, nullptr, 10);
}

uint16_t GPS::parseUInt16(const char *s) {
    return (uint16_t)parseUInt(s);
}

float GPS::parseFloat(const char *s) {
    if (!s || !*s) return 0.0f;
    return (float)atof(s);
}
