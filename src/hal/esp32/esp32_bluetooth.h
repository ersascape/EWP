#pragma once

#include "ersa/hal/bluetooth.h"

namespace ersa {
namespace hal {

class Esp32Bluetooth : public IBluetooth {
public:
    Esp32Bluetooth();
    ~Esp32Bluetooth() override;

    Result<void> init() override;
    void startAdvertising() override;
    void stopAdvertising() override;
    bool isConnected() const override;
    bool isAdvertising() const override;
    void tick() override;
    const char* getDeviceName() const override;
    const char* getDeviceAddress() const override;

    void setCallCallback(BleCallCallback cb, void* userData) override;
    void setMediaCallback(BleMediaCallback cb, void* userData) override;
    void setConnectionCallback(BleConnectionCallback cb, void* userData) override;
    void setNotificationCallback(BleNotificationCallback cb, void* userData) override;
    void setTimeCallback(BleTimeCallback cb, void* userData) override;

    bool supportsDial() const override;
    bool supportsHangup() const override;
    bool notificationsReady() const override;
    bool mediaReady() const override;
    void acceptCall() override;
    void rejectCall() override;
    void hangupCall() override;
    void dial(const char* number) override;

    void mediaCommand(BleMediaAction action) override;
    bool dismissNotification(uint32_t uid) override;

private:
    void beginAdvertising();
    class Impl;
    Impl* pImpl_{nullptr};

    BleCallCallback callCb_{nullptr};
    void* callUserData_{nullptr};

    BleMediaCallback mediaCb_{nullptr};
    void* mediaUserData_{nullptr};

    BleConnectionCallback connCb_{nullptr};
    void* connUserData_{nullptr};

    BleNotificationCallback notifCb_{nullptr};
    void* notifUserData_{nullptr};
};

} // namespace hal
} // namespace ersa
