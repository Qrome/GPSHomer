```
  GGGG     PPPP     SSSS        H   H     oooo     m   m     eeee     rrrr
 G         P   P   S            H   H    o    o    mm mm     e        r   r
 G  GG     PPPP     SSS         HHHHH    o    o    m m m     eeee     rrrr
 G   G     P            S       H   H    o    o    m   m     e        r  r
  GGGG     P        SSSS        H   H     oooo     m   m     eeee     r   r

      G   P   S       H   o   m   e   r
```

# GPSHomer — RP2040 GPS Home Radar Display

![GPSHomer - Ground Radar for tracking home](images/GPSHomer_01.png) 
![GPSHomer - Ground Radar for tracking home](images/GPSHomer_02.png)

## Overview
GPSHomer is a GPS‑based “Home Direction Radar” built around:

- Waveshare RP2040 Zero  
- 1.28" Round GC9A01 TFT Display (240×240, SPI)  
- NMEA GPS module (UART)

The display shows:

- Aircraft symbol (centered)
- Home direction marker (white circle with black “H”)
- **Distance from home printed next to the H marker at the top**
- (H) symbol represents home in relation to the aircraft
- Ground speed
- Satellite count
- Heading‑up radar rotation
- North indicating arrow on outer ring
- **Smooth dynamic zoom based on distance**
- **Optional distance rings (configurable)**
- Distance clamping (auto‑scaled)
- Quick double-tap to reset Home
- Single-tap to display Flight Summary
- Summary View of Flight  

[![GPS Homer Instrument Panel - Working Radar Style Display](https://img.youtube.com/vi/dhLf5rBQKtM/0.jpg)](https://www.youtube.com/watch?v=dhLf5rBQKtM)

---
## Software & Environment Setup

This project is built for the Raspberry Pi Pico / RP2040 / RP2350 architecture using the Arduino IDE. Follow the steps below to configure your development environment.

### 1. Install the Board Support Package (BSP)
You must use the Arduino Pico core maintained by Earle F. Philhower, III. The default Arduino core will not support all features or pin mappings.

1. Open the Arduino IDE.
2. Navigate to **File** -> **Preferences**.
3. Locate the **Additional Boards Manager URLs** field and paste the following URL:
   ```text
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```
4. Click **OK**.
5. Go to **Tools** -> **Board** -> **Boards Manager...**
6. Search for `Pico` or `Philhower` and install **Raspberry Pi Pico/RP2040/RP2350** by *Earle F. Philhower, III*.
7. Once installed, go to **Tools** -> **Board** -> **Raspberry Pi Pico/RP2040** and select **Waveshare RP2040 Zero**.

### 2. Install Required Libraries
Open the Arduino Library Manager (**Tools** -> **Manage Libraries...** or press `Ctrl+Shift+I` / `Cmd+Shift+I`) to search for and install:

* **Adafruit NeoPixel** (by Adafruit)
* **TFT_eSPI** (by Bodmer)

> 💡 **Note:** When installing the libraries, if the IDE prompts you to install missing dependencies (such as `Adafruit BusIO`), choose **Install All**.
---

## Hardware

![Qrome's GPSHomer Printed Circuit Board)](images/GPSHomer_PCB.png)

### GPSHomer Printed Circuit Board (PCB) by Qrome: [Link Soon]
- Powered from 5V PWM Connection to RC Rx  
- Display Plug and Play    

### Waveshare RP2040 Zero: https://amzn.to/4v0oCKq
- Dual‑core RP2040  
- 3.3V logic  
- SPI + UART support  

### 1.28" Round GC9A01 TFT 240x240 LCD Display: https://amzn.to/4fnbzhy
- 240×240 resolution  
- 7‑wire SPI  
- GC9A01 driver  
- Pins: VCC, GND, SCL, SDA, DC, CS, RST  
- No BL pin (backlight always on)

### GPS Module: https://amzn.to/4weRRKt
- Any NMEA‑compatible GPS module  
- Baud: 9600 / 38400 / 57600 / 115200  
- Outputs GGA + RMC  
- Provides lat/lon, speed, course, fix type, satellite count  

![HGLRC M100 GPS Module](images/M100_GPS_Rx.png)
---

## Wiring

### RC RX Channel → RP2040 Zero

| PWM Pin | RP2040 Pin | Description              |
|---------|------------|--------------------------|
| Signal  | GP27       | RC RX Ch6 → RP2040 (27)  |
| RX      | 5V         | 5V RX → RP2040 (5V)      |
| GND     | GND        | Ground                   |

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

### RP2040 Zero → GPS Module (4 wire)

| GPS Pin | RP2040 Pin | Description              |
|---------|------------|--------------------------|
| TX      | GP1        | GPS → RP2040 (RX)        |
| RX      | GP0        | RP2040 → GPS (TX)        |
| VCC     | 3.3V       | Power                    |
| GND     | GND        | Ground                   |

---

## TFT_eSPI Configuration (User_Setup.h)

Important details on the TFT_eSPI library and User_Setup.h -- this file 
is located next to this README.md in the the GPSHomer project.   Please 
copy it to your TFT_eSPI lbirary path -- details in comments:
```
// ============================================================================
//  GPSHomer - Custom TFT_eSPI User Setup (User_Setup.h)
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
```
---

## Software Architecture

### RP2040 Core 0
- Display rendering  
- Radar drawing  
- UI elements  

### RP2040 Core 1
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
- **Draws distance from home at the top of display**
- Shows ground speed + satellite count  
- Heading‑up rotation  
- **Smooth dynamic zoom**
- **Optional distance rings**
- Flight Summary

---

## Radar Logic

- Convert lat/lon to meters  
- Compute vector from aircraft to home  
- Rotate world by negative heading  
- Compute distance  
- **Apply smooth dynamic zoom (logarithmic scaling)**  
- Clamp distance to radar radius  
- **Draw distance rings**  
- Draw home marker last  
- **Print distance from home near the top**  

---

## Configuration (config.h)

```cpp
#define OSD_UNITS        UNITS_IMPERIAL  // UNITS_METRIC or UNITS_IMPERIAL
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
│   ├── LED.cpp
│   ├── LED.h
│   └── config.h
└── README.md
```

---

## Behavior Notes

- Home position is captured on first valid fix  
- Home position is reset by double tapping RC PWM signal low to high  
- Summary view is triggered with RC RX signal set to high  
- GPS baud autodetected at startup  
- Satellite count and ground speed shown on OSD  
- **Radar zooms smoothly based on distance**  
- **Distance rings scale dynamically**  
- **Distance printed next to home marker**  
- Home marker always drawn on top  

![Summary View with RC RX PWM switch to high](images/GPSHomer_Summary.jpg)
---

## License

MIT License (or your preferred license)
