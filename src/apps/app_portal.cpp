#include "app_portal.h"
#include "core/watch_config.h"
#include "core/net_sync.h"
#include "core/debug_log.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Fonts/FreeSansBold9pt7b.h>

namespace AppPortal {

namespace {
bool apRunning = false;
uint32_t apStartTime = 0;
WebServer server(80);
DNSServer dnsServer;

void drawCentered(Adafruit_GFX& display, const char* text, int16_t y, const GFXfont* font = nullptr) {
    display.setFont(font);
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - int16_t(w)) / 2 - x1, y - y1);
    display.print(text);
}

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Ersa Watch Settings</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;margin:0;padding:16px;background:#f4f4f7;color:#222;}
.card{background:#fff;border-radius:12px;padding:20px;box-shadow:0 2px 8px rgba(0,0,0,0.08);max-width:440px;margin:0 auto;}
h2{margin-top:0;color:#111;font-size:22px;border-bottom:2px solid #eee;padding-bottom:10px;}
h3{margin:18px 0 8px 0;font-size:16px;color:#444;}
label{display:block;margin:10px 0 4px 0;font-weight:600;font-size:13px;color:#555;}
input[type=text],input[type=password],select{width:100%;box-sizing:border-box;padding:10px;border:1px solid #ccc;border-radius:6px;font-size:14px;}
.btn{display:block;width:100%;box-sizing:border-box;background:#0066cc;color:#fff;border:none;padding:12px;border-radius:6px;font-size:15px;font-weight:bold;margin-top:16px;cursor:pointer;}
.btn-alt{background:#28a745;}
.btn-stop{background:#dc3545;}
.note{font-size:12px;color:#888;margin-top:4px;}
</style>
</head>
<body>
<div class="card">
<h2>Watch Settings</h2>
<form action="/save" method="POST">
<h3>Wi-Fi</h3>
<label>Network SSID</label>
<input type="text" name="ssid" value="%SSID%" required>
<label>Password</label>
<input type="password" name="pass" value="%PASS%">

<h3>CalDAV (Nextcloud / Cloud)</h3>
<label>Server URL</label>
<input type="text" name="dav_srv" value="%DAV_SRV%" placeholder="https://cloud.example.com/remote.php/dav/calendars/user/">
<label>Username</label>
<input type="text" name="dav_usr" value="%DAV_USR%">
<label>App Password / Token</label>
<input type="password" name="dav_pwd" value="%DAV_PWD%">
<label>Events Calendar Name</label>
<input type="text" name="dav_cal" value="%DAV_CAL%" placeholder="personal">
<label>Tasks / Todo Calendar Name</label>
<input type="text" name="dav_tod" value="%DAV_TOD%" placeholder="tasks">

<h3>Clock & Timezone</h3>
<label>Timezone Offset (Minutes)</label>
<select name="tz">
<option value="330" %TZ_330%>+05:30 (India Standard Time)</option>
<option value="0" %TZ_0%>UTC (GMT / London)</option>
<option value="-300" %TZ_M300%>-05:00 (US Eastern Time)</option>
<option value="-480" %TZ_M480%>-08:00 (US Pacific Time)</option>
<option value="60" %TZ_60%>+01:00 (Central European Time)</option>
<option value="120" %TZ_120%>+02:00 (Eastern European Time)</option>
<option value="480" %TZ_480%>+08:00 (Singapore / China)</option>
<option value="540" %TZ_540%>+09:00 (Japan / Korea)</option>
</select>

<button type="submit" class="btn">Save Settings</button>
<button type="submit" name="sync_ntp" value="1" class="btn btn-alt">Save & Sync NTP Time</button>
</form>
<form action="/stop" method="POST">
<button type="submit" class="btn btn-stop">Turn Off Hotspot</button>
</form>
</div>
</body>
</html>
)rawliteral";

String renderHtml() {
    const auto& cfg = WatchConfig::get();
    String s = FPSTR(HTML_PAGE);
    s.replace("%SSID%", cfg.wifiSsid);
    s.replace("%PASS%", cfg.wifiPass);
    s.replace("%DAV_SRV%", cfg.caldavServer);
    s.replace("%DAV_USR%", cfg.caldavUser);
    s.replace("%DAV_PWD%", cfg.caldavPass);
    s.replace("%DAV_CAL%", cfg.caldavCalendar);
    s.replace("%DAV_TOD%", cfg.caldavTodoPath);

    s.replace("%TZ_330%", cfg.timezoneOffsetMin == 330 ? "selected" : "");
    s.replace("%TZ_0%", cfg.timezoneOffsetMin == 0 ? "selected" : "");
    s.replace("%TZ_M300%", cfg.timezoneOffsetMin == -300 ? "selected" : "");
    s.replace("%TZ_M480%", cfg.timezoneOffsetMin == -480 ? "selected" : "");
    s.replace("%TZ_60%", cfg.timezoneOffsetMin == 60 ? "selected" : "");
    s.replace("%TZ_120%", cfg.timezoneOffsetMin == 120 ? "selected" : "");
    s.replace("%TZ_480%", cfg.timezoneOffsetMin == 480 ? "selected" : "");
    s.replace("%TZ_540%", cfg.timezoneOffsetMin == 540 ? "selected" : "");
    return s;
}

void handleRoot() {
    server.send(200, "text/html", renderHtml());
}

void handleSave() {
    if (server.hasArg("ssid")) {
        WatchConfig::setWifi(server.arg("ssid").c_str(), server.arg("pass").c_str());
    }
    if (server.hasArg("dav_srv")) {
        WatchConfig::setCalDav(server.arg("dav_srv").c_str(),
                               server.arg("dav_usr").c_str(),
                               server.arg("dav_pwd").c_str(),
                               server.arg("dav_cal").c_str(),
                               server.arg("dav_tod").c_str());
    }
    if (server.hasArg("tz")) {
        WatchConfig::setTimezone(server.arg("tz").toInt());
    }
    WatchConfig::save();

    const bool syncNow = (server.arg("sync_ntp") == "1");
    String msg = "<html><body style='font-family:sans-serif;text-align:center;padding:40px;'>";
    msg += "<h2>Settings Saved!</h2>";
    if (syncNow) {
        msg += "<p>Hotspot shutting down to sync NTP time with router...</p>";
    } else {
        msg += "<p>Configuration stored to flash.</p>";
    }
    msg += "<a href='/'>Back</a></body></html>";
    server.send(200, "text/html", msg);

    if (syncNow) {
        delay(1000);
        AppPortal::onButton(Buttons::Event::Action); // Stop AP
        NetSync::syncNtp();
    }
}

void handleStop() {
    server.send(200, "text/html", "<html><body style='font-family:sans-serif;text-align:center;padding:40px;'><h2>Hotspot Stopped</h2><p>You may close this tab.</p></body></html>");
    delay(500);
    AppPortal::onButton(Buttons::Event::Action); // Stop AP
}

void startAp() {
    const auto& cfg = WatchConfig::get();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(cfg.apSsid, cfg.apPass[0] != '\0' ? cfg.apPass : nullptr);
    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/stop", HTTP_POST, handleStop);

    // Captive portal probes
    server.on("/generate_204", handleRoot);
    server.on("/fwlink", handleRoot);
    server.on("/hotspot-detect.html", handleRoot);
    server.onNotFound(handleRoot);

    server.begin();
    apRunning = true;
    apStartTime = millis();
    DebugLog::log("HOTSPOT started SSID='%s' IP=%s", cfg.apSsid, WiFi.softAPIP().toString().c_str());
}

void stopAp() {
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    apRunning = false;
    DebugLog::log("HOTSPOT stopped (power save)");
}
} // namespace

void begin() {
    apRunning = false;
}

bool isActive() {
    return apRunning;
}

void tick() {
    if (!apRunning) return;

    dnsServer.processNextRequest();
    server.handleClient();

    // Auto-timeout shutoff to prevent battery drain
    const auto& cfg = WatchConfig::get();
    if (cfg.apTimeoutSec > 0 && uint32_t(millis() - apStartTime) >= (uint32_t(cfg.apTimeoutSec) * 1000)) {
        DebugLog::log("HOTSPOT auto-off timer expired (%u sec)", cfg.apTimeoutSec);
        stopAp();
    }
}

bool onButton(Buttons::Event event) {
    if (event == Buttons::Event::Action) {
        if (apRunning) {
            stopAp();
        } else {
            startAp();
        }
        return true;
    } else if (event == Buttons::Event::ActionLong) {
        // Long press triggers direct NTP sync if WiFi is configured!
        stopAp();
        NetSync::syncNtp();
        return true;
    }
    return false;
}

void render(Adafruit_GFX& display) {
    drawCentered(display, "WIFI HOTSPOT", 18, &FreeSansBold9pt7b);

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    display.drawFastHLine(14, 42, 172, 0);

    const auto& cfg = WatchConfig::get();

    display.setCursor(14, 52);
    display.print("Status: ");
    if (apRunning) {
        display.print("ACTIVE");
    } else {
        display.print("OFF (Standby)");
    }

    display.setCursor(14, 70);
    display.print("AP SSID: ");
    display.print(cfg.apSsid);

    display.setCursor(14, 88);
    display.print("Password: ");
    display.print(cfg.apPass[0] != '\0' ? cfg.apPass : "(open)");

    display.setCursor(14, 106);
    display.print("Web IP: ");
    if (apRunning) {
        display.print(WiFi.softAPIP().toString().c_str());
    } else {
        display.print("192.168.4.1");
    }

    if (apRunning) {
        const uint32_t elapsed = (millis() - apStartTime) / 1000;
        const int32_t remain = int32_t(cfg.apTimeoutSec) - int32_t(elapsed);
        display.setCursor(14, 126);
        display.print("Auto-off: ");
        if (remain > 0) {
            display.print(remain / 60);
            display.print("m ");
            display.print(remain % 60);
            display.print("s");
        } else {
            display.print("now");
        }

        drawCentered(display, "Connect phone to AP", 146);
    } else {
        display.setCursor(14, 126);
        display.print("Press B2 to start AP");
        drawCentered(display, "Hold B2: SYNC NTP", 146);
    }
}

} // namespace AppPortal
