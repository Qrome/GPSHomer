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
      _nmeaPos(0),
      _ubxState(0),
      _useUBX(false)
{
    memset(_nmeaBuf, 0, sizeof(_nmeaBuf));
    memset(_ubxPayload, 0, sizeof(_ubxPayload));
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
                //Serial.print("[GPS] Byte during autodetect: 0x");
                //Serial.println((uint8_t)c, HEX);
                processByte(c);
                if (_healthy || _useUBX) {
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
    
    // Process all available bytes in a tight loop to keep the hardware buffer empty
    while (_port->available() > 0) {
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

    _ubxState = 0;
    _useUBX = false;
    memset(_ubxPayload, 0, sizeof(_ubxPayload));
}

void GPS::processByte(char c) {
    bool consumedByUBX = false;
    //Serial.print(c);
    // ----------------------------------------------------
    // UBX binary protocol state machine
    // ----------------------------------------------------
    switch (_ubxState) {
        case 0:
            if ((uint8_t)c == 0xB5) {
                _ubxState = 1;
                consumedByUBX = true; // Protect the sync byte from NMEA
            }
            break;

        case 1:
            consumedByUBX = true;
            if ((uint8_t)c == 0x62) _ubxState = 2;
            else _ubxState = 0;
            break;

        case 2:
            consumedByUBX = true;
            _ubxClass = (uint8_t)c;
            _ubxState = 3;
            break;

        case 3:
            consumedByUBX = true;
            _ubxID = (uint8_t)c;
            _ubxState = 4;
            break;

        case 4:
            consumedByUBX = true;
            _ubxLen = (uint8_t)c;
            _ubxState = 5;
            break;

        case 5:
            consumedByUBX = true;
            _ubxLen |= ((uint16_t)(uint8_t)c << 8);
            _ubxIndex = 0;
            _ubxCKA = 0;
            _ubxCKB = 0;
            _ubxState = 6;
            break;

        case 6:
            consumedByUBX = true;
            if (_ubxIndex < sizeof(_ubxPayload)) {
                _ubxPayload[_ubxIndex++] = (uint8_t)c;
            }
            _ubxCKA += (uint8_t)c;
            _ubxCKB += _ubxCKA;

            if (_ubxIndex >= _ubxLen) {
                _ubxState = 7;
            }
            break;

        case 7:
            consumedByUBX = true;
            if ((uint8_t)c == _ubxCKA) _ubxState = 8;
            else _ubxState = 0;
            break;

        case 8:
            consumedByUBX = true;
            if ((uint8_t)c == _ubxCKB) {
                // Valid UBX packet
                Serial.print("[UBX] Class: ");
                Serial.print(_ubxClass, HEX);
                Serial.print(" ID: ");
                Serial.print(_ubxID, HEX);
                Serial.print(" Len: ");
                Serial.println(_ubxLen);

                if (_ubxClass == 0x01 && _ubxID == 0x07) {
                    Serial.println("[UBX] NAV-PVT received");
                    parseUBX_NAV_PVT(_ubxPayload);
                    _useUBX = true;   // switch protocol
                    _healthy = true;  // mark GPS as good
                }
            } else {
                Serial.println("[UBX] Checksum FAIL");
            }
            _ubxState = 0;
            break;
    }

    // If the UBX parser actively locked onto or is reading a frame,
    // do NOT let this byte touch the NMEA text state machine.
    if (consumedByUBX) {
        return;
    }

    // ----------------------------------------------------
    // NMEA text protocol
    // ----------------------------------------------------
    if (c == '$') {
        _nmeaPos = 0;
        _nmeaBuf[_nmeaPos++] = c;
        return;
    }

    if (_nmeaPos == 0) return;

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

bool GPS::validChecksum(const char *s) {
    const char *star = strchr(s, '*');
    if (!star) {
        //Serial.println("[NMEA] Missing checksum");
        return false;
    }

    uint8_t cs = 0;
    for (const char *p = s + 1; p < star; ++p)
        cs ^= (uint8_t)*p;

    uint8_t sent = (uint8_t)strtoul(star + 1, nullptr, 16);
    if (cs != sent) {
        //Serial.println("[NMEA] Checksum FAIL");
        return false;
    }
    return cs == sent;
}

void GPS::processSentence(const char *s) {
    if (_useUBX) {
        return; // UBX mode active — ignoring NMEA
    }

    if (s[0] != '$') return;
    if (!validChecksum(s)) return;

    // Find the comma to isolate the NMEA address field (e.g., "$GNGGA")
    const char *comma = strchr(s, ',');
    if (!comma) return;
    
    int headerLen = comma - s;
    if (headerLen < 6) return; // Must be at least $XXYYY

    // Look at the last 3 characters of the header field (e.g., "GGA" or "RMC")
    if (strncmp(comma - 3, "GGA", 3) == 0) {
        parseGGA(s);
    } else if (strncmp(comma - 3, "RMC", 3) == 0) {
        parseRMC(s);
    }
}

void GPS::parseGGA(const char *s) {
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
        _altMSL = (int32_t)(alt * 100.0f);
        _fixType = fix;
        _satCount = sats;
        _hasFix = (fix >= 1);
        _healthy = true;
        _lastUpdateMs = millis();
    }
}

void GPS::parseRMC(const char *s) {
    const char *p = s;
    int field = 0;

    char status = 'V';
    char latStr[16] = {0};
    char lonStr[16] = {0};
    char ns = 0, ew = 0;
    char sogStr[16] = {0};
    char cogStr[16] = {0};

    char buf[20];
    int bufPos = 0;

    auto flushField = [&](void) {
        buf[bufPos] = '\0';
        switch (field) {
            case 2: status = buf[0]; break;
            case 3: strncpy(latStr, buf, sizeof(latStr) - 1); break;
            case 4: ns = buf[0]; break;
            case 5: strncpy(lonStr, buf, sizeof(lonStr) - 1); break;
            case 6: ew = buf[0]; break;
            case 7: strncpy(sogStr, buf, sizeof(sogStr) - 1); break;
            case 8: strncpy(cogStr, buf, sizeof(cogStr) - 1); break;
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

    float sogKnots = parseFloat(sogStr);
    float cogDeg   = parseFloat(cogStr);

    if (status == 'A') {
        _hasFix = true;

        int32_t lat1e7 = 0, lon1e7 = 0;
        if (parseLatLon(latStr, ns, lat1e7) && parseLatLon(lonStr, ew, lon1e7)) {
            _lat = lat1e7;
            _lon = lon1e7;
        }
    }

    float sogMs = sogKnots * 0.514444f;
    _groundSpeed = (uint16_t)(sogMs * 100.0f);
    _course      = (uint16_t)(cogDeg * 10.0f);
    _healthy     = true;
    _lastUpdateMs = millis();
}

void GPS::parseUBX_NAV_PVT(const uint8_t *p) {
    _healthy = true;
    uint8_t fixType = p[20];
    uint8_t numSV   = p[23];

    int32_t lon = *(int32_t*)(p + 24);
    int32_t lat = *(int32_t*)(p + 28);
    int32_t hMSL = *(int32_t*)(p + 36);

    uint32_t gSpeed = *(uint32_t*)(p + 60);
    uint32_t headMot = *(uint32_t*)(p + 64);

    Serial.print("[UBX] FixType=");
    Serial.print(fixType);
    Serial.print(" Sats=");
    Serial.print(numSV);
    Serial.print(" Lat=");
    Serial.print(lat);
    Serial.print(" Lon=");
    Serial.println(lon);

    _fixType = fixType;
    _satCount = numSV;

    if (fixType >= 2) {
        _hasFix = true;
        _lat = lat;
        _lon = lon;
        _altMSL = hMSL / 10;
        _groundSpeed = gSpeed / 10;
        _course = headMot / 1000;
        _healthy = true;
        _lastUpdateMs = millis();
    }
}

bool GPS::parseLatLon(const char *field, char hemi, int32_t &out1e7) {
    if (!field || !*field) return false;

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
