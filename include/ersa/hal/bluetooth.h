#pragma once

#include "ersa/common/types.h"
#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace hal {

enum class BleCallAction : uint8_t {
    Incoming = 0,
    Answered,
    Rejected,
    Ended
};

enum class BleMediaAction : uint8_t {
    Play = 0,
    Pause,
    Toggle,
    Next,
    Previous,
    VolumeUp,
    VolumeDown
};

using BleCallCallback = void (*)(BleCallAction action, const char* caller, const char* number, void* userData);
using BleMediaCallback = void (*)(bool playing, const char* title, const char* artist, void* userData);
using BleConnectionCallback = void (*)(bool connected, void* userData);
// Null title/message remove one UID; null app as well clears the protocol session.
using BleNotificationCallback = void (*)(const char* title, const char* message, const char* app, uint32_t uid, bool canDismissRemotely, void* userData);

class IBluetooth {
public:
    virtual ~IBluetooth() = default;

    virtual Result<void> init() = 0;
    virtual void startAdvertising() = 0;
    virtual void stopAdvertising() = 0;
    virtual bool isConnected() const = 0;
    virtual bool isAdvertising() const { return false; }
    virtual void tick() {}
    virtual const char* getDeviceName() const = 0;
    virtual const char* getDeviceAddress() const = 0;

    // Callbacks for events arriving from phone
    virtual void setCallCallback(BleCallCallback cb, void* userData) = 0;
    virtual void setMediaCallback(BleMediaCallback cb, void* userData) = 0;
    virtual void setConnectionCallback(BleConnectionCallback cb, void* userData) = 0;
    virtual void setNotificationCallback(BleNotificationCallback cb, void* userData) = 0;

    // Capabilities describe the connected provider, independently of the UI.
    virtual bool supportsDial() const { return false; }
    virtual bool supportsHangup() const { return false; }
    virtual bool notificationsReady() const { return isConnected(); }
    virtual bool mediaReady() const { return isConnected(); }

    // Commands sent from watch to phone
    virtual void acceptCall() = 0;
    virtual void rejectCall() = 0;
    virtual void hangupCall() = 0;
    virtual void dial(const char* number) = 0;

    virtual void mediaCommand(BleMediaAction action) = 0;
    virtual bool dismissNotification(uint32_t uid) { (void)uid; return false; }
};

} // namespace hal
} // namespace ersa
