#include "app_call.h"
#include "ersa/services/bluetooth_manager.h"
#include "ersa/app/application_manager.h"
#include "fonts/misans_fonts.h"
#include "ui/text_layout.h"
#include "core/debug_log.h"
#include <Arduino.h>

namespace AppCall {

void begin() {}

bool isCallActiveOrIncoming() {
    auto state = ersa::services::BluetoothManager::instance().getCallState();
    return (state == ersa::services::CallState::Incoming || state == ersa::services::CallState::Active);
}

bool onButton(Buttons::Event event) {
    auto& bleMgr = ersa::services::BluetoothManager::instance();
    auto state = bleMgr.getCallState();
    if (event == Buttons::Event::None) return false;

    if (state == ersa::services::CallState::Incoming) {
        if (event == Buttons::Event::Next) {
            // B1 = ACCEPT Call
            DebugLog::log("CALL: B1 pressed -> Accept call");
            bleMgr.acceptCall();
            return true;
        } else if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong) {
            // B2 = DECLINE / HANG UP Call
            DebugLog::log("CALL: B2 pressed -> Decline call");
            bleMgr.rejectCall();
            ersa::app::ApplicationManager::instance().switchTo("watchface_clock");
            return true;
        } else if (event == Buttons::Event::Home) {
            ersa::app::ApplicationManager::instance().switchTo("watchface_clock");
            return true;
        }
    } else if (state == ersa::services::CallState::Active) {
        if ((event == Buttons::Event::Action || event == Buttons::Event::ActionLong) && bleMgr.canHangup()) {
            // B2 = HANG UP Call
            DebugLog::log("CALL: B2 pressed -> Hang up call");
            bleMgr.hangupCall();
            ersa::app::ApplicationManager::instance().switchTo("watchface_clock");
            return true;
        } else if (event == Buttons::Event::Home) {
            // Long B1: Minimize to watchface while keeping call active
            ersa::app::ApplicationManager::instance().switchTo("watchface_clock");
            return true;
        }
    } else if (state == ersa::services::CallState::Idle) {
        if (event == Buttons::Event::Next) {
            // B1 = Quick dial top recent call
            if (bleMgr.canDial() && bleMgr.getRecentCallCount() > 0) {
                DebugLog::log("CALL: B1 pressed -> Quick dial recent %s", bleMgr.getRecentCall(0).name);
                bleMgr.dialRecent(0);
                return true;
            }
        } else if (event == Buttons::Event::Action || event == Buttons::Event::Home || event == Buttons::Event::ActionLong) {
            ersa::app::ApplicationManager::instance().switchTo("app_drawer");
            return true;
        }
    } else {
        // Any button on ended call returns to watchface
        ersa::app::ApplicationManager::instance().switchTo("watchface_clock");
        return true;
    }

    return false;
}

void render(Adafruit_GFX& display) {
    auto& ble = ersa::services::BluetoothManager::instance();
    const auto state = ble.getCallState();
    using ersa::services::CallState;
    display.fillScreen(0);
    display.setTextColor(1);
    display.setTextWrap(false);
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(18, 24);
    display.print("calls");
    display.setFont(&MiSansLatin_Regular8pt7b);

    if (state == CallState::Idle) {
        WatchText::line(display, ble.isConnected() ? "recent calls" : "connect from status", 18, 47, 166);
        const size_t count = ble.getRecentCallCount();
        if (!count) {
            display.setFont(&MiSansLatin_Regular10pt7b);
            WatchText::line(display, "no recent calls", 18, 85, 166);
            display.setFont(&MiSansLatin_Regular8pt7b);
            WatchText::line(display, "incoming calls appear here", 18, 111, 166);
        }
        for (size_t i = 0; i < count && i < 2; ++i) {
            const auto& call = ble.getRecentCall(i);
            const int16_t y = 60 + i * 48;
            display.setFont(&MiSansLatin_Bold8pt7b);
            WatchText::line(display, call.name, 18, y + 17, 164);
            display.setFont(&MiSansLatin_Regular8pt7b);
            WatchText::line(display, call.number, 18, y + 35, 164);
            display.drawFastHLine(18, y + 43, 164, 1);
        }
        WatchText::line(display, ble.canDial() && count ? "b1: call most recent" : "call from your phone", 18, 168, 166);
        WatchText::line(display, "b2: back", 18, 186, 166);
        return;
    }

    WatchText::line(display, state == CallState::Incoming ? "incoming call" :
                    state == CallState::Active ? "in call" : "ringing finished", 18, 47, 166);
    display.setFont(&MiSansLatin_Bold10pt7b);
    WatchText::line(display, ble.getCallerName()[0] ? ble.getCallerName() : "unknown caller", 18, 89, 164);
    display.setFont(&MiSansLatin_Regular8pt7b);
    WatchText::line(display, ble.getCallerNumber(), 18, 114, 164);
    display.drawFastHLine(18, 135, 164, 1);
    if (state == CallState::Active) {
        char duration[20];
        const uint32_t seconds = ble.getCallDurationSec();
        snprintf(duration, sizeof(duration), "%02u:%02u", unsigned(seconds / 60), unsigned(seconds % 60));
        WatchText::line(display, duration, 18, 153, 164);
    } else if (state == CallState::Incoming) {
        WatchText::line(display, "hold b1: back", 18, 153, 164);
    }
    if (state == CallState::Incoming) {
        WatchText::line(display, "b1: answer", 18, 168, 166);
        WatchText::line(display, "b2: decline", 18, 186, 166);
    } else if (state == CallState::Active) {
        WatchText::line(display, ble.canHangup() ? "b2: end call" : "manage call on phone", 18, 168, 166);
        WatchText::line(display, "hold b1: back", 18, 186, 166);
    } else {
        WatchText::line(display, "check call on your phone", 18, 168, 166);
        WatchText::line(display, "press a button to return", 18, 186, 166);
    }
}

} // namespace AppCall
