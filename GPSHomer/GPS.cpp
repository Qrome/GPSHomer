// -------------------------------------------------------
// GPS.cpp
// RP2040 Zero + BN220 (u-blox M10)
// NMEA parser + UBX CFG-VALSET configuration
// -------------------------------------------------------

#include <string.h>
#include "GPS.h"

// NOTE: All constexpr constants (EARTH_RADIUS_M, UBX_SYNC_1,
// KEY_RATE_MEAS, etc.) are defined in GPS.h — do NOT redefine
// them here. GPS.cpp inherits them by including GPS.h.

// -------------------------------------------------------
// File-scoped haversine — forward declaration
// Not exposed in GPS.h — internal use only
// -------------------------------------------------------
static float haversineDistM(int32_t lat1e7, int32_t lon1e7, int32_t lat2e7,
                            int32_t lon2e7);

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------
GPS::GPS()
    : _port(nullptr),
      _healthy(false),
      _hasFix(false),
      _homeSet(false),
      _isM10(false),
      _fixType(0U),
      _satCount(0U),
      _lat(0),
      _lon(0),
      _altMSL(0),
      _homeLat1e7(0),
      _homeLon1e7(0),
      _groundSpeed(0U),
      _course(0U),
      _nmeaPos(0U),
      _lastUpdateMs(0UL),
      _unixTime(0UL),
      _distHomeM(0.0f) {
    memset(_nmeaBuf, 0, sizeof(_nmeaBuf));
}

// -------------------------------------------------------
// begin()
// -------------------------------------------------------
void GPS::begin(HardwareSerial &port) {
    _port = &port;
    resetState();
}

// -------------------------------------------------------
// autodetectBaud()
// -------------------------------------------------------
bool GPS::autodetectBaud(const uint32_t *baudList, size_t baudCount,
                         uint32_t perBaudTimeoutMs) {
    if (!_port) {
        Serial.println(F("[GPS] autodetectBaud: No port assigned!"));
        return false;
    }

    if (!baudList || baudCount == 0U) {
        Serial.println(F("[GPS] autodetectBaud: Empty baud list!"));
        return false;
    }

    for (size_t i = 0U; i < baudCount; i++) {
        uint32_t baud = baudList[i];
        Serial.print(F("[GPS] Trying baud: "));
        Serial.println(baud);

        _port->begin(baud);
        resetState();

        uint32_t start = millis();
        while ((millis() - start) < perBaudTimeoutMs) {
            while (_port->available()) {
                char c = static_cast<char>(_port->read());
                processByte(c);

                if (_healthy) {
                    Serial.print(F("[GPS] Valid NMEA detected at baud: "));
                    Serial.println(baud);
                    configureM10_VALSET();
                    return true;
                }
            }
        }

        Serial.print(F("[GPS] No response at baud: "));
        Serial.println(baud);
    }

    Serial.println(
        F("[GPS] ERROR: Autodetect FAILED — no valid GPS data found."));
    Serial.println(F("[GPS] Check TX/RX wiring, power supply, and module."));
    return false;
}

// -------------------------------------------------------
// update()
// Non-blocking — call every loop iteration on Core 1
// -------------------------------------------------------
void GPS::update() {
    if (!_port) return;

    while (_port->available()) {
        char c = static_cast<char>(_port->read());
        processByte(c);
    }

    // Staleness check — mark unhealthy if no update recently
    if (_healthy && ((millis() - _lastUpdateMs) > GPS_STALE_TIMEOUT_MS)) {
        _healthy = false;
        _hasFix = false;
        Serial.println(F("[GPS] Warning: No data received — marked stale."));
    }
}

// -------------------------------------------------------
// State accessors
// -------------------------------------------------------
bool GPS::hasFix() const { return _hasFix; }
bool GPS::isHealthy() const { return _healthy; }
bool GPS::isHomeSet() const { return _homeSet; }
uint8_t GPS::getFixType() const { return _fixType; }
uint8_t GPS::getSatCount() const { return _satCount; }
uint32_t GPS::lastUpdateMs() const { return _lastUpdateMs; }
int32_t GPS::getLatitude() const { return _lat; }
int32_t GPS::getLongitude() const { return _lon; }
int32_t GPS::getAltitudeMSL() const { return _altMSL; }
uint16_t GPS::getGroundSpeed() const { return _groundSpeed; }
uint16_t GPS::getCourse() const { return _course; }
uint32_t GPS::getUnixTime() const { return _unixTime; }
float GPS::getDistanceFromHomeM() const { return _distHomeM; }

// -------------------------------------------------------
// resetHome()
// -------------------------------------------------------
void GPS::resetHome() {
    _homeSet = false;
    _distHomeM = 0.0f;
}

// -------------------------------------------------------
// forceSetHome()
// -------------------------------------------------------
void GPS::forceSetHome() {
    if (!_hasFix || _satCount < MIN_SATS_FOR_HOME) {
        Serial.println(F("[GPS] forceSetHome: Fix or sat count insufficient."));
        return;
    }
    _homeLat1e7 = _lat;
    _homeLon1e7 = _lon;
    _homeSet = true;
    _distHomeM = 0.0f;
    Serial.println(F("[GPS] Home position set."));
}

// -------------------------------------------------------
// getBearingToHomeDeg()
// -------------------------------------------------------
float GPS::getBearingToHomeDeg() const {
    if (!_homeSet || !_hasFix) return 0.0f;

    float lat1 = static_cast<float>(_lat) * 1.0e-7f * DEG_TO_RAD;
    float lon1 = static_cast<float>(_lon) * 1.0e-7f * DEG_TO_RAD;
    float lat2 = static_cast<float>(_homeLat1e7) * 1.0e-7f * DEG_TO_RAD;
    float lon2 = static_cast<float>(_homeLon1e7) * 1.0e-7f * DEG_TO_RAD;

    float dlon = lon2 - lon1;
    float y = sinf(dlon) * cosf(lat2);
    float x = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(dlon);

    float brng = atan2f(y, x) * RAD_TO_DEG;
    if (brng < 0.0f) brng += 360.0f;
    return brng;
}

// -------------------------------------------------------
// resetState()
// -------------------------------------------------------
void GPS::resetState() {
    _healthy = false;
    _hasFix = false;
    _fixType = 0U;
    _satCount = 0U;
    _lat = 0;
    _lon = 0;
    _altMSL = 0;
    _groundSpeed = 0U;
    _course = 0U;
    _lastUpdateMs = 0UL;
    _unixTime = 0UL;
    _nmeaPos = 0U;
    memset(_nmeaBuf, 0, sizeof(_nmeaBuf));
}

// -------------------------------------------------------
// processByte()
// -------------------------------------------------------
void GPS::processByte(char c) {
    const bool isValid = (c == '$') || (c == ',') || (c == '.') || (c == '*') ||
                         (c == '+') || (c == '-') || (c == ' ') ||
                         (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                         (c >= 'a' && c <= 'z') || (c == '\r') || (c == '\n');

    if (!isValid) {
        _nmeaPos = 0U;
        return;
    }

    if (c == '$') {
        _nmeaPos = 0U;
        _nmeaBuf[_nmeaPos++] = c;
        return;
    }

    if (_nmeaPos == 0U) return;

    if (c == '\n' || c == '\r') {
        _nmeaBuf[_nmeaPos] = '\0';
        if (_nmeaPos > 6U) {
            processSentence(_nmeaBuf);
        }
        _nmeaPos = 0U;
        return;
    }

    if (_nmeaPos < static_cast<uint16_t>(NMEA_BUF_LEN - 1U)) {
        _nmeaBuf[_nmeaPos++] = c;
    } else {
        // Buffer overrun — discard
        _nmeaPos = 0U;
    }
}

// -------------------------------------------------------
// processSentence()
// Accepts $GP, $GN, $GL talker IDs
// -------------------------------------------------------
void GPS::processSentence(const char *s) {
    if (!s) return;
    if (s[0] != '$') return;
    if (s[1] != 'G') return;

    const char talker = s[2];
    if (talker != 'P' && talker != 'N' && talker != 'L') return;

    const char t0 = s[3];
    const char t1 = s[4];
    const char t2 = s[5];

    if (t0 == 'G' && t1 == 'G' && t2 == 'A')
        parseGGA(s);
    else if (t0 == 'R' && t1 == 'M' && t2 == 'C')
        parseRMC(s);
}

// -------------------------------------------------------
// parseGGA()
// Fields: 0=ID, 1=time, 2=lat, 3=NS, 4=lon, 5=EW,
// 6=fix, 7=sats, 8=HDOP, 9=alt, 10=M, ...
// -------------------------------------------------------
void GPS::parseGGA(const char *s) {
    if (!s) return;

    const char *p = s;
    int field = 0;
    int bufPos = 0;
    char buf[NMEA_FIELD_BUF_LEN];

    char latStr[NMEA_FIELD_BUF_LEN] = {'\0'};
    char lonStr[NMEA_FIELD_BUF_LEN] = {'\0'};
    char ns = '\0';
    char ew = '\0';
    uint8_t fix = 0U;
    uint8_t sats = 0U;
    float alt = 0.0f;

    memset(buf, 0, sizeof(buf));

    while (*p != '\0') {
        char c = *p++;

        if (c == ',' || c == '*') {
            buf[bufPos] = '\0';

            switch (field) {
                case 2:
                    strncpy(latStr, buf, NMEA_FIELD_BUF_LEN - 1U);
                    latStr[NMEA_FIELD_BUF_LEN - 1U] = '\0';
                    break;
                case 3:
                    ns = buf[0];
                    break;
                case 4:
                    strncpy(lonStr, buf, NMEA_FIELD_BUF_LEN - 1U);
                    lonStr[NMEA_FIELD_BUF_LEN - 1U] = '\0';
                    break;
                case 5:
                    ew = buf[0];
                    break;
                case 6:
                    fix = static_cast<uint8_t>(parseUInt(buf));
                    break;
                case 7:
                    sats = static_cast<uint8_t>(parseUInt(buf));
                    break;
                case 9:
                    alt = parseFloat(buf);
                    break;
                default:
                    break;
            }

            bufPos = 0;
            memset(buf, 0, sizeof(buf));
            field++;
            if (c == '*') break;

        } else if (bufPos < (NMEA_FIELD_BUF_LEN - 1)) {
            buf[bufPos++] = c;
        }
    }

    // Sanity checks
    if (sats > MAX_SATS_SANITY) {
        Serial.println(F("[GPS] GGA: Rejected — implausible sat count."));
        return;
    }

    int32_t lat1e7 = 0;
    int32_t lon1e7 = 0;

    if (!parseLatLon(latStr, ns, lat1e7) || !parseLatLon(lonStr, ew, lon1e7)) {
        return;
    }

    if (lat1e7 == 0 && lon1e7 == 0) return;

    // Commit parsed data
    _lat = lat1e7;
    _lon = lon1e7;
    _altMSL = static_cast<int32_t>(alt * ALT_CM_SCALE);
    _fixType = fix;
    _satCount = sats;
    _hasFix = (fix >= MIN_FIX_QUALITY);
    _healthy = true;
    _lastUpdateMs = millis();

    // Auto-set home on first valid fix with enough satellites
    if (!_homeSet && _hasFix && (_satCount >= MIN_SATS_FOR_HOME)) {
        _homeLat1e7 = _lat;
        _homeLon1e7 = _lon;
        _homeSet = true;
        _distHomeM = 0.0f;
        Serial.println(F("[GPS] Home position auto-set."));
    }

    // Rate-guarded haversine — only runs on new GGA, not every loop()
    if (_homeSet && _hasFix && (_satCount >= MIN_SATS_FOR_HOME)) {
        _distHomeM = haversineDistM(_homeLat1e7, _homeLon1e7, _lat, _lon);
    } else {
        _distHomeM = 0.0f;
    }
}

// -------------------------------------------------------
// parseRMC()
// Fields: 0=ID, 1=time, 2=status(A/V), 3=lat, 4=NS,
// 5=lon, 6=EW, 7=SOG(knots), 8=COG(degrees),
// 9=date, 10=mag var, 11=EW, 12=mode(optional)
//
// GGA is the authority for position and fix status.
// RMC is used only for speed and course.
// -------------------------------------------------------
void GPS::parseRMC(const char *s) {
    if (!s) return;

    const char *p = s;
    int field = 0;
    int bufPos = 0;
    char buf[NMEA_FIELD_BUF_LEN];

    char status = 'V';  // Default void — reject unless 'A'
    char sogStr[NMEA_FIELD_BUF_LEN] = {'\0'};
    char cogStr[NMEA_FIELD_BUF_LEN] = {'\0'};

    memset(buf, 0, sizeof(buf));

    while (*p != '\0') {
        char c = *p++;

        if (c == ',' || c == '*') {
            buf[bufPos] = '\0';

            switch (field) {
                // Field 0: sentence ID — skip
                case 2:
                    status = (bufPos > 0) ? buf[0] : 'V';
                    break;
                // Fields 3–6: lat/lon — GGA is preferred source, skip
                case 7:
                    strncpy(sogStr, buf, NMEA_FIELD_BUF_LEN - 1U);
                    sogStr[NMEA_FIELD_BUF_LEN - 1U] = '\0';
                    break;
                case 8:
                    strncpy(cogStr, buf, NMEA_FIELD_BUF_LEN - 1U);
                    cogStr[NMEA_FIELD_BUF_LEN - 1U] = '\0';
                    break;
                // Fields 9–12: date, mag variation, mode — not used
                default:
                    break;
            }

            bufPos = 0;
            memset(buf, 0, sizeof(buf));
            field++;
            if (c == '*') break;

        } else if (bufPos < (NMEA_FIELD_BUF_LEN - 1)) {
            buf[bufPos++] = c;
        }
    }

    // Only accept data from an Active (valid) fix
    if (status != 'A') {
        // RMC void — do not update speed or course.
        // Do NOT clear _hasFix here — GGA is the fix authority.
        return;
    }

    // Parse speed and course
    float sogKnots = parseFloat(sogStr);
    float cogDeg = parseFloat(cogStr);

    // Defensive: reject negative or implausibly high speed
    if (sogKnots < 0.0f || sogKnots > MAX_SPEED_KNOTS_SANITY) {
        Serial.println(F("[GPS] RMC: Rejected — implausible speed."));
        return;
    }

    // Convert knots to cm/s (1 knot = KNOTS_TO_CMS cm/s)
    float sogCms = sogKnots * KNOTS_TO_CMS;
    _groundSpeed = static_cast<uint16_t>(sogCms);

    // Clamp course to valid compass range before scaling
    if (cogDeg < 0.0f) cogDeg = 0.0f;
    if (cogDeg > 360.0f) cogDeg = 360.0f;

    // Store course as degrees * COURSE_SCALE (e.g. 123.4 deg = 1234)
    _course = static_cast<uint16_t>(cogDeg * COURSE_SCALE);

    // RMC confirms healthy data stream
    _healthy = true;
    _lastUpdateMs = millis();
}

// -------------------------------------------------------
// parseLatLon()
// Converts NMEA DDMM.MMMMM or DDDMM.MMMMM string to
// a signed integer in units of 1e-7 degrees.
// Returns false if the field is empty or zero (no fix).
// -------------------------------------------------------
bool GPS::parseLatLon(const char *field, char hemi, int32_t &out1e7) {
    if (!field || field[0] == '\0') return false;

    float raw = parseFloat(field);
    if (raw == 0.0f) return false;

    // NMEA format: DDDMM.MMMM
    // Degrees = integer part of (raw / 100)
    // Minutes = remainder
    int deg = static_cast<int>(raw / 100.0f);
    float minutes = raw - static_cast<float>(deg * 100);
    float degFloat = static_cast<float>(deg) + (minutes / 60.0f);

    // Apply hemisphere sign
    if (hemi == 'S' || hemi == 'W') {
        degFloat = -degFloat;
    }

    out1e7 = static_cast<int32_t>(degFloat * 1.0e7f);
    return true;
}

// -------------------------------------------------------
// parseInt() / parseUInt() / parseUInt16() / parseFloat()
// Centralised string-to-number conversion.
// All guard against null and empty strings.
// -------------------------------------------------------
int32_t GPS::parseInt(const char *s) {
    if (!s || s[0] == '\0') return 0;
    return static_cast<int32_t>(strtol(s, nullptr, 10));
}

uint32_t GPS::parseUInt(const char *s) {
    if (!s || s[0] == '\0') return 0U;
    return static_cast<uint32_t>(strtoul(s, nullptr, 10));
}

uint16_t GPS::parseUInt16(const char *s) {
    return static_cast<uint16_t>(parseUInt(s));
}

float GPS::parseFloat(const char *s) {
    if (!s || s[0] == '\0') return 0.0f;
    return static_cast<float>(atof(s));
}

// -------------------------------------------------------
// haversineDistM()
// File-scoped — not exposed in GPS.h.
// Computes great-circle distance in metres between two
// positions given as 1e-7 degree integers.
// Only called from parseGGA() — rate-guarded by sentence rate.
// -------------------------------------------------------
static float haversineDistM(int32_t lat1e7, int32_t lon1e7, int32_t lat2e7,
                            int32_t lon2e7) {
    const float lat1 = static_cast<float>(lat1e7) * 1.0e-7f * DEG_TO_RAD;
    const float lon1 = static_cast<float>(lon1e7) * 1.0e-7f * DEG_TO_RAD;
    const float lat2 = static_cast<float>(lat2e7) * 1.0e-7f * DEG_TO_RAD;
    const float lon2 = static_cast<float>(lon2e7) * 1.0e-7f * DEG_TO_RAD;

    const float dlat = lat2 - lat1;
    const float dlon = lon2 - lon1;
    const float sinDlat = sinf(dlat * 0.5f);
    const float sinDlon = sinf(dlon * 0.5f);

    const float a =
        (sinDlat * sinDlat) + cosf(lat1) * cosf(lat2) * (sinDlon * sinDlon);

    const float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

    return EARTH_RADIUS_M * c;
}


// -------------------------------------------------------
// waitForUBXAck()
// Non-blocking state machine that reads incoming bytes
// looking for UBX ACK-ACK (0xB5 0x62 0x05 0x01) or
// ACK-NAK (0xB5 0x62 0x05 0x00).
// Exits on first match or after UBX_ACK_TIMEOUT_MS.
// Returns true only on ACK-ACK.
// -------------------------------------------------------
bool GPS::waitForUBXAck() {
    if (!_port) return false;

    enum class AckState : uint8_t {
        WAIT_SYNC1,  // waiting for 0xB5
        WAIT_SYNC2,  // waiting for 0x62
        WAIT_CLASS,  // waiting for 0x05 (ACK class)
        WAIT_ID      // 0x01 = ACK-ACK, 0x00 = ACK-NAK
    };

    AckState state = AckState::WAIT_SYNC1;
    uint32_t start = millis();

    while ((millis() - start) < UBX_ACK_TIMEOUT_MS) {
        while (_port->available()) {
            const uint8_t b = static_cast<uint8_t>(_port->read());

            switch (state) {
                case AckState::WAIT_SYNC1:
                    // Look for first UBX sync byte
                    if (b == UBX_SYNC_1) {
                        state = AckState::WAIT_SYNC2;
                    }
                    break;

                case AckState::WAIT_SYNC2:
                    // Must immediately follow with second sync byte
                    if (b == UBX_SYNC_2) {
                        state = AckState::WAIT_CLASS;
                    } else {
                        // False start — reset and keep looking
                        state = AckState::WAIT_SYNC1;
                    }
                    break;

                case AckState::WAIT_CLASS:
                    // ACK class is always 0x05
                    if (b == UBX_CLASS_ACK) {
                        state = AckState::WAIT_ID;
                    } else {
                        // Different message class — reset
                        state = AckState::WAIT_SYNC1;
                    }
                    break;

                case AckState::WAIT_ID:
                    if (b == UBX_ID_ACK_ACK) {
                        // 0x01 = ACK-ACK — configuration accepted
                        Serial.println(F("[GPS] CFG-VALSET ACK received."));
                        return true;
                    } else if (b == UBX_ID_ACK_NAK) {
                        // 0x00 = ACK-NAK — configuration rejected by module
                        Serial.println(
                            F("[GPS] CFG-VALSET NAK — config rejected by "
                              "module."));
                        Serial.println(
                            F("[GPS] Check: baud rate, key IDs, layer flags."));
                        return false;
                    } else {
                        // Unexpected ID byte — reset state machine
                        state = AckState::WAIT_SYNC1;
                    }
                    break;

                    // No default needed — enum class covers all cases
            }
        }
        // Yield between byte reads — keeps Core 1 responsive
        // No delay() — busy-wait on millis() is non-blocking
    }

    Serial.println(
        F("[GPS] CFG-VALSET ACK timeout — no response from module."));
    Serial.println(F("[GPS] Check: TX/RX wiring, module power, baud rate."));
    return false;
}

// -------------------------------------------------------
// UBX Helper — writeKey()
// Writes a uint32 key into buf at offset (little-endian).
// Returns the new offset after writing 4 bytes.
// -------------------------------------------------------
uint8_t GPS::writeKey(uint8_t *buf, uint8_t offset, uint32_t key) {
    buf[offset++] = static_cast<uint8_t>(key & 0xFFU);
    buf[offset++] = static_cast<uint8_t>((key >> 8U) & 0xFFU);
    buf[offset++] = static_cast<uint8_t>((key >> 16U) & 0xFFU);
    buf[offset++] = static_cast<uint8_t>((key >> 24U) & 0xFFU);
    return offset;
}

// -------------------------------------------------------
// UBX Helper — writeU8()
// Writes a uint8 value into buf at offset.
// Returns the new offset after writing 1 byte.
// -------------------------------------------------------
uint8_t GPS::writeU8(uint8_t *buf, uint8_t offset, uint8_t val) {
    buf[offset++] = val;
    return offset;
}

// -------------------------------------------------------
// UBX Helper — writeU16()
// Writes a uint16 value into buf at offset (little-endian).
// Returns the new offset after writing 2 bytes.
// -------------------------------------------------------
uint8_t GPS::writeU16(uint8_t *buf, uint8_t offset, uint16_t val) {
    buf[offset++] = static_cast<uint8_t>(val & 0xFFU);
    buf[offset++] = static_cast<uint8_t>((val >> 8U) & 0xFFU);
    return offset;
}

// -------------------------------------------------------
// UBX Helper — ubxChecksum()
// Computes UBX Fletcher-8 checksum over buf[start..end).
// Checksum covers: class, id, length, payload.
// Does NOT include the two sync bytes (0xB5, 0x62).
// -------------------------------------------------------
void GPS::ubxChecksum(const uint8_t *buf, size_t start, size_t end,
                      uint8_t &ckA, uint8_t &ckB) {
    ckA = 0U;
    ckB = 0U;
    for (size_t i = start; i < end; i++) {
        ckA += buf[i];
        ckB += ckA;
    }
}

// -------------------------------------------------------
// sendVALSET()
// Assembles a complete UBX CFG-VALSET frame and transmits it.
//
// Frame layout:
//   [0]          0xB5        sync char 1
//   [1]          0x62        sync char 2
//   [2]          0x06        class: CFG
//   [3]          0x8A        id:    VALSET
//   [4]          lenLo       payload length LSB
//   [5]          lenHi       payload length MSB
//   [6..6+len-1] payload     CFG-VALSET payload bytes
//   [6+len]      CK_A        Fletcher checksum A
//   [6+len+1]    CK_B        Fletcher checksum B
//
// Checksum covers bytes [2 .. 6+len) — class through end of payload.
// -------------------------------------------------------
void GPS::sendVALSET(const uint8_t *payload, uint16_t payloadLen) {
    if (!_port || !payload || payloadLen == 0U) return;

    // Frame overhead: sync(2) + class(1) + id(1) + length(2) + ckA(1) + ckB(1)
    static constexpr uint8_t HEADER_LEN = 6U;
    static constexpr uint8_t TAIL_LEN = 2U;
    static constexpr uint8_t OVERHEAD = HEADER_LEN + TAIL_LEN;

    // Maximum payload we will ever send.
    // Current configureM10_VALSET() payload = 51 bytes.
    // Sized to 64 for future growth without stack risk on RP2040.
    static constexpr uint8_t MAX_PAYLOAD = 64U;
    static constexpr uint8_t MAX_FRAME = MAX_PAYLOAD + OVERHEAD;

    // Defensive: reject oversized payload before touching the stack buffer
    if (payloadLen > MAX_PAYLOAD) {
        Serial.println(F("[GPS] sendVALSET: Payload too large — aborted."));
        return;
    }

    const uint16_t frameLen =
        static_cast<uint16_t>(HEADER_LEN + payloadLen + TAIL_LEN);

    // Stack-allocated frame — no heap allocation
    uint8_t frame[MAX_FRAME];
    memset(frame, 0U, sizeof(frame));

    // --- Fill header ---
    frame[0U] = UBX_SYNC_1;
    frame[1U] = UBX_SYNC_2;
    frame[2U] = UBX_CLASS_CFG;
    frame[3U] = UBX_ID_VALSET;
    frame[4U] = static_cast<uint8_t>(payloadLen & 0xFFU);
    frame[5U] = static_cast<uint8_t>((payloadLen >> 8U) & 0xFFU);

    // --- Copy payload into frame body ---
    memcpy(&frame[HEADER_LEN], payload, payloadLen);

    // --- Compute Fletcher checksum over [class..end of payload] ---
    // Checksum range: bytes [2 .. HEADER_LEN + payloadLen)
    uint8_t ckA = 0U;
    uint8_t ckB = 0U;
    ubxChecksum(frame, 2U, static_cast<size_t>(HEADER_LEN + payloadLen), ckA,
                ckB);

    frame[HEADER_LEN + payloadLen] = ckA;
    frame[HEADER_LEN + payloadLen + 1U] = ckB;

    // --- Transmit complete frame in a single write ---
    _port->write(frame, frameLen);
}

// -------------------------------------------------------
// configureM10_VALSET()
//
// Sends a single UBX CFG-VALSET message to the u-blox M10
// that configures:
//   1. Measurement rate (GPS_MEAS_PERIOD_MS)
//   2. Nav solution ratio (UBX_NAV_RATIO = 1)
//   3. Enable  GGA on UART1
//   4. Enable  RMC on UART1
//   5. Disable GSV on UART1
//   6. Disable GSA on UART1
//   7. Disable VTG on UART1
//   8. Disable GLL on UART1
//   9. Disable GST on UART1
//
// CFG-VALSET payload layout:
//   Byte 0:    version  = 0x01
//   Byte 1:    layers   = 0x03 (RAM + BBR)
//   Bytes 2-3: reserved = 0x00, 0x00
//   Then key-value pairs (little-endian):
//     4 bytes key  (uint32)
//     N bytes value (uint8 or uint16 depending on key type)
//
// Payload size calculation:
//   Header        =  4 bytes  (version + layers + reserved)
//   CFG-RATE-MEAS =  6 bytes  (4 key + 2 value uint16)
//   CFG-RATE-NAV  =  6 bytes  (4 key + 2 value uint16)
//   GGA enable    =  5 bytes  (4 key + 1 value uint8)
//   RMC enable    =  5 bytes  (4 key + 1 value uint8)
//   GSV disable   =  5 bytes  (4 key + 1 value uint8)
//   GSA disable   =  5 bytes  (4 key + 1 value uint8)
//   VTG disable   =  5 bytes  (4 key + 1 value uint8)
//   GLL disable   =  5 bytes  (4 key + 1 value uint8)
//   GST disable   =  5 bytes  (4 key + 1 value uint8)
//                 = --------
//   Total         = 51 bytes
//
// Returns true if module responds with ACK-ACK.
// -------------------------------------------------------
bool GPS::configureM10_VALSET() {
    if (!_port) {
        Serial.println(F("[GPS] configureM10_VALSET: No port assigned!"));
        return false;
    }

    // ------------------------------------------------------------------
    // Payload size:
    //   4  header bytes
    //   6  CFG-RATE-MEAS  (uint16 value)
    //   6  CFG-RATE-NAV   (uint16 value)
    //   5  GGA  (uint8 value)
    //   5  RMC  (uint8 value)
    //   5  GSV  (uint8 value)
    //   5  GSA  (uint8 value)
    //   5  VTG  (uint8 value)
    //   5  GLL  (uint8 value)
    //   5  GST  (uint8 value)
    //  ----
    //  51 bytes total
    // ------------------------------------------------------------------
    static constexpr uint8_t  VALSET_HEADER_LEN  = 4U;
    static constexpr uint8_t  KV_U16_LEN         = 6U;   // 4 key + 2 value
    static constexpr uint8_t  KV_U8_LEN          = 5U;   // 4 key + 1 value
    static constexpr uint8_t  NUM_U16_KEYS        = 2U;   // RATE-MEAS, RATE-NAV
    static constexpr uint8_t  NUM_U8_KEYS         = 7U;   // GGA,RMC,GSV,GSA,VTG,GLL,GST

    static constexpr uint8_t  PAYLOAD_LEN =
        VALSET_HEADER_LEN +
        (NUM_U16_KEYS * KV_U16_LEN) +
        (NUM_U8_KEYS  * KV_U8_LEN);
    // PAYLOAD_LEN = 4 + 12 + 35 = 51

    // Stack-allocated payload buffer — no heap used
    uint8_t payload[PAYLOAD_LEN];
    memset(payload, 0U, sizeof(payload));

    uint8_t off = 0U;

    // ------------------------------------------------------------------
    // CFG-VALSET payload header
    // ------------------------------------------------------------------
    payload[off++] = UBX_VALSET_VERSION;   // version = 0x01
    payload[off++] = UBX_LAYER_RAM_BBR;    // layers  = 0x03 (RAM + BBR)
    payload[off++] = 0x00U;                // reserved
    payload[off++] = 0x00U;                // reserved

    // ------------------------------------------------------------------
    // Key-value pairs
    // Each writeKey() advances offset by 4.
    // Each writeU16() advances offset by 2.
    // Each writeU8()  advances offset by 1.
    // ------------------------------------------------------------------

    // --- Measurement period (ms) ---
    // e.g. GPS_UPDATE_RATE_HZ=5 → GPS_MEAS_PERIOD_MS=200
    off = writeKey(payload, off, KEY_RATE_MEAS);
    off = writeU16(payload, off, GPS_MEAS_PERIOD_MS);

    // --- Nav solution ratio (1 = output every measurement) ---
    off = writeKey(payload, off, KEY_RATE_NAV);
    off = writeU16(payload, off, UBX_NAV_RATIO);

    // --- Enable GGA on UART1 ---
    off = writeKey(payload, off, KEY_NMEA_GGA_UART1);
    off = writeU8 (payload, off, 0x01U);

    // --- Enable RMC on UART1 ---
    off = writeKey(payload, off, KEY_NMEA_RMC_UART1);
    off = writeU8 (payload, off, 0x01U);

    // --- Disable GSV on UART1 ---
    off = writeKey(payload, off, KEY_NMEA_GSV_UART1);
    off = writeU8 (payload, off, 0x00U);

    // --- Disable GSA on UART1 ---
    off = writeKey(payload, off, KEY_NMEA_GSA_UART1);
    off = writeU8 (payload, off, 0x00U);

    // --- Disable VTG on UART1 ---
    off = writeKey(payload, off, KEY_NMEA_VTG_UART1);
    off = writeU8 (payload, off, 0x00U);

    // --- Disable GLL on UART1 ---
    off = writeKey(payload, off, KEY_NMEA_GLL_UART1);
    off = writeU8 (payload, off, 0x00U);

    // --- Disable GST on UART1 ---
    off = writeKey(payload, off, KEY_NMEA_GST_UART1);
    off = writeU8 (payload, off, 0x00U);

    // ------------------------------------------------------------------
    // Defensive: verify offset matches expected payload length.
    // If this fires, a key was added or removed without updating
    // PAYLOAD_LEN — catch it at runtime in debug builds.
    // ------------------------------------------------------------------
    if (off != PAYLOAD_LEN) {
        Serial.print(F("[GPS] configureM10_VALSET: payload size mismatch! "
                       "Expected: "));
        Serial.print(PAYLOAD_LEN);
        Serial.print(F(" Got: "));
        Serial.println(off);
        return false;
    }

    // ------------------------------------------------------------------
    // Log what we are about to send
    // ------------------------------------------------------------------
    Serial.print(F("[GPS] Configuring M10: "));
    Serial.print(GPS_UPDATE_RATE_HZ);
    Serial.print(F("Hz (period="));
    Serial.print(GPS_MEAS_PERIOD_MS);
    Serial.println(F("ms), GGA+RMC only..."));

    // ------------------------------------------------------------------
    // Assemble UBX frame and transmit via sendVALSET()
    // ------------------------------------------------------------------
    sendVALSET(payload, PAYLOAD_LEN);

    // ------------------------------------------------------------------
    // Wait for ACK-ACK or ACK-NAK from module
    // Non-blocking — exits on first match or UBX_ACK_TIMEOUT_MS timeout
    // ------------------------------------------------------------------
    const bool acked = waitForUBXAck();

    if (acked) {
        _isM10 = true;
        Serial.println(F("[GPS] M10 configuration complete."));
    } else {
        Serial.println(F("[GPS] M10 configuration FAILED — running with defaults."));
        Serial.println(F("[GPS] OSD will still work but may receive extra sentences."));
    }

    return acked;
}