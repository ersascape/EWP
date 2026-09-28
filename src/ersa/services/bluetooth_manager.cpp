#include "ersa/services/bluetooth_manager.h"
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
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
    : ble_(ble), bus_(bus) {
    // Seed standard fallback recent call for instant out-of-the-box quick dial
    addRecentCall("Home", "+1234567890");
    addRecentCall("Mom", "+1987654321");
}

Result<void> BluetoothManager::init() {
    ble_.setCallCallback(onBleCall, this);
    ble_.setMediaCallback(onBleMedia, this);
    ble_.setConnectionCallback(onBleConnection, this);
    ble_.setNotificationCallback(onBleNotification, this);

    Result<void> res = ble_.init();
    ble_.startAdvertising();
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
    ble_.stopAdvertising();
    ble_.startAdvertising();
}

uint32_t BluetoothManager::getCallDurationSec() const {
    if (callState_ != CallState::Active || callStartMs_ == 0) return 0;
    const uint32_t now = millis();
    return (now >= callStartMs_) ? ((now - callStartMs_) / 1000) : 0;
}

void BluetoothManager::acceptCall() {
    ble_.acceptCall();
    callState_ = CallState::Active;
    callStartMs_ = millis();

    events::Event evt = events::Event::createCall(
        events::EventType::CallAccepted, currentCaller_, currentNumber_, 1, callStartMs_);
    bus_.publish(evt);
}

void BluetoothManager::rejectCall() {
    ble_.rejectCall();
    callState_ = CallState::Ended;

    events::Event evt = events::Event::createCall(
        events::EventType::CallRejected, currentCaller_, currentNumber_, 2, millis());
    bus_.publish(evt);
}

void BluetoothManager::hangupCall() {
    ble_.hangupCall();
    callState_ = CallState::Ended;

    events::Event evt = events::Event::createCall(
        events::EventType::CallEnded, currentCaller_, currentNumber_, 2, millis());
    bus_.publish(evt);
}

void BluetoothManager::dial(const char* number, const char* name) {
    if (!number || number[0] == '\0') return;

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
    if (!number || number[0] == '\0') return;

    // Shift entries down to make room at index 0
    size_t copyLimit = (recentCount_ < MAX_RECENTS) ? recentCount_ : (MAX_RECENTS - 1);
    for (size_t i = copyLimit; i > 0; --i) {
        recents_[i] = recents_[i - 1];
    }

    if (name && name[0] != '\0') {
        strncpy(recents_[0].name, name, sizeof(recents_[0].name) - 1);
    } else {
        strncpy(recents_[0].name, number, sizeof(recents_[0].name) - 1);
    }
    recents_[0].name[sizeof(recents_[0].name) - 1] = '\0';

    strncpy(recents_[0].number, number, sizeof(recents_[0].number) - 1);
    recents_[0].number[sizeof(recents_[0].number) - 1] = '\0';
    recents_[0].timestampEpoch = millis() / 1000;

    if (recentCount_ < MAX_RECENTS) {
        recentCount_++;
    }
}

void BluetoothManager::mediaPlay() {
    ble_.mediaCommand(hal::BleMediaAction::Play);
    mediaPlaying_ = true;
    bus_.publish(events::Event::createMedia(mediaTitle_, mediaArtist_, true, millis()));
}

void BluetoothManager::mediaPause() {
    ble_.mediaCommand(hal::BleMediaAction::Pause);
    mediaPlaying_ = false;
    bus_.publish(events::Event::createMedia(mediaTitle_, mediaArtist_, false, millis()));
}

void BluetoothManager::mediaToggle() {
    ble_.mediaCommand(hal::BleMediaAction::Toggle);
    mediaPlaying_ = !mediaPlaying_;
    bus_.publish(events::Event::createMedia(mediaTitle_, mediaArtist_, mediaPlaying_, millis()));
}

void BluetoothManager::mediaNext() {
    ble_.mediaCommand(hal::BleMediaAction::Next);
}

void BluetoothManager::mediaPrevious() {
    ble_.mediaCommand(hal::BleMediaAction::Previous);
}

void BluetoothManager::simulateIncomingCall(const char* name, const char* number) {
    onBleCall(hal::BleCallAction::Incoming, name, number, this);
}

void BluetoothManager::simulateMedia(const char* title, const char* artist, bool playing) {
    onBleMedia(playing, title, artist, this);
}

void BluetoothManager::onBleCall(hal::BleCallAction action, const char* caller, const char* number, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (!self) return;

    if (action == hal::BleCallAction::Incoming) {
        self->callState_ = CallState::Incoming;
        self->callStartMs_ = 0;

        if (caller && caller[0] != '\0') {
            strncpy(self->currentCaller_, caller, sizeof(self->currentCaller_) - 1);
        } else {
            strncpy(self->currentCaller_, number ? number : "Unknown", sizeof(self->currentCaller_) - 1);
        }
        self->currentCaller_[sizeof(self->currentCaller_) - 1] = '\0';

        if (number) {
            strncpy(self->currentNumber_, number, sizeof(self->currentNumber_) - 1);
            self->currentNumber_[sizeof(self->currentNumber_) - 1] = '\0';
        } else {
            self->currentNumber_[0] = '\0';
        }

        self->addRecentCall(self->currentCaller_, self->currentNumber_);

        events::Event evt = events::Event::createCall(
            events::EventType::CallIncoming, self->currentCaller_, self->currentNumber_, 0, millis());
        self->bus_.publish(evt);
    } else if (action == hal::BleCallAction::Answered) {
        self->callState_ = CallState::Active;
        self->callStartMs_ = millis();
        events::Event evt = events::Event::createCall(
            events::EventType::CallAccepted, self->currentCaller_, self->currentNumber_, 1, self->callStartMs_);
        self->bus_.publish(evt);
    } else if (action == hal::BleCallAction::Rejected || action == hal::BleCallAction::Ended) {
        self->callState_ = CallState::Ended;
        events::Event evt = events::Event::createCall(
            events::EventType::CallEnded, self->currentCaller_, self->currentNumber_, 2, millis());
        self->bus_.publish(evt);
    }
}

void BluetoothManager::onBleMedia(bool playing, const char* title, const char* artist, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (!self) return;

    self->mediaPlaying_ = playing;
    if (title && title[0] != '\0') {
        strncpy(self->mediaTitle_, title, sizeof(self->mediaTitle_) - 1);
        self->mediaTitle_[sizeof(self->mediaTitle_) - 1] = '\0';
    }
    if (artist && artist[0] != '\0') {
        strncpy(self->mediaArtist_, artist, sizeof(self->mediaArtist_) - 1);
        self->mediaArtist_[sizeof(self->mediaArtist_) - 1] = '\0';
    }

    events::Event evt = events::Event::createMedia(self->mediaTitle_, self->mediaArtist_, self->mediaPlaying_, millis());
    self->bus_.publish(evt);
}

void BluetoothManager::onBleConnection(bool connected, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (!self) return;

    events::Event evt(connected ? events::EventType::BleConnected : events::EventType::BleDisconnected, millis());
    self->bus_.publish(evt);
}

void BluetoothManager::simulateNotification(const char* title, const char* message, const char* app) {
    onBleNotification(title, message, app, 1, this);
}

const AppNotification& BluetoothManager::getNotification(size_t index) const {
    if (notifCount_ == 0) {
        static AppNotification empty{"", "", "", 0, 0};
        return empty;
    }
    return notifications_[index < notifCount_ ? index : 0];
}

void BluetoothManager::addNotification(const char* title, const char* message, const char* app, uint32_t uid) {
    if (!title && !message) return;

    const size_t maxShift = (notifCount_ < MAX_NOTIFS) ? notifCount_ : (MAX_NOTIFS - 1);
    for (size_t i = maxShift; i > 0; --i) {
        notifications_[i] = notifications_[i - 1];
    }

    AppNotification& newest = notifications_[0];
    newest.uid = uid;
    newest.timestampEpoch = millis() / 1000;
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

void BluetoothManager::onBleNotification(const char* title, const char* message, const char* app, uint32_t uid, void* user) {
    auto* self = static_cast<BluetoothManager*>(user);
    if (!self) return;

    self->addNotification(title, message, app, uid);

    events::Event evt = events::Event::createNotification(title, message, app, uid, millis());
    self->bus_.publish(evt);
}

} // namespace services
} // namespace ersa
