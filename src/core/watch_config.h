#pragma once
#include <stdint.h>
#include <stddef.h>

namespace WatchConfig {

struct Config {
    char wifiSsid[33];
    char wifiPass[65];
    char caldavServer[129];
    char caldavUser[49];
    char caldavPass[65];
    char caldavCalendar[49];
    char caldavTodoPath[49];
    int16_t timezoneOffsetMin;
    bool militaryTime;
    uint8_t fullRefreshInterval;
    char apSsid[33];
    char apPass[33];
    uint16_t apTimeoutSec;
};

void begin();
const Config& get();
void setWifi(const char* ssid, const char* pass);
void setCalDav(const char* server, const char* user, const char* pass, const char* calendar, const char* todoPath);
void setTimezone(int16_t offsetMinutes);
void setTimeFormat(bool military24h);
void setApConfig(const char* ssid, const char* pass, uint16_t timeoutSec);
void save();
void resetDefaults();

} // namespace WatchConfig
