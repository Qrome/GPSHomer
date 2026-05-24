&#x20; GGGG     PPPP     SSSS        H   H     oooo     m   m     eeee     rrrr

&#x20;G         P   P   S            H   H    o    o    mm mm     e        r   r

&#x20;G  GG     PPPP     SSS         HHHHH    o    o    m m m     eeee     rrrr

&#x20;G   G     P            S       H   H    o    o    m   m     e        r  r

&#x20; GGGG     P        SSSS        H   H     oooo     m   m     eeee     r   r



\# GPSHomer — RP2040 GPS Home Radar Display



\## 🛰️ Overview

GPSHomer is a GPS‑based “Home Direction Radar” built around:



\- Waveshare RP2040 Zero

\- 1.28" Round GC9A01 TFT Display (240×240, SPI)

\- NMEA GPS module (UART)



The display shows:



\- Aircraft symbol (centered)

\- Home direction marker (white circle with black “H”)

\- Ground speed

\- Satellite count

\- Heading‑up radar rotation

\- Distance clamping (configurable)



On boot, the system autodetects the GPS baud rate and briefly displays it.



\---



\## 🧩 Hardware



\### 1. Waveshare RP2040 Zero

\- Dual‑core RP2040

\- 3.3V logic

\- SPI + UART support

\- Compact form factor



\### 2. 1.28" Round GC9A01 TFT Display

\- 240×240 resolution

\- 4‑wire SPI

\- GC9A01 driver

\- Pins: VCC, GND, SCL, SDA, DC, CS, RST

\- No BL pin (backlight always on)



\### 3. GPS Module

\- Any NMEA‑compatible GPS module

\- Baud: 9600 / 38400 / 57600 / 115200

\- Outputs GGA + RMC

\- Provides lat/lon, speed, course, fix type, satellite count



\---



\## 🔌 Wiring



\### RP2040 Zero → GC9A01 Display



| Display Pin | RP2040 Pin | Description   |

|-------------|------------|---------------|

| VCC         | 3.3V       | Power         |

| GND         | GND        | Ground        |

| SCL         | GP18       | SPI Clock     |

| SDA         | GP19       | SPI MOSI      |

| DC          | GP16       | Data/Command  |

| CS          | GP17       | Chip Select   |

| RST         | GP20       | Reset         |



> No BL pin — backlight is internally tied to VCC.



\### RP2040 Zero → GPS Module



| GPS Pin | RP2040 Pin | Description              |

|---------|------------|--------------------------|

| TX      | GP1        | GPS → RP2040 (RX)        |

| RX      | GP0        | RP2040 → GPS (TX)        |

| VCC     | 3.3V/5V    | Power                    |

| GND     | GND        | Ground                   |



\---



\## ⚙️ TFT\_eSPI Configuration (in config.h)





