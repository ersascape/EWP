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
    int16_t statusCode{0};
};

struct Event {
    EventType type{EventType::None};
    uint32_t timestampMs{0};

    union {
        ButtonPayload button;
        TimePayload time;
        BatteryPayload battery;
        NetworkPayload network;
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
};

} // namespace events
} // namespace ersa
