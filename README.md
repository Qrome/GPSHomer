&#x20; GGGG     PPPP     SSSS        H   H     oooo     m   m     eeee     rrrr

&#x20;G         P   P   S            H   H    o    o    mm mm     e        r   r

&#x20;G  GG     PPPP     SSS         HHHHH    o    o    m m m     eeee     rrrr

&#x20;G   G     P            S       H   H    o    o    m   m     e        r  r

&#x20; GGGG     P        SSSS        H   H     oooo     m   m     eeee     r   r



\# 📘 \*\*GPSHomer — RP2040 GPS Home Radar Display\*\*



\## 🛰️ Overview

This project implements a \*\*GPS‑based “Home Direction Radar”\*\* using:



\- \*\*Waveshare RP2040 Zero\*\*

\- \*\*1.28" Round GC9A01 TFT Display (240×240)\*\*

\- \*\*Standard NMEA GPS module (UART)\*\*

\- \*\*Dual‑core architecture\*\*  

&#x20; - Core 0 → Display rendering  

&#x20; - Core 1 → GPS parsing  



The display shows:



\- Aircraft symbol (centered)

\- Home direction marker (white circle with black “H”)

\- Ground speed (m/s or mph)

\- Satellite count

\- Heading‑up radar rotation

\- Distance clamping (configurable)



On boot, the system autodetects the GPS baud rate and displays it for a few seconds.



\---



\## 🧩 Hardware Used



\### \*\*1. Waveshare RP2040 Zero\*\*

A compact RP2040 board with:



\- Dual‑core Cortex‑M0+

\- Hardware UARTs

\- SPI support

\- 3.3V logic (compatible with GC9A01)



\### \*\*2. 1.28" Round GC9A01 TFT Display\*\*

\- Resolution: \*\*240×240\*\*

\- Interface: \*\*4‑wire SPI\*\*

\- Driver: \*\*GC9A01\*\*

\- Pins:  

&#x20; - \*\*VCC\*\*  

&#x20; - \*\*GND\*\*  

&#x20; - \*\*SCL\*\* (SPI Clock)  

&#x20; - \*\*SDA\*\* (SPI MOSI)  

&#x20; - \*\*DC\*\*  

&#x20; - \*\*CS\*\*  

&#x20; - \*\*RST\*\*  

\- \*\*No backlight pin\*\* (BL is internally tied to VCC)



\### \*\*3. GPS Module\*\*

Any NMEA‑compatible GPS module with UART output:



\- 9600 / 38400 / 57600 / 115200 baud  

\- Outputs GGA + RMC sentences  

\- Provides:  

&#x20; - Latitude / Longitude  

&#x20; - Ground speed  

&#x20; - Course  

&#x20; - Fix type  

&#x20; - Satellite count  



\---



\## 🔌 Wiring Diagram



\### \*\*RP2040 Zero → GC9A01 Display\*\*



| Display Pin | RP2040 Pin | Description |

|-------------|------------|-------------|

| \*\*VCC\*\* | 3.3V | Power |

| \*\*GND\*\* | GND | Ground |

| \*\*SCL\*\* | \*\*GP18\*\* | SPI Clock |

| \*\*SDA\*\* | \*\*GP19\*\* | SPI MOSI |

| \*\*DC\*\* | \*\*GP16\*\* | Data/Command |

| \*\*CS\*\* | \*\*GP17\*\* | Chip Select |

| \*\*RST\*\* | \*\*GP20\*\* | Reset |



> Note: Your display has \*\*no BL pin\*\*, so backlight control is disabled.



\---



\### \*\*RP2040 Zero → GPS Module\*\*



| GPS Pin | RP2040 Pin | Description |

|---------|------------|-------------|

| \*\*TX\*\* | \*\*GP1\*\* | GPS → RP2040 (RX) |

| \*\*RX\*\* | \*\*GP0\*\* | RP2040 → GPS (TX) |

| \*\*VCC\*\* | 3.3V or 5V | Power (depends on module) |

| \*\*GND\*\* | GND | Ground |



\---



\## ⚙️ TFT\_eSPI Configuration (in `config.h`)



This project uses \*\*TFT\_eSPI\*\* with overrides defined directly in `config.h`:



```cpp

\#define USER\_SETUP\_LOADED

\#define GC9A01\_DRIVER



\#define TFT\_MOSI 19

\#define TFT\_SCLK 18

\#define TFT\_CS   17

\#define TFT\_DC   16

\#define TFT\_RST  20



// No backlight pin on this display

\#define TFT\_BL   -1



// No touch panel

\#define TOUCH\_CS -1



\#define SPI\_FREQUENCY 60000000

```



This eliminates the need to modify `User\_Setup.h`.



\---



\## 🧭 Software Architecture



\### \*\*Dual‑Core Execution\*\*

\- \*\*Core 0 (loop)\*\*  

&#x20; - Radar rendering  

&#x20; - Display updates  

&#x20; - UI elements  



\- \*\*Core 1 (loop1)\*\*  

&#x20; - GPS UART parsing  

&#x20; - NMEA decoding  

&#x20; - Updating shared state  



\### \*\*GPS Class\*\*

Encapsulates:



\- Autodetecting baud rate  

\- Parsing GGA + RMC sentences  

\- Extracting:  

&#x20; - Latitude / Longitude  

&#x20; - Speed (cm/s)  

&#x20; - Course (deg × 10)  

&#x20; - Fix type  

&#x20; - Satellite count  



\### \*\*Display Class\*\*

Handles:



\- Background clearing  

\- Aircraft symbol  

\- Home marker (white circle + black “H”)  

\- Speed + satellite text  

\- Heading‑up rotation  

\- Distance clamping  



\---



\## 🧮 Radar Math Summary



\- Convert lat/lon to meters  

\- Rotate world around aircraft heading  

\- Clamp distance to screen radius  

\- Draw home marker last (always on top)



\---



\## 🛠️ Configuration Options (`config.h`)



```cpp

\#define RADAR\_CLAMP\_DISTANCE\_M   1000

\#define HOME\_MARKER\_RADIUS       8



\#define UNITS\_METRIC     0

\#define UNITS\_IMPERIAL   1

\#define OSD\_UNITS        UNITS\_METRIC



\#define BOOT\_BAUD\_DISPLAY\_MS   2500

\#define RADAR\_DRAW\_FPS         20

```



\---



\## 📦 File Structure



```

/src

&#x20; ├── main.ino

&#x20; ├── Display.h

&#x20; ├── Display.cpp

&#x20; ├── GPS.h

&#x20; ├── GPS.cpp

&#x20; └── config.h

```



\---



\## 🧪 Testing Notes



\- On boot, the detected GPS baud rate is shown for 2.5 seconds  

\- Home position is set automatically on first valid 2D/3D fix  

\- Satellite count appears next to ground speed  

\- Home marker always draws on top of all other elements  



\---



\## 📜 License

MIT License (or your preferred license)



