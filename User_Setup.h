// ============================================================================
//  GPSHomer - Custom TFT_eSPI User Setup
// ============================================================================
//
//  IMPORTANT:
//  This file is a *project-local override* of the standard TFT_eSPI
//  configuration. It REPLACES the default User_Setup.h found in:
//
//      Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//
//  Why this file exists:
//  ----------------------
//  GPSHomer uses a GC9A01 round display and the RP2040 Zero's PIO-driven SPI.
//  The stock TFT_eSPI configuration does NOT support this hardware layout.
//  Therefore, this project provides its own User_Setup.h with the correct
//  driver, pins, and SPI mode.
//
//  Where to place this file:
//  -------------------------
//  Replace the global file:
//
//      Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//
//  Summary:
//  --------
//  * This file *must* override the default TFT_eSPI configuration.
//  * It ensures GPSHomer uses the correct GC9A01 driver + RP2040 PIO SPI.
//  * It must replace the global one.
//  * No other changes are needed once this file is in place.
//
// ============================================================================
//                            USER DEFINED SETTINGS
// ============================================================================

// Disable library warnings if needed
// #define DISABLE_ALL_LIBRARY_WARNINGS

// --- Driver & Display ---
#define USER_SETUP_INFO "User_Setup"
#define GC9A01_DRIVER          // Required for round 240x240 GC9A01 display
#define TFT_WIDTH  240
#define TFT_HEIGHT 240
#define TFT_RGB_ORDER TFT_BGR  // Correct color order for GC9A01
#define SMOOTH_FONT            // Enable anti-aliased fonts
#define RP2040_PIO_SPI         // Use RP2040 PIO hardware SPI (critical)

// --- Pins (RP2040 Zero - GPSHomer) ---
#define TFT_MOSI 3
#define TFT_SCLK 2
#define TFT_CS   4
#define TFT_DC   5
#define TFT_RST  6
#define TFT_BL   -1            // No backlight pin (always on)

// --- Fonts to load ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

// --- SPI Frequency ---
#define SPI_FREQUENCY 60000000 // 60 MHz works reliably on RP2040 PIO SPI
