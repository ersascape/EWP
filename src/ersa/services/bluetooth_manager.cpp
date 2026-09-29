#include "ersa/services/bluetooth_manager.h"
#include "ersa/services/storage_service.h"
#include "core/debug_log.h"
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#else
#include <time.h>
static uint32_t host_millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}
#define millis host_millis
#endif

namespace ersa {
namespace services {

class DummyBle : public hal::IBluetooth {
public:
    Result<void> init() override { return Result<void>(); }
    void startAdvertising() override {}
    void stopAdvertising() override {}
    bool isConnected() const override { return false; }
    void setCallCallback(hal::BleCallCallback, void*) override {}
    void setMediaCallback(hal::BleMediaCallback, void*) override {}
    void setConnectionCallback(hal::BleConnectionCallback, void*) override {}
    void setNotificationCallback(hal::BleNotificationCallback, void*) override {}
    void acceptCall() override {}
    void rejectCall() override {}
    void hangupCall() override {}
    void dial(const char*) override {}
    void mediaCommand(hal::BleMediaAction) override {}
    const char* getDeviceName() const override { return "Ersa Wearable"; }
    const char* getDeviceAddress() const override { return "00:00:00:00:00:00"; }
};

static BluetoothManager* s_instance = nullptr;

BluetoothManager& BluetoothManager::instance() {
    if (!s_instance) {
        static DummyBle s_dummyBle;
        static events::EventBus s_dummyBus;
        static BluetoothManager s_dummy(s_dummyBle, s_dummyBus);
        return s_dummy;
    }
    return *s_instance;
}

void BluetoothManager::setInstance(BluetoothManager* inst) {
    s_instance = inst;
}

BluetoothManager::BluetoothManager(hal::IBluetooth& ble, events::EventBus& bus)
    : ble_(ble), bus_(bus) {}

Result<void> BluetoothManager::init() {
    auto& storage = StorageService::instance();
    storage.init();
    recentCount_ = static_cast<size_t>(storage.getInt("recent_count", 0));
    if (recentCount_ > MAX_RECENTS) recentCount_ = MAX_RECENTS;
    for (size_t i = 0; i < recentCount_; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "recent_name_%u", unsigned(i));
        const std::string name = storage.getString(key);
        snprintf(key, sizeof(key), "recent_num_%u", unsigned(i));
        const std::string number = storage.getString(key);
        snprintf(key, sizeof(key), "recent_time_%u", unsigned(i));
        strncpy(recents_[i].name, name.c_str(), sizeof(recents_[i].name) - 1);
        strncpy(recents_[i].number, number.c_str(), sizeof(recents_[i].number) - 1);
        recents_[i].timestampEpoch = static_cast<uint32_t>(storage.getInt(key, 0));
        recents_[i].name[sizeof(recents_[i].name) - 1] = '\0';
        recents_[i].number[sizeof(recents_[i].number) - 1] = '\0';
    }
#if defined(ARDUINO)
    if (!incomingQueue_) incomingQueue_ = xQueueCreate(24, sizeof(events::Event));
    if (!incomingQueue_) return Result<void>(ErrorCode::OutOfMemory, "BLE event queue");
#endif
    ble_.setCallCallback(onBleCall, this);
    ble_.setMediaCallback(onBleMedia, this);
    ble_.setConnectionCallback(onBleConnection, this);
    ble_.setNotificationCallback(onBleNotification, this);
    ble_.setTimeCallback([](uint32_t epoch, void* user) {
        auto* self = static_cast<BluetoothManager*>(user);
        if (self) {
            if (self->wakeCallback_) self->wakeCallback_(self->wakeUserData_);
            self->bus_.post(events::Event::createTimeSync(
                epoch, events::TimeSource::BleCurrentTime, millis()));
        }
    }, this);

    Result<void> res = ble_.init();
    if (res.isOk()) ble_.startAdvertising();
    return res;
}

bool BluetoothManager::isConnected() const {
    return ble_.isConnected();
}

const char* BluetoothManager::getDeviceName() const {
    return ble_.getDeviceName();
}

const char* BluetoothManager::getDeviceAddress() const {
    return ble_.getDeviceAddress();
}

void BluetoothManager::restartAdvertising() {
    if (isConnected()) return;
    ble_.stopAdvertising();
    ble_.startAdvertising();
}

uint32_t BluetoothManager::getCallDurationSec() const {
    if (callState_ != CallState::Active || callStartMs_ == 0) return 0;
    const uint32_t now = millis();
    return (now >= callStartMs_) ? ((now - callStartMs_) / 1000) : 0;
}

void BluetoothManager::acceptCall() {
    if (isConnected() && callState_ == CallState::Incoming) ble_.acceptCall();
}

void BluetoothManager::rejectCall() {
    if (isConnected() && callState_ == CallState::Incoming) ble_.rejectCall();
}

void BluetoothManager::hangupCall() {
    if (canHangup() && callState_ == CallState::Active) ble_.hangupCall();
}

void BluetoothManager::dial(const char* number, const char* name) {
    if (!canDial() || !number || number[0] == '\0') return;

    ble_.dial(number);

    strncpy(currentNumber_, number, sizeof(currentNumber_) - 1);
    currentNumber_[sizeof(currentNumber_) - 1] = '\0';

    if (name && name[0] != '\0') {
        strncpy(currentCaller_, name, sizeof(currentCaller_) - 1);
    } else {
        strncpy(currentCaller_, number, sizeof(currentCaller_) - 1);
    }
    currentCaller_[sizeof(currentCaller_) - 1] = '\0';

    callState_ = CallState::Active;
    callStartMs_ = millis();

    addRecentCall(currentCaller_, currentNumber_);

    events::Event evt = events::Event::createCall(
        events::EventType::CallAccepted, currentCaller_, currentNumber_, 1, callStartMs_);
    bus_.publish(evt);
}

void BluetoothManager::dialRecent(size_t index) {
    if (recentCount_ == 0) return;
    if (index >= recentCount_) index = 0;
    if (!recents_[index].number[0]) return;
    dial(recents_[index].number, recents_[index].name);
}

const RecentCall& BluetoothManager::getRecentCall(size_t index) const {
    if (recentCount_ == 0) {
        static RecentCall empty{"", "", 0};
        return empty;
    }
    return recents_[index < recentCount_ ? index : 0];
}

void BluetoothManager::addRecentCall(const char* name, const char* number) {
    const bool hasName = name && name[0];
    const bool hasNumber = number && number[0];
    if (!hasName && !hasNumber) name = "unknown caller";
    const bool effectiveName = name && name[0];

    // Merge an ANCS name-only event with a later phone-number update for the
    // same caller rather than consuming two slots in the recent list.
    const bool sameNumber = hasNumber && recentCount_ && recents_[0].number[0] &&
                            strcmp(recents_[0].number, number) == 0;
    const bool enrichNameOnly = effectiveName && recentCount_ && !recents_[0].number[0] &&
                                strcmp(recents_[0].name, name) == 0;
    const bool enrichUnknown = effectiveName && recentCount_ &&
                               !recents_[0].number[0] &&
                               strcmp(recents_[0].name, "unknown caller") == 0 &&
                               strcmp(name, "unknown caller") != 0;
    if (sameNumber || enrichNameOnly || enrichUnknown) {
        if (effectiveName) {
            strncpy(recents_[0].name, name, sizeof(recents_[0].name) - 1);
            recents_[0].name[sizeof(recents_[0].name) - 1] = '\0';
        }
        if (hasNumber) {
            strncpy(recents_[0].number, number, sizeof(recents_[0].number) - 1);
            recents_[0].number[sizeof(recents_[0].number) - 1] = '\0';
        }
        recents_[0].timestampEpoch = millis() / 1000;
    } else {
        size_t copyLimit = (recentCount_ < MAX_RECENTS) ? recentCount_ : (MAX_RECENTS - 1);
        for (size_t i = copyLimit; i > 0; --i) recents_[i] = recents_[i - 1];

        const char* displayName = effectiveName ? name : number;
        strncpy(recents_[0].name, displayName, sizeof(recents_[0].name) - 1);
        recents_[0].name[sizeof(recents_[0].name) - 1] = '\0';

        if (hasNumber) strncpy(recents_[0].number, number, sizeof(recents_[0].number) - 1);
        else recents_[0].number[0] = '\0';
        recents_[0].number[sizeof(recents_[0].number) - 1] = '\0';
        recents_[0].timestampEpoch = millis() / 1000;

        if (recentCount_ < MAX_RECENTS) recentCount_++;
    }

    auto& storage = StorageService::instance();
    storage.setInt("recent_count", static_cast<int32_t>(recentCount_));
    for (size_t i = 0; i < recentCount_; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "recent_name_%u", unsigned(i));
        storage.setString(key, recents_[i].name);
        snprintf(key, sizeof(key), "recent_num_%u", unsigned(i));
        storage.setString(key, recents_[i].number);
        snprintf(key, sizeof(key), "recent_time_%u", unsigned(i));
        storage.setInt(key, static_cast<int32_t>(recents_[i].timestampEpoch));
    }
#if defined(ARDUINO)
    DebugLog::log("CALL: recent saved count=%u has_name=%u has_number=%u",
                  unsigned(recentCount_), unsigned(effectiveName), unsigned(hasNumber));
#endif
}

void BluetoothManager::mediaPlay() {
    if (mediaReady()) ble_.mediaCommand(hal::BleMediaAction::Play);
}

void BluetoothManager::mediaPause() {
    if (mediaReady()) ble_.mediaCommand(hal::BleMediaAction::Pause);
}

void BluetoothManager::mediaToggle() {
    if (mediaReady()) ble_.mediaCommand(hal::BleMediaAction::Toggle);
}

void BluetoothManager::mediaNext() {
    if (mediaReady()) ble_.mediaCommand(hal::BleMediaAction::Next);
}

void BluetoothManager::mediaPrevious() {
    if (mediaReady()) ble_.mediaCommand(hal::BleMediaAction::Previous);
}

void BluetoothManager::simulateIncomingCall(const char* name, const char* number) {
    onBleCall(hal::BleCallAction::Incoming, name, number, this);
}

void BluetoothManager::simulateMedia(const char* title, const char* artist, bool playing) {
    onBleMedia(playing, title, artist, this);
}

void BluetoothManager::receive(const events::Event& event) {
#if defined(ARDUINO)
    if (incomingQueue_) {
        if (xQueueSend(static_cast<QueueHandle_t>(incomingQueue_), &event, 0) == pdTRUE) {
            if (wakeCallback_) wakeCallback_(wakeUserData_);
        } else {
#if defined(ARDUINO)
            DebugLog::log("BLE: manager event queue full; dropping event type=%u", unsigned(event.type));
#endif
        }
    } else {
#if defined(ARDUINO)
        DebugLog::log("BLE: manager event queue unavailable; dropping event type=%u", unsigned(event.type));
#endif
    }
#else
    apply(event);
#endif
}

void BluetoothManager::tick() {
    ble_.tick();
#if defined(ARDUINO)
    events::Event event;
    while (incomingQueue_ && xQueueReceive(static_cast<QueueHandle_t>(incomingQueue_), &event, 0) == pdTRUE) apply(event);
#endif
}

void BluetoothManager::apply(const events::Event& event) {
    using events::EventType;
    switch (event.type) {
        case EventType::CallIncoming:
            callState_ = CallState::Incoming;
            strncpy(currentCaller_, event.call.caller, sizeof(currentCaller_) - 1);
            currentCaller_[sizeof(currentCaller_) - 1] = '\0';
            strncpy(currentNumber_, event.call.number, sizeof(currentNumber_) - 1);
            currentNumber_[sizeof(currentNumber_) - 1] = '\0';
            if (currentCaller_[0] || currentNumber_[0]) {
                if (recentCount_ == 0 ||
                    (currentNumber_[0] ? strcmp(recents_[0].number, currentNumber_) != 0
                                       : strcmp(recents_[0].name, currentCaller_) != 0))
                    addRecentCall(currentCaller_, currentNumber_);
            } else {
                // ANCS can announce an incoming-call category before its
                // caller attributes arrive, or without exposing them at all.
                addRecentCall(nullptr, nullptr);
            }
            break;
        case EventType::CallAccepted:
            callState_ = CallState::Active; callStartMs_ = millis(); break;
        case EventType::CallEnded:
        case EventType::CallRejected:
            callState_ = CallState::Idle;
            callStartMs_ = 0;
            currentCaller_[0] = currentNumber_[0] = '\0';
            break;
        case EventType::MediaTrackChanged:
            mediaPlaying_ = event.media.playing;
            strncpy(mediaTitle_, event.media.title, sizeof(mediaTitle_));
            strncpy(mediaArtist_, event.media.artist, sizeof(mediaArtist_));
            break;
        case EventType::NotificationsCleared:
            clearNotifications(); dismissedCount_ = 0; break;
        case EventType::NotificationRemoved:
            for (size_t i = 0; i < notifCount_; ++i) {
                if (notifications_[i].uid != event.notification.uid) continue;
                for (size_t j = i + 1; j < notifCount_; ++j) notifications_[j - 1] = notifications_[j];
                --notifCount_;
                break;
            }
            for (size_t i = 0; i < dismissedCount_; ++i) {
                if (dismissedUids_[i] != event.notification.uid) continue;
                for (size_t j = i + 1; j < dismissedCount_; ++j) dismissedUids_[j - 1] = dismissedUids_[j];
                --dismissedCount_;
                break;
            }
            break;
        case EventType::NotificationReceived:
#if defined(ARDUINO)
            DebugLog::log("ANCS: notification attributes received uid=%08lx",
                          static_cast<unsigned long>(event.notification.uid));
#endif
            for (size_t i = 0; i < dismissedCount_; ++i)
                if (dismissedUids_[i] == event.notification.uid) return;
            addNotification(event.notification.title, event.notification.message,
                            event.notification.app, event.notification.uid,
                            event.notification.canDismissRemotely);
            break;
        case EventType::BleDisconnected:
            callState_ = CallState::Idle; callStartMs_ = 0;
            currentCaller_[0] = currentNumber_[0] = 0;
            mediaPlaying_ = false; mediaTitle_[0] = mediaArtist_[0] = 0;
            clearNotifications(); // ANCS identifiers are scoped to a connection session.
            dismissedCount_ = 0;
            break;
        default: break;
    }
    bus_.publish(event);
}

void BluetoothManager::onBleCall(hal::BleCallAction action, const char* caller, const char* number, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (!self) return;
    const auto type = action == hal::BleCallAction::Incoming ? events::EventType::CallIncoming :
                      action == hal::BleCallAction::Answered ? events::EventType::CallAccepted : events::EventType::CallEnded;
    self->receive(events::Event::createCall(type, caller, number, uint8_t(action), millis()));
}

void BluetoothManager::onBleMedia(bool playing, const char* title, const char* artist, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (self) self->receive(events::Event::createMedia(title, artist, playing, millis()));
}

void BluetoothManager::onBleConnection(bool connected, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (self) self->receive(events::Event(connected ? events::EventType::BleConnected : events::EventType::BleDisconnected, millis()));
}

void BluetoothManager::simulateNotification(const char* title, const char* message, const char* app) {
    onBleNotification(title, message, app, 1, false, this);
}

const AppNotification& BluetoothManager::getNotification(size_t index) const {
    if (notifCount_ == 0) {
        static AppNotification empty{"", "", "", 0, 0, false};
        return empty;
    }
    return notifications_[index < notifCount_ ? index : 0];
}

bool BluetoothManager::dismissNotification(size_t index) {
    if (index >= notifCount_) return false;
    const uint32_t uid = notifications_[index].uid;
    if (notifications_[index].canDismissRemotely) ble_.dismissNotification(uid);
    if (dismissedCount_ == MAX_DISMISSED_UIDS) {
        for (size_t i = 1; i < dismissedCount_; ++i) dismissedUids_[i - 1] = dismissedUids_[i];
        --dismissedCount_;
    }
    dismissedUids_[dismissedCount_++] = uid;
    for (size_t i = index + 1; i < notifCount_; ++i) notifications_[i - 1] = notifications_[i];
    --notifCount_;
    return true;
}

void BluetoothManager::addNotification(const char* title, const char* message, const char* app, uint32_t uid, bool canDismissRemotely) {
    if (!title && !message) return;

    for (size_t i = 0; i < notifCount_; ++i) {
        if (notifications_[i].uid != uid) continue;
        for (size_t j = i + 1; j < notifCount_; ++j) notifications_[j - 1] = notifications_[j];
        --notifCount_;
        break;
    }
    const size_t maxShift = (notifCount_ < MAX_NOTIFS) ? notifCount_ : (MAX_NOTIFS - 1);
    for (size_t i = maxShift; i > 0; --i) {
        notifications_[i] = notifications_[i - 1];
    }

    AppNotification& newest = notifications_[0];
    newest.uid = uid;
    newest.timestampEpoch = millis() / 1000;
    newest.canDismissRemotely = canDismissRemotely;
    strncpy(newest.title, title ? title : "Notification", sizeof(newest.title) - 1);
    newest.title[sizeof(newest.title) - 1] = '\0';
    strncpy(newest.message, message ? message : "", sizeof(newest.message) - 1);
    newest.message[sizeof(newest.message) - 1] = '\0';
    strncpy(newest.app, app ? app : "", sizeof(newest.app) - 1);
    newest.app[sizeof(newest.app) - 1] = '\0';

    if (notifCount_ < MAX_NOTIFS) {
        notifCount_++;
    }
}

void BluetoothManager::clearNotifications() {
    notifCount_ = 0;
}

void BluetoothManager::onBleNotification(const char* title, const char* message, const char* app, uint32_t uid, bool canDismissRemotely, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (!self) return;

    events::Event evt = events::Event::createNotification(title, message, app, uid, millis(), canDismissRemotely);
    if (!title && !message) evt.type = app ? events::EventType::NotificationRemoved : events::EventType::NotificationsCleared;
    else {
#if defined(ARDUINO)
        DebugLog::log("ANCS: notification callback uid=%08lx dismiss=%u",
                      static_cast<unsigned long>(uid), unsigned(canDismissRemotely));
#endif
    }
    self->receive(evt);
}

} // namespace services
} // namespace ersa
