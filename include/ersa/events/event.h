#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace events {

enum class EventCategory : uint8_t {
    None = 0,
    Input,
    Time,
    Battery,
    Network,
    System
};

enum class ButtonId : uint8_t {
    Unknown = 0,
    Button1, // Top button (S2 on board)
    Button2  // Bottom button (S1 on board)
};

enum class ButtonAction : uint8_t {
    Press = 0,
    Release,
    Click,
    DoubleClick,
    LongPress
};

struct InputData {
    ButtonId button;
    ButtonAction action;
};

struct TimeData {
    uint32_t epoch;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct BatteryData {
    uint16_t millivolts;
    uint8_t percentage;
    bool connected;
    bool charging;
};

struct NetworkData {
    bool connected;
    int16_t statusCode;
};

struct SystemData {
    uint8_t state;
};

struct Event {
    EventCategory category{EventCategory::None};
    uint8_t subtype{0};
    uint32_t timestampMs{0};

    union {
        InputData input;
        TimeData time;
        BatteryData battery;
        NetworkData network;
        SystemData system;
    };

    Event() : category(EventCategory::None), subtype(0), timestampMs(0) {
        input = {ButtonId::Unknown, ButtonAction::Click};
    }

    static Event createInput(ButtonId btn, ButtonAction act, uint32_t ts = 0) {
        Event e;
        e.category = EventCategory::Input;
        e.subtype = static_cast<uint8_t>(act);
        e.timestampMs = ts;
        e.input.button = btn;
        e.input.action = act;
        return e;
    }

    static Event createMinuteTick(const TimeData& td, uint32_t ts = 0) {
        Event e;
        e.category = EventCategory::Time;
        e.subtype = 1; // Minute tick
        e.timestampMs = ts;
        e.time = td;
        return e;
    }

    static Event createBatteryUpdate(uint16_t mv, uint8_t pct, bool conn, bool chg, uint32_t ts = 0) {
        Event e;
        e.category = EventCategory::Battery;
        e.subtype = 1;
        e.timestampMs = ts;
        e.battery.millivolts = mv;
        e.battery.percentage = pct;
        e.battery.connected = conn;
        e.battery.charging = chg;
        return e;
    }
};

} // namespace events
} // namespace ersa
