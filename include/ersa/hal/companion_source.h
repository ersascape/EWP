#pragma once

#include "ersa/common/types.h"
#include <stdint.h>

namespace ersa {
namespace hal {

// Semantic companion events are transport-neutral. Providers may be backed by
// ANCS/AMS over BLE, an Android companion protocol, Linux MPRIS, or a test.
// Callback string pointers are borrowed for the callback duration only; sinks
// must copy them before returning. Sources may call callbacks from any thread.
enum class CompanionCallAction : uint8_t { Incoming = 0, Answered, Rejected, Ended };
enum class CompanionMediaAction : uint8_t { Play = 0, Pause, Toggle, Next, Previous, VolumeUp, VolumeDown };

using CompanionCallCallback = void (*)(CompanionCallAction, const char*, const char*, void*);
using CompanionMediaCallback = void (*)(bool, const char*, const char*, void*);
using CompanionTimeCallback = void (*)(uint32_t, void*);
using CompanionAvailabilityCallback = void (*)(bool, void*);
// Null title/message remove one UID; null app as well clears the provider session.
using CompanionNotificationCallback = void (*)(const char*, const char*, const char*, uint32_t, bool, void*);

struct CompanionCapabilities {
    bool notifications{false};
    bool media{false};
    bool calls{false};
    bool answerReject{false};
    bool hangup{false};
    bool dial{false};
    bool remoteDismiss{false};
    bool timeSync{false};
};

class ICompanionSource {
public:
    virtual ~ICompanionSource() = default;

    // Stable, non-null, machine-readable ID (e.g. "apple-ancs-ams",
    // "android-companion", "linux-mpris"). Keep it independent of transport
    // and UI labels; returned storage must remain valid for the source lifetime.
    virtual const char* sourceId() const = 0;
    virtual Result<void> initSource() { return Result<void>(); }
    // Called regularly from the application task; asynchronous sources can
    // leave this empty. It must not busy-wait.
    virtual void tickSource() {}
    virtual void shutdownSource() {}
    // Capabilities may vary by peer/session. Notify when availability flips;
    // consumers query capabilities() again after the callback.
    virtual bool isAvailable() const = 0;
    virtual CompanionCapabilities capabilities() const = 0;

    // Passing nullptr unregisters the corresponding sink; sources must never
    // retain a callback after shutdownSource().
    virtual void setCallCallback(CompanionCallCallback, void*) = 0;
    virtual void setMediaCallback(CompanionMediaCallback, void*) = 0;
    virtual void setNotificationCallback(CompanionNotificationCallback, void*) = 0;
    virtual void setTimeCallback(CompanionTimeCallback, void*) = 0;
    virtual void setAvailabilityCallback(CompanionAvailabilityCallback, void*) = 0;

    // Return true only when the provider accepted the command for processing;
    // this does not claim that the remote endpoint executed it.
    virtual bool acceptCall() = 0;
    virtual bool rejectCall() = 0;
    virtual bool hangupCall() = 0;
    virtual bool dial(const char* number) = 0;
    virtual bool mediaCommand(CompanionMediaAction action) = 0;
    virtual bool dismissNotification(uint32_t uid) = 0;
};

} // namespace hal
} // namespace ersa
