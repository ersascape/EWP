# Ersa Watch OS (EWP)

An open-source, minimalist smartwatch firmware for the Seeed Studio XIAO ESP32-C3 and 1.54" monochrome E-Paper Display (GxEPD2 / SSD1681), inspired by the iconic **Pebble Text Watch** aesthetic.

---

## Visual Showcase

<p align="center">
  <img src="docs/images/text_watchface.png" width="190" alt="Text Watchface" />
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/images/app_drawer.png" width="190" alt="App Drawer" />
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/images/caldav_tasks.png" width="190" alt="CalDAV Tasks" />
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/images/caldav_agenda.png" width="190" alt="CalDAV Agenda" />
</p>

<p align="center">
  <em>Text Watchface &bull; App Drawer &bull; CalDAV Tasks &bull; CalDAV Agenda</em>
</p>

---

## Key Features

- **Pebble Text Watchface**:
  - Full-bleed black background with pure white typography.
  - Natural time spelled out in English words using Xiaomi's official **MiSans Latin Bold & Light** fonts.
  - Lowercase natural date with ordinal suffix (e.g. `thursday` / `november 12th, 2020` or `monday` / `september 28th, 2026`).
  - Distraction-free: battery or sync alerts only appear when active or low.
- **Fast, Flicker-Free Partial Refresh**:
  - Panel controller kept energized during user interaction for **~450ms raw partial updates** with zero black/white blinking.
  - Smooth App Drawer scrolling and instant minute clock updates.
  - Automatic low-power sleep after 8 seconds of inactivity.
- **Nextcloud & CalDAV Cloud Sync**:
  - **CalDAV Agenda**: Filters events specifically for **today**, supporting standard events and recurring rules (`FREQ=DAILY`, `FREQ=WEEKLY`).
  - **CalDAV Tasks**: To-do checklist with instant on-watch toggling (`[ ]` $\leftrightarrow$ `[x]`), prioritizing active/open tasks.
- **NTP Clock Calibration**:
  - High-precision SNTP synchronization adjusting the onboard **DS3231 RTC** to exact local time with configurable timezone offset.
- **On-Demand Captive Portal Hotspot**:
  - Launch `ErsaWatch-Config` AP from the watch drawer to configure Wi-Fi credentials, CalDAV server, calendar presets (`murena-team`, `personal`, `tasks`), timezone, and time format (12h / 24h).
- **Battery Sensing**:
  - Hardware ADC battery monitoring on GPIO2 (A0) with multi-sample averaging and lithium discharge curve mapping.

---

## Hardware Specifications

| Component | Specification | Details |
| :--- | :--- | :--- |
| **MCU** | Seeed Studio XIAO ESP32-C3 | RISC-V 160 MHz, 320 KB SRAM, 4 MB Flash, Wi-Fi & BLE |
| **Display** | 1.54" E-Paper Display | 200×200 Monochrome (GxEPD2 / SSD1681), partial refresh capable |
| **RTC** | Maxim DS3231 | High-precision I2C RTC (address `0x68`) with backup cell |
| **Buttons** | Dual tactile switches | Upper `B1` = GPIO4 (D2), Lower `B2` = GPIO3 (D1) |
| **Battery** | LiPo sensing | GPIO2 (A0) via voltage divider |

---

## 2-Button Navigation System

```
                  ┌─────────────────┐
                  │ B1 (Upper GPIO4)│ ──> Click: SCROLL (Down / Next)
                  │                 │ ──> Hold:  MENU / EXIT (App Drawer / Clock)
  [Ersa Watch]    ├─────────────────┤
                  │ B2 (Lower GPIO3)│ ──> Click: OK / ACTION (Select / Toggle / Sync)
                  │                 │ ──> Hold:  QUICK SYNC (CalDAV & NTP)
                  └─────────────────┘
```

| Screen | B1 (Click) | B2 (Click) | Hold B1 | Hold B2 |
| :--- | :--- | :--- | :--- | :--- |
| **Watchface** | Open App Drawer | Quick CalDAV Sync | Open App Drawer | Quick CalDAV Sync |
| **App Drawer** | Scroll selection down | Launch selected app | Return to Clock | — |
| **Calendar** | Advance month (`+1 mo`) | Reset to current month | Return to Drawer | — |
| **Agenda** | Scroll event cards | Sync CalDAV events | Return to Drawer | Sync CalDAV |
| **Tasks** | Scroll checklist items | Toggle task (`[x]`) | Return to Drawer | Sync CalDAV |
| **Hotspot** | Refresh status | Start / Stop AP | Return to Drawer | Sync NTP |
| **Status** | Refresh sensors | Sync NTP Time | Return to Drawer | — |

---

## Project Structure

```
Ersa-W1/
├── include/
│   ├── board_pins.h           # Hardware pinout definitions
│   └── fonts/
│       └── misans_fonts.h     # MiSans Latin Bold & Light GFX fonts
├── src/
│   ├── apps/
│   │   ├── app_agenda.*       # CalDAV events card viewer
│   │   ├── app_calendar.*     # Interactive monthly calendar
│   │   ├── app_drawer.*       # App launcher with white capsule cursor
│   │   ├── app_portal.*       # Wi-Fi captive configuration portal
│   │   ├── app_status.*       # Hardware diagnostics & battery stats
│   │   └── app_todo.*         # CalDAV to-do checklist
│   ├── core/
│   │   ├── battery.*          # ADC voltage & battery curve calculations
│   │   ├── buttons.*          # OneButton debounce & event dispatcher
│   │   ├── debug_log.*        # USB Serial logging & boot crash records
│   │   ├── net_sync.*         # NTP time & CalDAV iCalendar parser
│   │   ├── watch_clock.*      # DS3231 RTC driver & time caching
│   │   └── watch_config.*     # NVS non-volatile settings storage
│   ├── ui/
│   │   ├── watch_icons.*      # Monochrome bitmaps
│   │   └── watch_ui.*         # Display controller, refresh scheduler, page routing
│   ├── watchfaces/
│   │   └── watchface_clock.*  # Pebble Text Watch watchface
│   └── main.cpp               # Setup & cooperative event loop
├── platformio.ini             # PlatformIO build configuration
└── scripts/
    └── pio.sh                 # Self-contained PlatformIO CLI bootstrap
```

---

## Build & Flash

This project uses a project-local PlatformIO toolchain managed by `scripts/pio.sh`.

### 1. Compile Firmware
```bash
bash scripts/pio.sh run
```

### 2. Upload to Watch
Connect the XIAO ESP32-C3 via USB-C and run:
```bash
bash scripts/pio.sh run --target upload
```

### 3. Serial Monitor
```bash
bash scripts/pio.sh device monitor
```

---

## Initial Setup via Wi-Fi Portal

1. On the watch, press **`B1`** to enter the App Drawer, scroll to **`hotspot`**, and press **`B2`** to start the AP.
2. Connect your phone or computer to the Wi-Fi network:
   - **SSID**: `ErsaWatch-Config`
   - **Password**: `12345678`
3. A captive portal page will automatically open (or navigate to `http://192.168.4.1`).
4. Enter your home Wi-Fi credentials, Nextcloud/Murena CalDAV URL, username, and app password.
5. Select **Save & Sync NTP Time**. The watch will connect, calibrate the DS3231 RTC, download your daily events and tasks, and turn off Wi-Fi to conserve power.

---

## License

MIT License. Designed and crafted for the open-source hardware community.
