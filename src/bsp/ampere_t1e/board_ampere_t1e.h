#pragma once

#if defined(ARDUINO)

#include "ersa/board/board.h"
#include "ersa/board/board_config.h"
#include "hal/esp32/esp32_display.h"
#include "hal/esp32/esp32_rtc.h"
#include "hal/esp32/esp32_battery.h"
#include "hal/esp32/esp32_input.h"
#include "hal/esp32/esp32_bluetooth.h"

namespace ersa {
namespace board {

class BoardAmpereT1e : public Board {
public:
    BoardAmpereT1e();
    ~BoardAmpereT1e() override = default;

    Result<void> init() override;
    const char* getName() const override { return "Ampere Works T1E"; }
    const BoardConfig& getConfig() const override { return config_; }

    hal::IDisplay& getDisplay() override { return display_; }
    hal::IRtc& getRtc() override { return rtc_; }
    hal::IBattery& getBattery() override { return battery_; }
    hal::IInput& getInput() override { return input_; }
    hal::IBluetooth& getBluetooth() override { return bluetooth_; }
    hal::ICompanionSource& getCompanionSource() override { return bluetooth_; }

    hal::Esp32Display& getEsp32Display() { return display_; }
    hal::Esp32Rtc& getEsp32Rtc() { return rtc_; }
    hal::Esp32Bluetooth& getEsp32Bluetooth() { return bluetooth_; }

    uint32_t getUptimeMs() const override;
    void delayMs(uint32_t ms) override;

    static BoardAmpereT1e& instance();

private:
    BoardConfig config_;
    hal::Esp32Display display_;
    hal::Esp32Rtc rtc_;
    hal::Esp32Battery battery_;
    hal::Esp32Input input_;
    hal::Esp32Bluetooth bluetooth_;
};

} // namespace board
} // namespace ersa

#endif // ARDUINO
