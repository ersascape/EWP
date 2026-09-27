#include "watch_config.h"
#include "debug_log.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace WatchConfig {

namespace {
Config activeConfig;
Preferences prefs;
constexpr const char* PREFS_NS = "watch_cfg";

void safeCopy(char* dest, const char* src, size_t maxLen) {
    if (!dest || maxLen == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, maxLen - 1);
    dest[maxLen - 1] = '\0';
}
} // namespace

void resetDefaults() {
    safeCopy(activeConfig.wifiSsid, "", sizeof(activeConfig.wifiSsid));
    safeCopy(activeConfig.wifiPass, "", sizeof(activeConfig.wifiPass));
    safeCopy(activeConfig.caldavServer, "", sizeof(activeConfig.caldavServer));
    safeCopy(activeConfig.caldavUser, "", sizeof(activeConfig.caldavUser));
    safeCopy(activeConfig.caldavPass, "", sizeof(activeConfig.caldavPass));
    safeCopy(activeConfig.caldavCalendar, "personal", sizeof(activeConfig.caldavCalendar));
    safeCopy(activeConfig.caldavTodoPath, "tasks", sizeof(activeConfig.caldavTodoPath));
    activeConfig.timezoneOffsetMin = 330; // Default +05:30 (IST)
    activeConfig.militaryTime = true;
    activeConfig.fullRefreshInterval = 20;
    safeCopy(activeConfig.apSsid, "ErsaWatch-Config", sizeof(activeConfig.apSsid));
    safeCopy(activeConfig.apPass, "12345678", sizeof(activeConfig.apPass));
    activeConfig.apTimeoutSec = 180;
}

void begin() {
    resetDefaults();
    if (prefs.begin(PREFS_NS, true)) {
        String s;
        s = prefs.getString("ssid", "");
        if (s.length() > 0) safeCopy(activeConfig.wifiSsid, s.c_str(), sizeof(activeConfig.wifiSsid));

        s = prefs.getString("pass", "");
        if (s.length() > 0) safeCopy(activeConfig.wifiPass, s.c_str(), sizeof(activeConfig.wifiPass));

        s = prefs.getString("dav_srv", "");
        if (s.length() > 0) safeCopy(activeConfig.caldavServer, s.c_str(), sizeof(activeConfig.caldavServer));

        s = prefs.getString("dav_usr", "");
        if (s.length() > 0) safeCopy(activeConfig.caldavUser, s.c_str(), sizeof(activeConfig.caldavUser));

        s = prefs.getString("dav_pwd", "");
        if (s.length() > 0) safeCopy(activeConfig.caldavPass, s.c_str(), sizeof(activeConfig.caldavPass));

        s = prefs.getString("dav_cal", "personal");
        safeCopy(activeConfig.caldavCalendar, s.c_str(), sizeof(activeConfig.caldavCalendar));

        s = prefs.getString("dav_tod", "tasks");
        safeCopy(activeConfig.caldavTodoPath, s.c_str(), sizeof(activeConfig.caldavTodoPath));

        activeConfig.timezoneOffsetMin = prefs.getShort("tz", activeConfig.timezoneOffsetMin);
        activeConfig.militaryTime = prefs.getBool("24h", activeConfig.militaryTime);
        activeConfig.fullRefreshInterval = prefs.getUChar("fref", activeConfig.fullRefreshInterval);

        s = prefs.getString("ap_ssid", "ErsaWatch-Config");
        safeCopy(activeConfig.apSsid, s.c_str(), sizeof(activeConfig.apSsid));

        s = prefs.getString("ap_pass", "12345678");
        safeCopy(activeConfig.apPass, s.c_str(), sizeof(activeConfig.apPass));

        activeConfig.apTimeoutSec = prefs.getUShort("ap_to", activeConfig.apTimeoutSec);
        prefs.end();
        DebugLog::log("CONFIG loaded (tz=%d, ssid='%s', caldav='%s')",
                      activeConfig.timezoneOffsetMin, activeConfig.wifiSsid, activeConfig.caldavServer);
    } else {
        DebugLog::log("CONFIG no stored preferences; using defaults");
    }
}

const Config& get() {
    return activeConfig;
}

void setWifi(const char* ssid, const char* pass) {
    safeCopy(activeConfig.wifiSsid, ssid, sizeof(activeConfig.wifiSsid));
    safeCopy(activeConfig.wifiPass, pass, sizeof(activeConfig.wifiPass));
}

void setCalDav(const char* server, const char* user, const char* pass, const char* calendar, const char* todoPath) {
    safeCopy(activeConfig.caldavServer, server, sizeof(activeConfig.caldavServer));
    safeCopy(activeConfig.caldavUser, user, sizeof(activeConfig.caldavUser));
    safeCopy(activeConfig.caldavPass, pass, sizeof(activeConfig.caldavPass));
    if (calendar && calendar[0] != '\0') {
        safeCopy(activeConfig.caldavCalendar, calendar, sizeof(activeConfig.caldavCalendar));
    }
    if (todoPath && todoPath[0] != '\0') {
        safeCopy(activeConfig.caldavTodoPath, todoPath, sizeof(activeConfig.caldavTodoPath));
    }
}

void setTimezone(int16_t offsetMinutes) {
    activeConfig.timezoneOffsetMin = offsetMinutes;
}

void setTimeFormat(bool military24h) {
    activeConfig.militaryTime = military24h;
}

void setApConfig(const char* ssid, const char* pass, uint16_t timeoutSec) {
    safeCopy(activeConfig.apSsid, ssid, sizeof(activeConfig.apSsid));
    safeCopy(activeConfig.apPass, pass, sizeof(activeConfig.apPass));
    activeConfig.apTimeoutSec = timeoutSec;
}

void save() {
    if (prefs.begin(PREFS_NS, false)) {
        prefs.putString("ssid", activeConfig.wifiSsid);
        prefs.putString("pass", activeConfig.wifiPass);
        prefs.putString("dav_srv", activeConfig.caldavServer);
        prefs.putString("dav_usr", activeConfig.caldavUser);
        prefs.putString("dav_pwd", activeConfig.caldavPass);
        prefs.putString("dav_cal", activeConfig.caldavCalendar);
        prefs.putString("dav_tod", activeConfig.caldavTodoPath);
        prefs.putShort("tz", activeConfig.timezoneOffsetMin);
        prefs.putBool("24h", activeConfig.militaryTime);
        prefs.putUChar("fref", activeConfig.fullRefreshInterval);
        prefs.putString("ap_ssid", activeConfig.apSsid);
        prefs.putString("ap_pass", activeConfig.apPass);
        prefs.putUShort("ap_to", activeConfig.apTimeoutSec);
        prefs.end();
        DebugLog::log("CONFIG saved to NVS");
    } else {
        DebugLog::log("CONFIG error opening NVS for write");
    }
}

} // namespace WatchConfig
