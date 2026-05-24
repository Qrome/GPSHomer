#pragma once

// -----------------------------
// Units
// -----------------------------
#define UNITS_METRIC     0
#define UNITS_IMPERIAL   1

#define OSD_UNITS        UNITS_IMPERIAL

#define RC_PWM_PIN 7
#define RC_THRESHOLD 1700

// -----------------------------
// Hardware Pins
// -----------------------------
#define PIN_GPS_TX      0   // UART0 TX
#define PIN_GPS_RX      1   // UART0 RX

#define PIN_LCD_CS      4
#define PIN_LCD_DC      5
#define PIN_LCD_RST     6
#define PIN_LCD_SCK     2
#define PIN_LCD_MOSI    3
#define TOUCH_CS        -1   // <— disables warning
#define TFT_BL   -1
#define TOUCH_CS -1

// -----------------------------
// GPS Settings
// -----------------------------
#define GPS_DEFAULT_BAUD   9600
const uint32_t baudList[] = {9600, 38400, 57600, 115200, 9600};

// -----------------------------
// Display Settings
// -----------------------------
#define SCREEN_WIDTH       240
#define SCREEN_HEIGHT      240
#define SCREEN_CENTER_X    120
#define SCREEN_CENTER_Y    120
#define SCREEN_RADIUS      120

// Colors (TFT_eSPI style)
#define COLOR_BACKGROUND   TFT_BLACK
#define COLOR_AIRCRAFT     TFT_SKYBLUE
#define COLOR_HOME_CIRCLE  TFT_WHITE
#define COLOR_HOME_TEXT    TFT_BLACK
#define COLOR_TEXT         TFT_WHITE
#define COLOR_RING         0x03E0  // very dark lime green (RGB565) Darker: 0x02C0


// -----------------------------
// Radar Behavior
// -----------------------------
#define HOME_MARKER_RADIUS       8      // pixels
#define OSD_SHOW_RINGS           1      // 1 = show rings, 0 = hide rings


// -----------------------------
// Boot / UI Timing
// -----------------------------
#define BOOT_BAUD_DISPLAY_MS   3000
#define RADAR_DRAW_FPS         20      // target FPS
