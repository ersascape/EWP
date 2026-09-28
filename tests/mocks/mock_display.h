#pragma once

#include "ersa/hal/display.h"
#include <vector>

namespace ersa {
namespace test {

class MockDisplay : public hal::IDisplay {
public:
    static constexpr int16_t W = 200;
    static constexpr int16_t H = 200;

    MockDisplay() : buffer_(W * H, 0) {}

    Result<void> init() override {
        powered_ = true;
        return Result<void>();
    }

    int16_t width() const override { return W; }
    int16_t height() const override { return H; }

    void clear(hal::Color color = hal::Color::Black) override {
        std::fill(buffer_.begin(), buffer_.end(), (color == hal::Color::White) ? 1 : 0);
    }

    void drawPixel(int16_t x, int16_t y, hal::Color color) override {
        if (x >= 0 && x < W && y >= 0 && y < H) {
            buffer_[y * W + x] = (color == hal::Color::White) ? 1 : 0;
        }
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, hal::Color color) override {
        for (int16_t j = y; j < y + h; ++j) {
            for (int16_t i = x; i < x + w; ++i) {
                drawPixel(i, j, color);
            }
        }
    }

    void refresh(bool full = false) override {
        if (full) fullRefreshes_++;
        else partialRefreshes_++;
        powered_ = true;
    }

    void refreshRect(const Rect& rect) override {
        (void)rect;
        partialRefreshes_++;
        powered_ = true;
    }

    bool isBusy() const override { return false; }
    void setBusyCallback(hal::DisplayBusyCallback cb, void* userData = nullptr) override {
        (void)cb; (void)userData;
    }

    void powerOff() override { powered_ = false; }
    void powerOn() override { powered_ = true; }
    bool isPowered() const override { return powered_; }

    const uint8_t* getBuffer() const override { return buffer_.data(); }
    uint8_t* getBuffer() override { return buffer_.data(); }

    hal::Color getPixel(int16_t x, int16_t y) const {
        if (x >= 0 && x < W && y >= 0 && y < H) {
            return buffer_[y * W + x] ? hal::Color::White : hal::Color::Black;
        }
        return hal::Color::Black;
    }

    uint32_t fullRefreshes_{0};
    uint32_t partialRefreshes_{0};
    bool powered_{true};

private:
    std::vector<uint8_t> buffer_;
};

} // namespace test
} // namespace ersa
