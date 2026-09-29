#pragma once

#include "ersa/common/types.h"
#include <stdint.h>

namespace ersa {
namespace hal {

using BleConnectionCallback = void (*)(bool connected, void* userData);

class IBluetooth {
public:
    virtual ~IBluetooth() = default;

    virtual Result<void> init() = 0;
    virtual void startAdvertising() = 0;
    virtual void stopAdvertising() = 0;
    virtual bool isConnected() const = 0;
    virtual bool isAdvertising() const { return false; }
    // Milliseconds until radio maintenance must run, or UINT32_MAX if none.
    virtual uint32_t nextWakeDelayMs(uint32_t nowMs) const { (void)nowMs; return UINT32_MAX; }
    virtual void tick() {}
    virtual const char* getDeviceName() const = 0;
    virtual const char* getDeviceAddress() const = 0;

    // BLE link state only. Companion data and commands belong to ICompanionSource.
    virtual void setConnectionCallback(BleConnectionCallback cb, void* userData) = 0;
};

} // namespace hal
} // namespace ersa
