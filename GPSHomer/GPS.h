#pragma once

// -------------------------------------------------------
// GPS.h
// RP2040 Zero + BN220 (u-blox M10)
// NMEA parser + UBX CFG-VALSET configuration header
// -------------------------------------------------------

#include <Arduino.h>
#include <HardwareSerial.h>

// -------------------------------------------------------
// Update rate — change this to 1, 5, or 10
// BN220 / u-blox M10 supports up to 10Hz
// -------------------------------------------------------
static constexpr uint8_t  GPS_UPDATE_RATE_HZ        = 10U;

// Derived measurement period in ms — do not edit directly
static constexpr uint16_t GPS_MEAS_PERIOD_MS        =
    static_cast<uint16_t>(1000U / GPS_UPDATE_RATE_HZ);

// -------------------------------------------------------
// Fix quality thresholds
// -------------------------------------------------------

// GGA fix quality field: 0=none, 1=GPS, 2=DGPS
static constexpr uint8_t  MIN_FIX_QUALITY           = 1U;

// Minimum satellites required before home point is accepted
static constexpr uint8_t  MIN_SATS_FOR_HOME         = 6U;

// Reject satellite counts above this (corrupt sentence guard)
static constexpr uint8_t  MAX_SATS_SANITY           = 40U;

// Reject ground speeds above this in knots
// ~272 knots = ~500 km/h — well above any drone limit
static constexpr float    MAX_SPEED_KNOTS_SANITY    = 272.0f;

// -------------------------------------------------------
// Timing
// -------------------------------------------------------

// Mark GPS stale if no valid sentence received within this window
static constexpr uint32_t GPS_STALE_TIMEOUT_MS      = 2000UL;

// Timeout waiting for UBX ACK-ACK after CFG-VALSET
static constexpr uint32_t UBX_ACK_TIMEOUT_MS        = 500UL;

// Default per-baud autodetect timeout in ms
static constexpr uint32_t GPS_BAUD_DETECT_TIMEOUT_MS = 250UL;

// -------------------------------------------------------
// Unit conversion constants
// -------------------------------------------------------

// Speed: 1 knot = 51.4444 cm/s
static constexpr float    KNOTS_TO_CMS              = 51.4444f;

// Altitude: metres to centimetres
static constexpr float    ALT_CM_SCALE              = 100.0f;

// Course: stored as degrees * 10 (e.g. 123.4 deg = 1234)
static constexpr float    COURSE_SCALE              = 10.0f;

// Haversine: Earth radius in metres (WGS-84 mean)
static constexpr float    EARTH_RADIUS_M            = 6371000.0f;

// -------------------------------------------------------
// Buffer sizes
// -------------------------------------------------------

// NMEA sentence buffer — longest standard sentence is ~82 chars
// 256 provides headroom for non-standard implementations
static constexpr uint16_t NMEA_BUF_LEN              = 256U;

// Per-field accumulation buffer inside parsers
static constexpr uint8_t  NMEA_FIELD_BUF_LEN        = 20U;

// -------------------------------------------------------
// UBX protocol constants
// -------------------------------------------------------
static constexpr uint8_t  UBX_SYNC_1               = 0xB5U;
static constexpr uint8_t  UBX_SYNC_2               = 0x62U;
static constexpr uint8_t  UBX_CLASS_CFG            = 0x06U;
static constexpr uint8_t  UBX_ID_VALSET            = 0x8AU;
static constexpr uint8_t  UBX_CLASS_ACK            = 0x05U;
static constexpr uint8_t  UBX_ID_ACK_ACK           = 0x01U;
static constexpr uint8_t  UBX_ID_ACK_NAK           = 0x00U;
static constexpr uint8_t  UBX_VALSET_VERSION       = 0x01U;

// Layers: bit0=RAM, bit1=BBR (battery-backed RAM)
// 0x03 = RAM + BBR — survives power cycle with backup battery
static constexpr uint8_t  UBX_LAYER_RAM_BBR        = 0x03U;

// Nav solution ratio — output every measurement cycle
static constexpr uint16_t UBX_NAV_RATIO            = 1U;

// -------------------------------------------------------
// UBX CFG-VALSET key IDs (u-blox M10)
// All keys are uint32, little-endian in the UBX frame.
// Value sizes:
//   CFG-RATE keys    = uint16 (2 bytes)
//   CFG-MSGOUT keys  = uint8  (1 byte)
// -------------------------------------------------------
static constexpr uint32_t KEY_RATE_MEAS            = 0x30210001UL;
static constexpr uint32_t KEY_RATE_NAV             = 0x30210002UL;
static constexpr uint32_t KEY_NMEA_GGA_UART1       = 0x209100BBUL;
static constexpr uint32_t KEY_NMEA_RMC_UART1       = 0x209100ACUL;
static constexpr uint32_t KEY_NMEA_GSV_UART1       = 0x209100C5UL;
static constexpr uint32_t KEY_NMEA_GSA_UART1       = 0x209100C0UL;
static constexpr uint32_t KEY_NMEA_VTG_UART1       = 0x209100B1UL;
static constexpr uint32_t KEY_NMEA_GLL_UART1       = 0x209100CAUL;
static constexpr uint32_t KEY_NMEA_GST_UART1       = 0x209100CFUL;

// -------------------------------------------------------
// GPS Class
// -------------------------------------------------------
class GPS {
public:

    // -------------------------------------------------------
    // Constructor / initialisation
    // -------------------------------------------------------
    GPS();

    // Assign hardware serial port — call before anything else
    void begin(HardwareSerial &port);

    // Scan baud rates until valid NMEA is detected.
    // Returns true if a baud rate with valid data was found.
    bool autodetectBaud(const uint32_t *baudList,
                        size_t          baudCount,
                        uint32_t        perBaudTimeoutMs = GPS_BAUD_DETECT_TIMEOUT_MS);

    // Non-blocking update — call every loop iteration on Core 1.
    // Reads all available bytes and checks for staleness.
    void update();

    // -------------------------------------------------------
    // Fix state accessors
    // -------------------------------------------------------
    bool     hasFix()         const;   // true if GGA fix quality >= MIN_FIX_QUALITY
    bool     isHealthy()      const;   // true if data is fresh and valid
    uint8_t  getFixType()     const;   // GGA fix quality field (0–6)
    uint8_t  getSatCount()    const;   // satellites in use
    uint32_t lastUpdateMs()   const;   // millis() of last valid sentence

    // -------------------------------------------------------
    // Position accessors
    // -------------------------------------------------------
    int32_t  getLatitude()    const;   // degrees * 1e7
    int32_t  getLongitude()   const;   // degrees * 1e7
    int32_t  getAltitudeMSL() const;   // centimetres MSL

    // -------------------------------------------------------
    // Motion accessors
    // -------------------------------------------------------
    uint16_t getGroundSpeed() const;   // cm/s
    uint16_t getCourse()      const;   // degrees * 10 (e.g. 1234 = 123.4 deg)

    // -------------------------------------------------------
    // Time
    // -------------------------------------------------------
    uint32_t getUnixTime()    const;   // Unix epoch seconds (if parsed)

    // -------------------------------------------------------
    // UBX M10 configuration
    // Sends CFG-VALSET to set update rate and enable
    // GGA + RMC only — disables GSV, GSA, VTG, GLL, GST.
    // Returns true if ACK received from module.
    // -------------------------------------------------------
    bool configureM10_VALSET();

    // -------------------------------------------------------
    // Home point and distance
    // -------------------------------------------------------

    // Haversine distance from current position to home (metres).
    // Computed on each new GGA sentence — not each loop iteration.
    float    getDistanceFromHomeM()  const;

    // Bearing from current position back toward home (degrees, 0–360)
    float    getBearingToHomeDeg()   const;

    // Manually lock home to current position.
    // Requires hasFix() == true and satCount >= MIN_SATS_FOR_HOME.
    void     forceSetHome();

    // True once a valid home point has been recorded
    bool     isHomeSet()             const;

    // Clear home point — distance returns 0.0f until re-set
    void     resetHome();

private:

    // -------------------------------------------------------
    // Hardware
    // -------------------------------------------------------
    HardwareSerial *_port;

    // -------------------------------------------------------
    // Fix state
    // bool/uint8 grouped first to minimise struct padding
    // -------------------------------------------------------
    bool     _healthy;        // fresh valid data received recently
    bool     _hasFix;         // GGA fix quality >= MIN_FIX_QUALITY
    bool     _homeSet;        // home point has been recorded
    bool     _isM10;          // true if module identified as u-blox M10

    uint8_t  _fixType;        // GGA fix quality field value
    uint8_t  _satCount;       // satellites in use

    // -------------------------------------------------------
    // Position  (int32 grouped)
    // -------------------------------------------------------
    int32_t  _lat;            // degrees * 1e7
    int32_t  _lon;            // degrees * 1e7
    int32_t  _altMSL;         // centimetres MSL

    // -------------------------------------------------------
    // Home point  (int32 grouped)
    // -------------------------------------------------------
    int32_t  _homeLat1e7;
    int32_t  _homeLon1e7;

    // -------------------------------------------------------
    // Motion  (uint16 grouped)
    // -------------------------------------------------------
    uint16_t _groundSpeed;    // cm/s
    uint16_t _course;         // degrees * 10
    uint16_t _nmeaPos;        // write index into _nmeaBuf

    // -------------------------------------------------------
    // Timing  (uint32 grouped)
    // -------------------------------------------------------
    uint32_t _lastUpdateMs;   // millis() of last valid sentence
    uint32_t _unixTime;       // Unix epoch seconds (if available)

    // -------------------------------------------------------
    // Distance to home  (float)
    // -------------------------------------------------------
    float    _distHomeM;

    // -------------------------------------------------------
    // NMEA sentence accumulation buffer
    // Sized by NMEA_BUF_LEN — largest member, placed last
    // -------------------------------------------------------
    char     _nmeaBuf[NMEA_BUF_LEN];

    // -------------------------------------------------------
    // Private methods — core state
    // -------------------------------------------------------
    void resetState();

    // -------------------------------------------------------
    // Private methods — NMEA parsing
    // -------------------------------------------------------
    void processByte(char c);
    void processSentence(const char *s);
    void parseGGA(const char *s);
    void parseRMC(const char *s);

    // Static: no hidden 'this' pointer needed for pure conversion helpers
    static bool     parseLatLon(const char *field, char hemi, int32_t &out1e7);
    static int32_t  parseInt(const char *s);
    static uint32_t parseUInt(const char *s);
    static uint16_t parseUInt16(const char *s);
    static float    parseFloat(const char *s);

    // -------------------------------------------------------
    // Private methods — UBX
    // -------------------------------------------------------

    // Write a uint32 key into buf at offset (little-endian).
    // Returns new offset after writing.
    static uint8_t writeKey(uint8_t *buf, uint8_t offset, uint32_t key);

    // Write a uint8 value into buf at offset.
    // Returns new offset after writing.
    static uint8_t writeU8(uint8_t *buf, uint8_t offset, uint8_t val);

    // Write a uint16 value into buf at offset (little-endian).
    // Returns new offset after writing.
    static uint8_t writeU16(uint8_t *buf, uint8_t offset, uint16_t val);

    // Compute UBX Fletcher checksum over buf[start..end).
    static void ubxChecksum(const uint8_t *buf,
                            size_t         start,
                            size_t         end,
                            uint8_t       &ckA,
                            uint8_t       &ckB);

    // Assemble and transmit a complete UBX CFG-VALSET frame.
    // Replaces the original sendVALSET() — identical purpose,
    // cleaner name matching UBX terminology.
    void sendVALSET(const uint8_t *payload, uint16_t payloadLen);

    // Block (with timeout) waiting for UBX ACK-ACK or ACK-NAK.
    // Non-blocking internally — exits on first byte match or timeout.
    // Returns true if ACK-ACK received within UBX_ACK_TIMEOUT_MS.
    bool waitForUBXAck();
};