#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace events {

enum class EventType : uint16_t {
    None = 0,
    Boot,
    Suspend,
    Resume,

    ButtonPressed,
    ButtonReleased,
    ButtonClicked,
    ButtonDoubleClicked,
    ButtonLongPressed,

    MinuteTick,
    SecondTick,

    BatteryChanged,
    BatteryLow,

    NetworkConnected,
    NetworkDisconnected,

    BleConnected,
    BleDisconnected,

    CallIncoming,
    CallAccepted,
    CallRejected,
    CallEnded,

    MediaTrackChanged,
    MediaStateChanged,

    NotificationReceived,
    Custom
};

enum class ButtonId : uint8_t {
    Unknown = 0,
    Button1, // Top button (S2 on Ampere Works T1E)
    Button2  // Bottom button (S1 on Ampere Works T1E)
};

struct ButtonPayload {
    ButtonId button{ButtonId::Unknown};
};

struct TimePayload {
    uint32_t epoch{0};
    uint16_t year{2026};
    uint8_t month{1};
    uint8_t day{1};
    uint8_t hour{0};
    uint8_t minute{0};
    uint8_t second{0};
};

struct BatteryPayload {
    uint16_t millivolts{0};
    uint8_t percentage{100};
    bool connected{false};
    bool charging{false};
};

struct NetworkPayload {
    bool connected{false};
};

struct CallPayload {
    char caller[32];
    char number[20];
    uint8_t state; // 0: Incoming, 1: Active, 2: Ended
};

struct MediaPayload {
    char title[32];
    char artist[32];
    bool playing;
};

struct NotificationPayload {
    char title[32];
    char message[64];
    char app[20];
    uint32_t uid;
};

struct Event {
    EventType type{EventType::None};
    uint32_t timestampMs{0};

    union {
        ButtonPayload button;
        TimePayload time;
        BatteryPayload battery;
        NetworkPayload network;
        CallPayload call;
        MediaPayload media;
        NotificationPayload notification;
        void* customPayload;
    };

    Event() : type(EventType::None), timestampMs(0) {
        button.button = ButtonId::Unknown;
    }

    explicit Event(EventType t, uint32_t ts = 0) : type(t), timestampMs(ts) {
        button.button = ButtonId::Unknown;
    }

    static Event createButton(EventType t, ButtonId btn, uint32_t ts = 0) {
        Event e(t, ts);
        e.button.button = btn;
        return e;
    }

    static Event createMinuteTick(const TimePayload& tp, uint32_t ts = 0) {
        Event e(EventType::MinuteTick, ts);
        e.time = tp;
        return e;
    }

    static Event createBatteryChanged(uint16_t mv, uint8_t pct, bool conn, bool chg, uint32_t ts = 0) {
        Event e(EventType::BatteryChanged, ts);
        e.battery.millivolts = mv;
        e.battery.percentage = pct;
        e.battery.connected = conn;
        e.battery.charging = chg;
        return e;
    }

    static Event createCall(EventType t, const char* caller, const char* number, uint8_t state, uint32_t ts = 0) {
        Event e(t, ts);
        e.call.state = state;
        e.call.caller[0] = '\0';
        e.call.number[0] = '\0';
        if (caller) {
            for (size_t i = 0; i < sizeof(e.call.caller) - 1 && caller[i]; ++i) {
                e.call.caller[i] = caller[i];
                e.call.caller[i + 1] = '\0';
            }
        }
        if (number) {
            for (size_t i = 0; i < sizeof(e.call.number) - 1 && number[i]; ++i) {
                e.call.number[i] = number[i];
                e.call.number[i + 1] = '\0';
            }
        }
        return e;
    }

    static Event createMedia(const char* title, const char* artist, bool playing, uint32_t ts = 0) {
        Event e(EventType::MediaTrackChanged, ts);
        e.media.playing = playing;
        e.media.title[0] = '\0';
        e.media.artist[0] = '\0';
        if (title) {
            for (size_t i = 0; i < sizeof(e.media.title) - 1 && title[i]; ++i) {
                e.media.title[i] = title[i];
                e.media.title[i + 1] = '\0';
            }
        }
        if (artist) {
            for (size_t i = 0; i < sizeof(e.media.artist) - 1 && artist[i]; ++i) {
                e.media.artist[i] = artist[i];
                e.media.artist[i + 1] = '\0';
            }
        }
        return e;
    }

    static Event createNotification(const char* title, const char* message, const char* app = "", uint32_t uid = 0, uint32_t ts = 0) {
        Event e(EventType::NotificationReceived, ts);
        e.notification.uid = uid;
        e.notification.title[0] = '\0';
        e.notification.message[0] = '\0';
        e.notification.app[0] = '\0';
        if (title) {
            for (size_t i = 0; i < sizeof(e.notification.title) - 1 && title[i]; ++i) {
                e.notification.title[i] = title[i];
                e.notification.title[i + 1] = '\0';
            }
        }
        if (message) {
            for (size_t i = 0; i < sizeof(e.notification.message) - 1 && message[i]; ++i) {
                e.notification.message[i] = message[i];
                e.notification.message[i + 1] = '\0';
            }
        }
        if (app) {
            for (size_t i = 0; i < sizeof(e.notification.app) - 1 && app[i]; ++i) {
                e.notification.app[i] = app[i];
                e.notification.app[i + 1] = '\0';
            }
        }
        return e;
    }
};

} // namespace events
} // namespace ersa
