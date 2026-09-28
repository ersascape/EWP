#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace config {

// ==========================================
// Network & Time Synchronization Settings
// ==========================================

// Primary and fallback NTP pool servers
inline constexpr const char* DEFAULT_NTP_SERVERS[] = {
    "pool.ntp.org",
    "time.google.com",
    "time.cloudflare.com",
    "time.apple.com",
    "time.nist.gov"
};
inline constexpr size_t NUM_NTP_SERVERS = sizeof(DEFAULT_NTP_SERVERS) / sizeof(DEFAULT_NTP_SERVERS[0]);

// HTTP Time Fallback Endpoints (used when UDP port 123 is blocked on cellular/guest networks)
inline constexpr const char* DEFAULT_HTTP_TIME_ENDPOINTS[] = {
    "http://clients3.google.com/generate_204",
    "http://connectivitycheck.gstatic.com/generate_204",
    "http://worldtimeapi.org/api/ip",
    "http://www.cloudflare.com"
};
inline constexpr size_t NUM_HTTP_TIME_ENDPOINTS = sizeof(DEFAULT_HTTP_TIME_ENDPOINTS) / sizeof(DEFAULT_HTTP_TIME_ENDPOINTS[0]);

// Network timeouts (milliseconds)
inline constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
inline constexpr uint32_t NTP_SYNC_TIMEOUT_MS     = 8000;
inline constexpr uint32_t HTTP_TIME_TIMEOUT_MS    = 6000;
inline constexpr uint32_t CALDAV_HTTP_TIMEOUT_MS  = 35000;
inline constexpr uint8_t  MAX_WIFI_RETRIES        = 2;

// Fallback Wi-Fi Credentials
inline constexpr const char* DEFAULT_WIFI_SSID    = "";
inline constexpr const char* DEFAULT_WIFI_PASS    = "";

// SoftAP Provisioning Portal Defaults
inline constexpr const char* DEFAULT_AP_SSID      = "ErsaWatch-Config";
inline constexpr const char* DEFAULT_AP_PASS      = "12345678";
inline constexpr const char* DEFAULT_AP_IP        = "192.168.4.1";
inline constexpr uint16_t   DEFAULT_AP_TIMEOUT_SEC = 180;

// CalDAV (Nextcloud/Radicale/etc.) Fallbacks
inline constexpr const char* DEFAULT_CALDAV_SERVER    = "";
inline constexpr const char* DEFAULT_CALDAV_USER      = "";
inline constexpr const char* DEFAULT_CALDAV_PASS      = "";
inline constexpr const char* DEFAULT_CALDAV_CALENDAR  = "murena-team";
inline constexpr const char* FALLBACK_CALDAV_CALENDAR = "personal";
inline constexpr const char* DEFAULT_CALDAV_TODO      = "tasks";
inline constexpr const char* FALLBACK_CALDAV_TODO      = "personal";

// Timezone and Display Localization Defaults
inline constexpr int16_t DEFAULT_TIMEZONE_OFFSET_MIN = 330; // +05:30 (IST)
inline constexpr bool    DEFAULT_MILITARY_TIME       = false; // 12-hour by default
inline constexpr uint8_t DEFAULT_FULL_REFRESH_CYCLES = 20;

// NVS / Preferences Namespaces
inline constexpr const char* PREFS_NS_CONFIG = "watch_cfg";
inline constexpr const char* PREFS_NS_CACHE  = "net_cache";

} // namespace config
} // namespace ersa
