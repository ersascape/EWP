#pragma once

#include "ersa/hal/rtc.h"

namespace ersa {
namespace test {

class MockRtc : public hal::IRtc {
public:
    explicit MockRtc(uint32_t epoch = 1790535111) : epoch_(epoch) {}

    Result<void> init() override {
        healthy_ = true;
        return Result<void>();
    }

    hal::TimePoint now() override {
        hal::TimePoint tp;
        tp.epoch = epoch_;
        tp.hour = (epoch_ / 3600) % 24;
        tp.minute = (epoch_ / 60) % 60;
        tp.second = epoch_ % 60;
        return tp;
    }

    Result<void> adjust(const hal::TimePoint& time) override {
        epoch_ = time.epoch;
        return Result<void>();
    }

    Result<void> setEpoch(uint32_t epochSeconds) override {
        epoch_ = epochSeconds;
        return Result<void>();
    }

    bool isHealthy() const override { return healthy_; }

    void advanceSeconds(uint32_t sec) { epoch_ += sec; }
    void advanceMinutes(uint32_t min) { epoch_ += min * 60; }
    void setHealthy(bool h) { healthy_ = h; }

private:
    uint32_t epoch_{0};
    bool healthy_{true};
};

} // namespace test
} // namespace ersa
