#include "app_now_playing.h"
#include "ersa/services/bluetooth_manager.h"
#include "ersa/app/application_manager.h"
#include "fonts/misans_fonts.h"
#include "core/debug_log.h"
#include <Arduino.h>

namespace AppNowPlaying {

void begin() {}

bool onButton(Buttons::Event event) {
    auto& bleMgr = ersa::services::BluetoothManager::instance();

    if (event == Buttons::Event::Next) {
        // B1 Click = Next Track
        DebugLog::log("MEDIA: Next track");
        bleMgr.mediaNext();
        return true;
    } else if (event == Buttons::Event::Action) {
        // B2 Click = Play / Pause Toggle
        DebugLog::log("MEDIA: Toggle play/pause");
        bleMgr.mediaToggle();
        return true;
    } else if (event == Buttons::Event::Previous) {
        // B1 Double Click = Previous Track
        DebugLog::log("MEDIA: Previous track");
        bleMgr.mediaPrevious();
        return true;
    } else if (event == Buttons::Event::Home) {
        // B1 Long = Exit to Drawer
        ersa::app::ApplicationManager::instance().switchTo("app_drawer");
        return true;
    } else if (event == Buttons::Event::ActionLong) {
        // B2 Long = Previous Track
        DebugLog::log("MEDIA: Previous track (hold B2)");
        bleMgr.mediaPrevious();
        return true;
    }
    return false;
}

void render(Adafruit_GFX& display) {
    auto& bleMgr = ersa::services::BluetoothManager::instance();
    const bool playing = bleMgr.isPlaying();
    const char* title = bleMgr.getMediaTitle();
    const char* artist = bleMgr.getMediaArtist();
    const bool connected = bleMgr.isConnected();

    display.fillScreen(0);   // Solid black
    display.setTextColor(1); // White

    // 1. Header
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(18, 24);
    display.print("now playing");

    display.setFont(&MiSansLatin_Regular8pt7b);
    display.setCursor(140, 24);
    display.print(connected ? "ble on" : "ble idle");

    // 2. Center Card (Media info)
    display.drawRoundRect(14, 40, 172, 100, 6, 1);

    // Track Title
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(24, 72);
    display.print((title && title[0]) ? title : "No Track");

    // Artist
    display.setFont(&MiSansLatin_Regular8pt7b);
    display.setCursor(24, 96);
    display.print((artist && artist[0]) ? artist : "Select track on phone");

    // Playback state indicator
    if (playing) {
        display.fillRoundRect(24, 110, 80, 20, 3, 1);
        display.setTextColor(0); // Black on white
        display.setFont(&MiSansLatin_Bold8pt7b);
        display.setCursor(30, 124);
        display.print("> PLAYING");
    } else {
        display.drawRoundRect(24, 110, 80, 20, 3, 1);
        display.setTextColor(1); // White outline
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(30, 124);
        display.print("|| PAUSED");
    }

    // 3. Footer controls guide
    display.setTextColor(1);
    display.setFont(&MiSansLatin_Regular8pt7b);
    display.setCursor(18, 170);
    display.print("b1: next track");
    display.setCursor(18, 188);
    display.print("b2: play / pause");
}

} // namespace AppNowPlaying
