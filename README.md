```
  GGGG     PPPP     SSSS        H   H     oooo     m   m     eeee     rrrr
 G         P   P   S            H   H    o    o    mm mm     e        r   r
 G  GG     PPPP     SSS         HHHHH    o    o    m m m     eeee     rrrr
 G   G     P            S       H   H    o    o    m   m     e        r  r
  GGGG     P        SSSS        H   H     oooo     m   m     eeee     r   r

      G   P   S       H   o   m   e   r
```

# GPSHomer — RP2040 GPS Home Radar Display

## Overview
GPSHomer is a GPS‑based “Home Direction Radar” built around:

- Waveshare RP2040 Zero  
- 1.28" Round GC9A01 TFT Display (240×240, SPI)  
- NMEA GPS module (UART)

The display shows:

- Aircraft symbol (centered)
- Home direction marker (white circle with black “H”)
- Ground speed
- Satellite count
- Heading‑up radar rotation
- Distance clamping

On boot, the system autodetects the GPS baud rate and displays it briefly.

---

## Hardware

### Waveshare RP2040 Zero
- Dual‑core RP2040  
- 3.3V logic  
- SPI + UART support  

### 1.28" Round GC9A01 TFT Display
- 240×240 resolution  
- 4‑wire SPI  
- GC9A01 driver  
- Pins: VCC, GND, SCL, SDA, DC, CS, RST  
- No BL pin (backlight always on)

### GPS Module
- Any NMEA‑compatible GPS module  
- Baud: 9600 / 38400 / 57600 / 115200  
- Outputs GGA + RMC  
- Provides lat/lon, speed, course, fix type, satellite count  

---

## Wiring

### RP2040 Zero → GC9A01 Display

| Display Pin | RP2040 Pin | Description   |
|-------------|------------|---------------|
| VCC         | 3.3V       | Power         |
| GND         | GND        | Ground        |
| SCL         | GP2        | SPI Clock     |
| SDA         | GP3        | SPI MOSI      |
| DC          | GP5        | Data/Command  |
| CS          | GP4        | Chip Select   |
| RST         | GP6        | Reset         |

Note: This display has no BL pin.

### RP2040 Zero → GPS Module

| GPS Pin | RP2040 Pin | Description              |
|---------|------------|--------------------------|
| TX      | GP1        | GPS → RP2040 (RX)        |
| RX      | GP0        | RP2040 → GPS (TX)        |
| VCC     | 3.3V/5V    | Power                    |
| GND     | GND        | Ground                   |

---

## TFT_eSPI Configuration (config.h)

```cpp
#define USER_SETUP_LOADED
#define GC9A01_DRIVER

#define PIN_LCD_CS      4
#define PIN_LCD_DC      5
#define PIN_LCD_RST     6
#define PIN_LCD_SCK     2
#define PIN_LCD_MOSI    3
#define TOUCH_CS        -1   // <— disables warning

#define TFT_BL   -1
#define TOUCH_CS -1

#define SPI_FREQUENCY       60000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY 2500000
```

---

## Software Architecture

### Core 0
- Display rendering  
- Radar drawing  
- UI elements  

### Core 1
- GPS UART reading  
- NMEA parsing  
- Updating shared navigation state  

### GPS Module
- Autodetects baud rate  
- Parses GGA + RMC  
- Tracks: lat/lon, speed, course, fix type, satellite count  

### Display Module
- Clears and redraws radar  
- Draws aircraft symbol  
- Draws home marker  
- Shows speed + satellite count  
- Heading‑up rotation  
- Distance clamping  

---

## Radar Logic

- Convert lat/lon to meters  
- Compute vector from aircraft to home  
- Rotate world by negative heading  
- Clamp distance to radar radius  
- Draw home marker last  

---

## Configuration (config.h)

```cpp
#define RADAR_CLAMP_DISTANCE_M   1000
#define HOME_MARKER_RADIUS       8

#define UNITS_METRIC     0
#define UNITS_IMPERIAL   1
#define OSD_UNITS        UNITS_METRIC

#define BOOT_BAUD_DISPLAY_MS   2500
#define RADAR_DRAW_FPS         20
```

---

## Project Structure

```
/
├── src
│   ├── main.ino
│   ├── Display.h
│   ├── Display.cpp
│   ├── GPS.h
│   ├── GPS.cpp
│   └── config.h
│   ├── LED.cpp
│   └── LED.h
└── README.md
```

---

## Behavior Notes

- Home position is captured on first valid fix  
- GPS baud autodetected at startup  
- Satellite count and ground speed shown on OSD  
- Home marker always drawn on top  

---

## License

MIT License (or your preferred license)
