#pragma once

#include "ersa/hal/bluetooth.h"
#include "ersa/hal/companion_source.h"
#include <atomic>

namespace ersa {
namespace hal {

// The T1E composition currently provides an ESP32 BLE transport and an Apple
// companion source in one driver object. Core services inject the two contracts
// separately so another source can replace Apple without changing the HAL API.
class Esp32Bluetooth : public IBluetooth, public ICompanionSource {
public:
    Esp32Bluetooth();
    ~Esp32Bluetooth() override;

    Result<void> init() override;
    void startAdvertising() override;
    void stopAdvertising() override;
    bool isConnected() const override;
    bool isAdvertising() const override;
    uint32_t nextWakeDelayMs(uint32_t nowMs) const override;
    void tick() override;
    const char* getDeviceName() const override;
    const char* getDeviceAddress() const override;

    const char* sourceId() const override;
    bool pauseForMaintenance() override;
    void resumeFromMaintenance() override;
    bool suspendForMaintenance() override;
    void resumeAfterMaintenance() override;
    bool isAvailable() const override;
    CompanionCapabilities capabilities() const override;
    void setCallCallback(CompanionCallCallback cb, void* userData) override;
    void setMediaCallback(CompanionMediaCallback cb, void* userData) override;
    void setConnectionCallback(BleConnectionCallback cb, void* userData) override;
    void setNotificationCallback(CompanionNotificationCallback cb, void* userData) override;
    void setTimeCallback(CompanionTimeCallback cb, void* userData) override;
    void setAvailabilityCallback(CompanionAvailabilityCallback cb, void* userData) override;
    bool acceptCall() override;
    bool rejectCall() override;
    bool hangupCall() override;
    bool dial(const char* number) override;
    bool mediaCommand(CompanionMediaAction action) override;
    bool dismissNotification(uint32_t uid) override;

private:
    void beginAdvertising();
    class Impl;
    Impl* pImpl_{nullptr};

    CompanionCallCallback callCb_{nullptr};
    void* callUserData_{nullptr};

    CompanionMediaCallback mediaCb_{nullptr};
    void* mediaUserData_{nullptr};

    BleConnectionCallback connCb_{nullptr};
    void* connUserData_{nullptr};

    CompanionNotificationCallback notifCb_{nullptr};
    void* notifUserData_{nullptr};
    CompanionTimeCallback timeCb_{nullptr};
    void* timeUserData_{nullptr};
    CompanionAvailabilityCallback availabilityCb_{nullptr};
    void* availabilityUserData_{nullptr};
    bool lastSourceAvailability_{false};
    bool maintenanceSuspended_{false};
    std::atomic<uint32_t> tickUsers_{0};
};

} // namespace hal
} // namespace ersa
