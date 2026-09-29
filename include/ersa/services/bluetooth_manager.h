#pragma once

#include "ersa/hal/bluetooth.h"
#include "ersa/events/event_bus.h"
#include "ersa/common/types.h"

namespace ersa {
namespace services {

enum class CallState : uint8_t {
    Idle = 0,
    Incoming,
    Active,
    Ended
};

struct RecentCall {
    char name[32];
    char number[20];
    uint32_t timestampEpoch;
};

struct AppNotification {
    char title[32];
    char message[64];
    char app[20];
    uint32_t uid;
    uint32_t timestampEpoch;
    bool canDismissRemotely;
};

class BluetoothManager {
public:
    using WakeCallback = void (*)(void* user);
    static BluetoothManager& instance();
    static void setInstance(BluetoothManager* inst);

    BluetoothManager(hal::IBluetooth& ble, events::EventBus& bus);

    Result<void> init();
    void tick();
    void setWakeCallback(WakeCallback callback, void* user) { wakeCallback_ = callback; wakeUserData_ = user; }
    bool canDial() const { return isConnected() && ble_.supportsDial(); }
    bool canHangup() const { return isConnected() && ble_.supportsHangup(); }
    bool notificationsReady() const { return ble_.notificationsReady(); }
    bool mediaReady() const { return ble_.mediaReady(); }

    bool isConnected() const;
    bool isAdvertising() const { return ble_.isAdvertising(); }
    const char* getDeviceName() const;
    const char* getDeviceAddress() const;
    void restartAdvertising();

    // Call state & telephony actions
    CallState getCallState() const { return callState_; }
    const char* getCallerName() const { return currentCaller_; }
    const char* getCallerNumber() const { return currentNumber_; }
    uint32_t getCallDurationSec() const;

    void acceptCall();
    void rejectCall();
    void hangupCall();
    void dial(const char* number, const char* name = nullptr);
    void dialRecent(size_t index = 0);

    // Recent calls history
    size_t getRecentCallCount() const { return recentCount_; }
    const RecentCall& getRecentCall(size_t index) const;
    void addRecentCall(const char* name, const char* number);

    // Media playback state & control
    bool isPlaying() const { return mediaPlaying_; }
    const char* getMediaTitle() const { return mediaTitle_; }
    const char* getMediaArtist() const { return mediaArtist_; }

    void mediaPlay();
    void mediaPause();
    void mediaToggle();
    void mediaNext();
    void mediaPrevious();

    // Testing / Simulation hooks
    void simulateIncomingCall(const char* name, const char* number);
    void simulateMedia(const char* title, const char* artist, bool playing);
    void simulateNotification(const char* title, const char* message, const char* app = "Messages");

    // Notifications history
    size_t getNotificationCount() const { return notifCount_; }
    const AppNotification& getNotification(size_t index) const;
    bool dismissNotification(size_t index);
    void addNotification(const char* title, const char* message, const char* app, uint32_t uid, bool canDismissRemotely = false);
    void clearNotifications();

private:
    static void onBleCall(hal::BleCallAction action, const char* caller, const char* number, void* user);
    static void onBleMedia(bool playing, const char* title, const char* artist, void* user);
    static void onBleConnection(bool connected, void* user);
    static void onBleNotification(const char* title, const char* message, const char* app, uint32_t uid, bool canDismissRemotely, void* user);

    void receive(const events::Event& event);
    void apply(const events::Event& event);
#if defined(ARDUINO)
    void* incomingQueue_{nullptr};
#endif
    WakeCallback wakeCallback_{nullptr};
    void* wakeUserData_{nullptr};
    hal::IBluetooth& ble_;
    events::EventBus& bus_;

    CallState callState_{CallState::Idle};
    char currentCaller_[32]{""};
    char currentNumber_[20]{""};
    uint32_t callStartMs_{0};

    static constexpr size_t MAX_RECENTS = 5;
    RecentCall recents_[MAX_RECENTS];
    size_t recentCount_{0};

    static constexpr size_t MAX_NOTIFS = 10;
    AppNotification notifications_[MAX_NOTIFS];
    size_t notifCount_{0};
    static constexpr size_t MAX_DISMISSED_UIDS = 16;
    uint32_t dismissedUids_[MAX_DISMISSED_UIDS]{};
    size_t dismissedCount_{0};

    bool mediaPlaying_{false};
    char mediaTitle_[32]{"No Media"};
    char mediaArtist_[32]{"Bluetooth Idle"};
};

} // namespace services
} // namespace ersa
