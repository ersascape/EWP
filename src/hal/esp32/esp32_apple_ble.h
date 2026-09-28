#pragma once

#include "ersa/hal/bluetooth.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <esp_gatts_api.h>

namespace ersa {
namespace hal {

class Esp32AppleClient {
public:
    Esp32AppleClient();
    ~Esp32AppleClient();

    void setCallCallback(BleCallCallback cb, void* userData);
    void setMediaCallback(BleMediaCallback cb, void* userData);
    void setNotificationCallback(BleNotificationCallback cb, void* userData);

    void startDiscovery(const esp_bd_addr_t bda, esp_ble_addr_type_t addrType = BLE_ADDR_TYPE_RANDOM);
    void authenticationComplete(bool success);
    void stop();

    bool isAncsActive() const;
    bool isAmsActive() const;

    void acceptCall();
    void rejectCall();
    bool dismissNotification(uint32_t uid);
    void mediaCommand(BleMediaAction action);

private:
    class Impl;
    Impl* pImpl_{nullptr};
};

} // namespace hal
} // namespace ersa

#else

namespace ersa {
namespace hal {

class Esp32AppleClient {
public:
    Esp32AppleClient() = default;
    ~Esp32AppleClient() = default;
    void setCallCallback(BleCallCallback, void*) {}
    void setMediaCallback(BleMediaCallback, void*) {}
    void setNotificationCallback(BleNotificationCallback, void*) {}
    void startDiscovery(const uint8_t*, uint8_t = 0) {}
    void authenticationComplete(bool) {}
    void stop() {}
    bool isAncsActive() const { return false; }
    bool isAmsActive() const { return false; }
    void acceptCall() {}
    void rejectCall() {}
    bool dismissNotification(uint32_t) { return false; }
    void mediaCommand(BleMediaAction) {}
};

} // namespace hal
} // namespace ersa

#endif
