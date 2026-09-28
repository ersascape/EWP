#pragma once

#include "ersa/hal/bluetooth.h"
#include <string.h>

namespace ersa {
namespace test {

class MockBluetooth : public hal::IBluetooth {
public:
    MockBluetooth() = default;

    Result<void> init() override { return Result<void>(); }
    void startAdvertising() override { advertising_ = true; }
    void stopAdvertising() override { advertising_ = false; }
    bool isConnected() const override { return connected_; }

    void setCallCallback(hal::BleCallCallback cb, void* userData) override {
        callCb_ = cb;
        callUserData_ = userData;
    }

    void setMediaCallback(hal::BleMediaCallback cb, void* userData) override {
        mediaCb_ = cb;
        mediaUserData_ = userData;
    }

    void setConnectionCallback(hal::BleConnectionCallback cb, void* userData) override {
        connCb_ = cb;
        connUserData_ = userData;
    }

    void acceptCall() override {
        acceptCount_++;
    }

    void rejectCall() override {
        rejectCount_++;
    }

    void hangupCall() override {
        hangupCount_++;
    }

    void dial(const char* number) override {
        dialCount_++;
        if (number) {
            strncpy(lastDialed_, number, sizeof(lastDialed_) - 1);
            lastDialed_[sizeof(lastDialed_) - 1] = '\0';
        }
    }

    void mediaCommand(hal::BleMediaAction action) override {
        lastMediaAction_ = action;
        mediaCmdCount_++;
    }

    // Test helper simulation triggers
    void simulateConnection(bool conn) {
        connected_ = conn;
        if (connCb_) connCb_(conn, connUserData_);
    }

    void simulateIncomingCall(const char* caller, const char* number) {
        if (callCb_) callCb_(hal::BleCallAction::Incoming, caller, number, callUserData_);
    }

    void simulateCallEnded() {
        if (callCb_) callCb_(hal::BleCallAction::Ended, "", "", callUserData_);
    }

    void simulateMedia(bool playing, const char* title, const char* artist) {
        if (mediaCb_) mediaCb_(playing, title, artist, mediaUserData_);
    }

    bool isAdvertising() const { return advertising_; }
    int acceptCount() const { return acceptCount_; }
    int rejectCount() const { return rejectCount_; }
    int hangupCount() const { return hangupCount_; }
    int dialCount() const { return dialCount_; }
    const char* lastDialed() const { return lastDialed_; }
    int mediaCmdCount() const { return mediaCmdCount_; }
    hal::BleMediaAction lastMediaAction() const { return lastMediaAction_; }

private:
    bool advertising_{false};
    bool connected_{false};

    hal::BleCallCallback callCb_{nullptr};
    void* callUserData_{nullptr};

    hal::BleMediaCallback mediaCb_{nullptr};
    void* mediaUserData_{nullptr};

    hal::BleConnectionCallback connCb_{nullptr};
    void* connUserData_{nullptr};

    int acceptCount_{0};
    int rejectCount_{0};
    int hangupCount_{0};
    int dialCount_{0};
    char lastDialed_[32]{""};

    int mediaCmdCount_{0};
    hal::BleMediaAction lastMediaAction_{hal::BleMediaAction::Play};
};

} // namespace test
} // namespace ersa
