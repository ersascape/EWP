# Adding Board Support to Ersa OS

This guide walks you through adding support for a new hardware target or development board to the **Ersa Wearable Platform (EWP)**.

---

## 1. Architectural Overview

Ersa OS is built with strict boundary layering to keep applications, system services, and UI components completely decoupled from underlying hardware:

```text
┌───────────────────────────────────────────────┐
│ Applications & Watchfaces                     │
│ (Only depend on ersa::app, ersa::ui::Canvas)  │
└───────────────────────────────────────────────┘
                       │
┌───────────────────────────────────────────────┐
│ Ersa System Services & Application Manager    │
│ (TimeService, PowerManager, NetworkManager)   │
└───────────────────────────────────────────────┘
                       │
┌───────────────────────────────────────────────┐
│ Ersa Platform HAL Interfaces                  │
│ (IDisplay, IRtc, IBattery, IInput, INetwork)  │
└───────────────────────────────────────────────┘
                       │
┌───────────────────────────────────────────────┐
│ Board Support Package (BSP)                   │
│ (Subclasses ersa::board::Board)               │
└───────────────────────────────────────────────┘
```

When porting Ersa OS to a new board:
- You **do not** modify applications or services.
- You **implement a Board class** that describes the board's capabilities, pins, and display geometry, and binds concrete HAL driver instances.
- If your board uses existing supported components (e.g. ESP32-C3 + DS3231 + GxEPD2 e-paper), you simply instantiate the existing HAL drivers with your board's pin definitions.
- If your board introduces new peripherals (e.g. an ST7789 LCD or an internal nRF52 RTC), you implement the corresponding HAL interface.

---

## 2. Directory Layout for a BSP

Each board target lives in its own subdirectory under `src/bsp/`:

```
src/bsp/
├── ampere_t1e/                   # Reference BSP (Ampere Works T1E)
│   ├── board_ampere_t1e.h
│   └── board_ampere_t1e.cpp
└── <your_board_name>/            # Your new board BSP
    ├── board_<your_board>.h
    └── board_<your_board>.cpp
```

---

## 3. Step-by-Step Implementation

### Step 1: Subclass `ersa::board::Board`

Create `src/bsp/<your_board>/board_<your_board>.h`:

```cpp
#pragma once

#if defined(ARDUINO)

#include "ersa/board/board.h"
#include "ersa/board/board_config.h"
#include "hal/esp32/esp32_display.h"
#include "hal/esp32/esp32_rtc.h"
#include "hal/esp32/esp32_battery.h"
#include "hal/esp32/esp32_input.h"

namespace ersa {
namespace board {

class BoardMyWatch : public Board {
public:
    BoardMyWatch();
    ~BoardMyWatch() override = default;

    Result<void> init() override;
    const char* getName() const override { return "My Custom Watch"; }
    const BoardConfig& getConfig() const override { return config_; }

    hal::IDisplay& getDisplay() override { return display_; }
    hal::IRtc& getRtc() override { return rtc_; }
    hal::IBattery& getBattery() override { return battery_; }
    hal::IInput& getInput() override { return input_; }

    uint32_t getUptimeMs() const override;
    void delayMs(uint32_t ms) override;

    static BoardMyWatch& instance();

private:
    BoardConfig config_;
    hal::Esp32Display display_;
    hal::Esp32Rtc rtc_;
    hal::Esp32Battery battery_;
    hal::Esp32Input input_;
};

} // namespace board
} // namespace ersa

#endif // ARDUINO
```

---

### Step 2: Configure Pins, Capabilities & Peripherals

In `src/bsp/<your_board>/board_<your_board>.cpp`:

```cpp
#if defined(ARDUINO)

#include "bsp/<your_board>/board_<your_board>.h"
#include <Arduino.h>

namespace ersa {
namespace board {

BoardMyWatch& BoardMyWatch::instance() {
    static BoardMyWatch s_board;
    return s_board;
}

BoardMyWatch::BoardMyWatch()
    : display_(/* CS */ 5, /* DC */ 20, /* RST */ 21, /* BUSY */ 9,
               /* SCK */ 8, /* MISO */ -1, /* MOSI */ 10),
      rtc_(/* SDA */ 6, /* SCL */ 7),
      battery_(/* ADC Pin */ 2),
      input_(/* Button1 */ 4, /* Button2 */ 3) {

    // 1. Board Name & Capabilities
    config_.name = "My Custom Watch";
    config_.capabilities.wifi = true;
    config_.capabilities.bluetooth = true;
    config_.capabilities.rtc = true;
    config_.capabilities.batteryGauge = true;
    config_.capabilities.buttons = true;
    config_.capabilities.haptics = false;
    config_.capabilities.touch = false;

    // 2. Display Characteristics
    config_.display.width = 200;
    config_.display.height = 200;
    config_.display.partialRefresh = true;
    config_.display.isEpaper = true;

    // 3. Pin Map Record (used by diagnostics and settings)
    config_.pins.sda = 6;
    config_.pins.scl = 7;
    config_.pins.sck = 8;
    config_.pins.mosi = 10;
    config_.pins.miso = -1;
    config_.pins.epdCs = 5;
    config_.pins.epdDc = 20;
    config_.pins.epdRst = 21;
    config_.pins.epdBusy = 9;
    config_.pins.button1 = 4;
    config_.pins.button2 = 3;
    config_.pins.batteryAdc = 2;
}

Result<void> BoardMyWatch::init() {
    Board::setCurrent(this);

    // Initialize hardware drivers
    rtc_.init();
    display_.init();
    battery_.init();
    input_.init();

    return Result<void>();
}

uint32_t BoardMyWatch::getUptimeMs() const {
    return millis();
}

void BoardMyWatch::delayMs(uint32_t ms) {
    delay(ms);
}

} // namespace board
} // namespace ersa

#endif // ARDUINO
```

---

## 4. Implementing New HAL Drivers (If Needed)

If your board uses a peripheral not yet supported by an existing driver, implement the corresponding abstract interface in `include/ersa/hal/`:

### Display Interface (`include/ersa/hal/display.h`)
```cpp
class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual Result<void> init() = 0;
    virtual void powerOff() = 0;
    virtual void powerOn() = 0;
    virtual uint16_t getWidth() const = 0;
    virtual uint16_t getHeight() const = 0;
    virtual void clear(uint16_t color = 0) = 0;
    virtual void display(bool fullRefresh = false) = 0;
};
```

### Real-Time Clock Interface (`include/ersa/hal/rtc.h`)
```cpp
class IRtc {
public:
    virtual ~IRtc() = default;
    virtual Result<void> init() = 0;
    virtual bool isOnline() const = 0;
    virtual uint32_t getEpoch() = 0;
    virtual Result<void> setEpoch(uint32_t epochSeconds) = 0;
};
```

### Battery Gauge Interface (`include/ersa/hal/battery.h`)
```cpp
class IBattery {
public:
    virtual ~IBattery() = default;
    virtual Result<void> init() = 0;
    virtual uint16_t getMillivolts() = 0;
    virtual uint8_t getPercentage() = 0;
    virtual bool isConnected() = 0;
};
```

### Input Interface (`include/ersa/hal/input.h`)
```cpp
class IInput {
public:
    virtual ~IInput() = default;
    virtual Result<void> init() = 0;
    virtual void poll() = 0;
};
```

---

## 5. Adding a Target to `platformio.ini`

Add your target board environment to `platformio.ini`:

```ini
[env:my_custom_watch]
platform = espressif32@6.12.0
board = seeed_xiao_esp32c3     ; Or esp32-s3-devkitc-1, etc.
framework = arduino
build_unflags = -std=gnu++11
build_flags =
    -std=gnu++17
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -Iinclude
    -Isrc
lib_deps =
    zinggjm/GxEPD2 @ ^1.6.9
    adafruit/Adafruit GFX Library @ ^1.12.6
    adafruit/RTClib @ ^2.1.4
    mathertel/OneButton @ ^2.6.2
```

---

## 6. Testing & Verifying Your Board

### Run Host Unit Tests
Verify your changes did not break core system services or the application lifecycle:
```bash
make test
```

### Compile Firmware
Build your firmware target with PlatformIO:
```bash
./scripts/pio.sh run -e my_custom_watch
```

### Flash and Monitor
```bash
./scripts/pio.sh run -e my_custom_watch --target upload
./scripts/pio.sh device monitor
```
