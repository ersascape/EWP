#pragma once

#include "ersa/hal/battery.h"

namespace ersa {
namespace test {

class MockBattery : public hal::IBattery {
public:
    explicit MockBattery(uint16_t mv = 3900, uint8_t pct = 80, bool conn = true, bool chg = false)
        : mv_(mv), pct_(pct), conn_(conn), chg_(chg) {}

    Result<void> init() override { return Result<void>(); }
    void sample() override {}

    uint16_t millivolts() const override { return mv_; }
    uint8_t percentage() const override { return pct_; }
    bool isConnected() const override { return conn_; }
    bool isCharging() const override { return chg_; }

    void setMv(uint16_t mv) { mv_ = mv; }
    void setPct(uint8_t pct) { pct_ = pct; }
    void setConn(bool conn) { conn_ = conn; }
    void setChg(bool chg) { chg_ = chg; }

private:
    uint16_t mv_;
    uint8_t pct_;
    bool conn_;
    bool chg_;
};

} // namespace test
} // namespace ersa
