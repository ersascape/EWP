#include "app_call.h"
#include "ersa/services/bluetooth_manager.h"
#include "ersa/app/application_manager.h"
#include "fonts/misans_fonts.h"
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

    if (state == ersa::services::CallState::Incoming) {
        if (event == Buttons::Event::Next) {
            // B1 = ACCEPT Call
            DebugLog::log("CALL: B1 pressed -> Accept call");
            bleMgr.acceptCall();
            return true;
        } else if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong || event == Buttons::Event::Home) {
            // B2 = DECLINE / HANG UP Call
            DebugLog::log("CALL: B2 pressed -> Decline call");
            bleMgr.rejectCall();
            ersa::app::ApplicationManager::instance().switchTo("watchface_clock");
            return true;
        }
    } else if (state == ersa::services::CallState::Active) {
        if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong) {
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
            if (bleMgr.getRecentCallCount() > 0) {
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
    auto& bleMgr = ersa::services::BluetoothManager::instance();
    auto state = bleMgr.getCallState();
    const char* caller = bleMgr.getCallerName();
    const char* number = bleMgr.getCallerNumber();

    display.fillScreen(0);   // Solid black
    display.setTextColor(1); // White

    if (state == ersa::services::CallState::Incoming) {
        // 1. Header
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 26);
        display.print("incoming call...");

        // Animated / blinking call badge icon outline
        display.drawRoundRect(18, 40, 164, 76, 6, 1);
        display.drawRoundRect(19, 41, 162, 74, 5, 1);

        // Caller name
        display.setFont(&MiSansLatin_Bold10pt7b);
        display.setCursor(30, 68);
        display.print((caller && caller[0]) ? caller : "Unknown");

        // Number
        if (number && number[0]) {
            display.setFont(&MiSansLatin_Regular8pt7b);
            display.setCursor(30, 92);
            display.print(number);
        }

        // 2. Action buttons
        // Top button B1: ACCEPT (Filled White Badge)
        display.fillRoundRect(18, 130, 164, 26, 4, 1);
        display.setTextColor(0); // Black text on white
        display.setFont(&MiSansLatin_Bold8pt7b);
        display.setCursor(30, 147);
        display.print("B1: ACCEPT CALL");

        // Bottom button B2: DECLINE (Outline Badge)
        display.setTextColor(1); // White text
        display.drawRoundRect(18, 162, 164, 26, 4, 1);
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(30, 179);
        display.print("B2: DECLINE / HANG UP");

    } else if (state == ersa::services::CallState::Active) {
        // Header
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 26);
        display.print("in call");

        // Call duration
        uint32_t sec = bleMgr.getCallDurationSec();
        char durBuf[16];
        snprintf(durBuf, sizeof(durBuf), "%02u:%02u", unsigned(sec / 60), unsigned(sec % 60));
        display.setCursor(140, 26);
        display.print(durBuf);

        // Caller box
        display.drawRoundRect(18, 44, 164, 72, 6, 1);
        display.setFont(&MiSansLatin_Bold10pt7b);
        display.setCursor(30, 74);
        display.print((caller && caller[0]) ? caller : "Connected");

        if (number && number[0]) {
            display.setFont(&MiSansLatin_Regular8pt7b);
            display.setCursor(30, 98);
            display.print(number);
        }

        // Hang Up Button B2
        display.fillRoundRect(18, 154, 164, 28, 4, 1);
        display.setTextColor(0); // Black text on white badge
        display.setFont(&MiSansLatin_Bold8pt7b);
        display.setCursor(36, 172);
        display.print("B2: HANG UP");

    } else if (state == ersa::services::CallState::Idle) {
        // Idle screen: Recent calls list
        display.setFont(&MiSansLatin_Bold10pt7b);
        display.setCursor(18, 24);
        display.print("recent calls");

        size_t count = bleMgr.getRecentCallCount();
        if (count == 0) {
            display.setFont(&MiSansLatin_Regular8pt7b);
            display.setCursor(18, 60);
            display.print("no recent calls");
        } else {
            constexpr int16_t startY = 52;
            constexpr int16_t rowH = 26;
            for (size_t i = 0; i < count && i < 4; ++i) {
                const auto& call = bleMgr.getRecentCall(i);
                int16_t y = startY + static_cast<int16_t>(i * rowH);

                display.setFont(&MiSansLatin_Bold8pt7b);
                display.setCursor(18, y);
                display.print(call.name);

                display.setFont(&MiSansLatin_Regular8pt7b);
                display.setCursor(94, y);
                display.print(call.number);
            }
        }

        // Footer
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 170);
        display.print("b1: dial top contact");
        display.setCursor(18, 188);
        display.print("b2: return to drawer");

    } else {
        // Call ended screen
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 26);
        display.print("call ended");

        display.setFont(&MiSansLatin_Bold10pt7b);
        display.setCursor(18, 80);
        display.print((caller && caller[0]) ? caller : "Call Finished");

        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 180);
        display.print("press any button to return");
    }
}

} // namespace AppCall
