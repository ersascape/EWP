#include "net_sync.h"
#include "watch_config.h"
#include "watch_clock.h"
#include "debug_log.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <time.h>

namespace NetSync {

namespace {
CalEvent events[MAX_EVENTS];
size_t numEvents = 0;

CalTodo todos[MAX_TODOS];
size_t numTodos = 0;

bool syncing = false;
char statusMsg[48] = "Ready";
Preferences cachePrefs;
constexpr const char* CACHE_NS = "net_cache";

void safeCopy(char* dest, const char* src, size_t maxLen) {
    if (!dest || maxLen == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, maxLen - 1);
    dest[maxLen - 1] = '\0';
}

void loadDefaultsIfEmpty() {
    if (numEvents == 0) {
        safeCopy(events[0].title, "Sync with CalDAV", sizeof(events[0].title));
        safeCopy(events[0].timeStr, "Setup via Hotspot", sizeof(events[0].timeStr));
        numEvents = 1;
    }
    if (numTodos == 0) {
        safeCopy(todos[0].title, "Connect to Hotspot", sizeof(todos[0].title));
        todos[0].completed = false;
        safeCopy(todos[1].title, "Configure WiFi", sizeof(todos[1].title));
        todos[1].completed = false;
        safeCopy(todos[2].title, "Sync CalDAV & NTP", sizeof(todos[2].title));
        todos[2].completed = false;
        numTodos = 3;
    }
}

void saveCache() {
    if (cachePrefs.begin(CACHE_NS, false)) {
        cachePrefs.putBytes("events", events, sizeof(events));
        cachePrefs.putUChar("ev_cnt", (uint8_t)numEvents);
        cachePrefs.putBytes("todos", todos, sizeof(todos));
        cachePrefs.putUChar("td_cnt", (uint8_t)numTodos);
        cachePrefs.end();
    }
}

void loadCache() {
    if (cachePrefs.begin(CACHE_NS, true)) {
        numEvents = cachePrefs.getUChar("ev_cnt", 0);
        if (numEvents > MAX_EVENTS) numEvents = 0;
        if (numEvents > 0) {
            cachePrefs.getBytes("events", events, sizeof(events));
        }

        numTodos = cachePrefs.getUChar("td_cnt", 0);
        if (numTodos > MAX_TODOS) numTodos = 0;
        if (numTodos > 0) {
            cachePrefs.getBytes("todos", todos, sizeof(todos));
        }
        cachePrefs.end();
    }
    loadDefaultsIfEmpty();
}

bool connectWiFi(const WatchConfig::Config& cfg) {
    if (cfg.wifiSsid[0] == '\0') {
        safeCopy(statusMsg, "WiFi SSID not set", sizeof(statusMsg));
        DebugLog::log("NET: WiFi SSID empty; configure via Hotspot");
        return false;
    }

    safeCopy(statusMsg, "Connecting WiFi...", sizeof(statusMsg));
    DebugLog::log("NET: Connecting to '%s'", cfg.wifiSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 9000) {
        delay(200);
    }

    if (WiFi.status() != WL_CONNECTED) {
        safeCopy(statusMsg, "WiFi Connect Failed", sizeof(statusMsg));
        DebugLog::log("NET: WiFi connect timeout");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    DebugLog::log("NET: WiFi connected, IP=%s", WiFi.localIP().toString().c_str());
    return true;
}

void disconnectWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    DebugLog::log("NET: WiFi turned off (power save)");
}

String buildCalDavUrl(const WatchConfig::Config& cfg, const char* calendarName, bool encodeAt = false) {
    String s = String(cfg.caldavServer);
    s.trim();
    if (s.isEmpty()) return "";

    if (s.indexOf("?export") != -1) {
        return s;
    }

    while (s.endsWith("/")) s.remove(s.length() - 1);

    if (s.indexOf("/calendars/") != -1) {
        return s + "?export";
    }

    if (s.indexOf("/remote.php/dav") == -1) {
        s += "/remote.php/dav";
    }

    if (cfg.caldavUser[0] != '\0') {
        s += "/calendars/";
        String u = String(cfg.caldavUser);
        if (encodeAt) u.replace("@", "%40");
        s += u;
        s += "/";
        s += (calendarName && calendarName[0] != '\0') ? calendarName : "personal";
        s += "?export";
    }

    return s;
}

bool fetchAndParseIcs(WiFiClientSecure& client, HTTPClient& https, const String& url,
                      const WatchConfig::Config& cfg,
                      size_t& outEvents, size_t& outTodos) {
    if (url.isEmpty()) return false;
    DebugLog::log("NET: Fetching ICS from: %s", url.c_str());

    if (!https.begin(client, url)) {
        DebugLog::log("NET: HTTPClient begin failed");
        return false;
    }

    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (cfg.caldavUser[0] != '\0' && cfg.caldavPass[0] != '\0') {
        https.setAuthorization(cfg.caldavUser, cfg.caldavPass);
    }
    https.setTimeout(8000);

    const int code = https.GET();
    DebugLog::log("NET: ICS GET code=%d", code);

    if (code != 200) {
        https.end();
        return false;
    }

    WiFiClient* stream = https.getStreamPtr();
    bool inEvent = false;
    bool inTodo = false;
    char curSummary[48] = "";
    char curDt[24] = "";
    bool isCompleted = false;

    while (https.connected() && stream->available()) {
        String line = stream->readStringUntil('\n');
        line.trim();

        if (line == "BEGIN:VEVENT") {
            inEvent = true;
            inTodo = false;
            curSummary[0] = '\0';
            curDt[0] = '\0';
        } else if (line == "BEGIN:VTODO") {
            inTodo = true;
            inEvent = false;
            curSummary[0] = '\0';
            isCompleted = false;
        } else if (line.startsWith("SUMMARY")) {
            int colon = line.indexOf(':');
            if (colon != -1) {
                safeCopy(curSummary, line.substring(colon + 1).c_str(), sizeof(curSummary));
            }
        } else if (inEvent && line.startsWith("DTSTART")) {
            int colon = line.indexOf(':');
            if (colon != -1) {
                safeCopy(curDt, line.substring(colon + 1).c_str(), sizeof(curDt));
            }
        } else if (inTodo && (line.indexOf("STATUS:COMPLETED") != -1 || line.startsWith("COMPLETED:"))) {
            isCompleted = true;
        } else if (line == "END:VEVENT") {
            if (inEvent && curSummary[0] != '\0' && outEvents < MAX_EVENTS) {
                safeCopy(events[outEvents].title, curSummary, sizeof(events[0].title));
                if (strlen(curDt) >= 13) {
                    snprintf(events[outEvents].timeStr, sizeof(events[0].timeStr),
                             "%.2s:%.2s", curDt + 9, curDt + 11);
                } else {
                    safeCopy(events[outEvents].timeStr, "Today", sizeof(events[0].timeStr));
                }
                DebugLog::log("NET: Event [%u] '%s' @ %s",
                              unsigned(outEvents), events[outEvents].title, events[outEvents].timeStr);
                ++outEvents;
            }
            inEvent = false;
            curSummary[0] = '\0';
            curDt[0] = '\0';
        } else if (line == "END:VTODO") {
            if (inTodo && curSummary[0] != '\0' && outTodos < MAX_TODOS) {
                safeCopy(todos[outTodos].title, curSummary, sizeof(todos[0].title));
                todos[outTodos].completed = isCompleted;
                DebugLog::log("NET: Todo [%u] '%s' (%s)",
                              unsigned(outTodos), todos[outTodos].title,
                              isCompleted ? "DONE" : "OPEN");
                ++outTodos;
            }
            inTodo = false;
            curSummary[0] = '\0';
            isCompleted = false;
        }
    }

    https.end();
    return true;
}
} // namespace

void begin() {
    loadCache();
}

size_t eventCount() { return numEvents; }
const CalEvent& getEvent(size_t index) { return events[index < numEvents ? index : 0]; }

size_t todoCount() { return numTodos; }
const CalTodo& getTodo(size_t index) { return todos[index < numTodos ? index : 0]; }

void toggleTodo(size_t index) {
    if (index < numTodos) {
        todos[index].completed = !todos[index].completed;
        saveCache();
        DebugLog::log("TODO [%u] '%s' -> %s", unsigned(index), todos[index].title,
                      todos[index].completed ? "DONE" : "OPEN");
    }
}

bool isSyncing() { return syncing; }
const char* lastStatus() { return statusMsg; }

bool syncNtp() {
    const auto& cfg = WatchConfig::get();
    syncing = true;

    if (!connectWiFi(cfg)) {
        syncing = false;
        return false;
    }

    safeCopy(statusMsg, "Syncing NTP...", sizeof(statusMsg));
    DebugLog::log("NET: Requesting NTP time (tz offset %d min)", cfg.timezoneOffsetMin);
    configTime(0, 0, "pool.ntp.org", "time.google.com");

    const uint32_t startMs = millis();
    time_t now = 0;
    while ((millis() - startMs) < 6000) {
        now = time(nullptr);
        if (now > 1700000000) break; // Valid epoch (post-2023)
        delay(250);
    }

    bool success = false;
    if (now > 1700000000) {
        const uint32_t localEpoch = static_cast<uint32_t>((int64_t)now + ((int64_t)cfg.timezoneOffsetMin * 60));
        WatchClock::setEpoch(localEpoch);
        safeCopy(statusMsg, "NTP Time Synced", sizeof(statusMsg));
        DebugLog::log("NET: NTP sync SUCCESS utc=%lu local=%lu (tzOffset=%d min)",
                      (unsigned long)now, (unsigned long)localEpoch, cfg.timezoneOffsetMin);
        success = true;
    } else {
        safeCopy(statusMsg, "NTP Timeout", sizeof(statusMsg));
        DebugLog::log("NET: NTP sync timeout");
    }

    disconnectWiFi();
    syncing = false;
    return success;
}

bool syncAll() {
    const auto& cfg = WatchConfig::get();
    syncing = true;

    if (!connectWiFi(cfg)) {
        syncing = false;
        return false;
    }

    // 1. Sync NTP time
    safeCopy(statusMsg, "Syncing NTP...", sizeof(statusMsg));
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    const uint32_t startMs = millis();
    time_t now = 0;
    while ((millis() - startMs) < 6000) {
        now = time(nullptr);
        if (now > 1700000000) break;
        delay(250);
    }
    if (now > 1700000000) {
        const uint32_t localEpoch = static_cast<uint32_t>((int64_t)now + ((int64_t)cfg.timezoneOffsetMin * 60));
        WatchClock::setEpoch(localEpoch);
        DebugLog::log("NET: NTP synced utc=%lu local=%lu (tzOffset=%d min)",
                      (unsigned long)now, (unsigned long)localEpoch, cfg.timezoneOffsetMin);
    }

    // 2. Sync CalDAV (if server URL configured)
    if (cfg.caldavServer[0] != '\0') {
        safeCopy(statusMsg, "Querying CalDAV...", sizeof(statusMsg));
        DebugLog::log("NET: Querying CalDAV (srv='%s', user='%s', cal='%s', todo='%s')",
                      cfg.caldavServer, cfg.caldavUser, cfg.caldavCalendar, cfg.caldavTodoPath);

        WiFiClientSecure client;
        client.setInsecure(); // Skip TLS cert validation to save flash/RAM
        HTTPClient https;

        size_t parsedEvents = 0;
        size_t parsedTodos = 0;

        // Step A: Fetch events calendar
        String eventsUrl = buildCalDavUrl(cfg, cfg.caldavCalendar, false);
        bool ok = fetchAndParseIcs(client, https, eventsUrl, cfg, parsedEvents, parsedTodos);
        if (!ok && eventsUrl.indexOf("@") != -1) {
            String retryUrl = buildCalDavUrl(cfg, cfg.caldavCalendar, true);
            ok = fetchAndParseIcs(client, https, retryUrl, cfg, parsedEvents, parsedTodos);
        }

        // Step B: Fetch tasks calendar if different from events calendar
        if (cfg.caldavTodoPath[0] != '\0' && strcmp(cfg.caldavCalendar, cfg.caldavTodoPath) != 0) {
            String tasksUrl = buildCalDavUrl(cfg, cfg.caldavTodoPath, false);
            bool ok2 = fetchAndParseIcs(client, https, tasksUrl, cfg, parsedEvents, parsedTodos);
            if (!ok2 && tasksUrl.indexOf("@") != -1) {
                String retryTasksUrl = buildCalDavUrl(cfg, cfg.caldavTodoPath, true);
                fetchAndParseIcs(client, https, retryTasksUrl, cfg, parsedEvents, parsedTodos);
            }
        }

        if (parsedEvents > 0 || parsedTodos > 0) {
            numEvents = parsedEvents;
            numTodos = parsedTodos;
            saveCache();
            snprintf(statusMsg, sizeof(statusMsg), "Synced %u ev, %u todo",
                     (unsigned)numEvents, (unsigned)numTodos);
            DebugLog::log("NET: CalDAV sync SUCCESS: %u events, %u todos",
                          (unsigned)numEvents, (unsigned)numTodos);
        } else {
            snprintf(statusMsg, sizeof(statusMsg), "CalDAV: 0 items parsed");
            DebugLog::log("NET: CalDAV returned 0 events/todos");
        }
    } else {
        safeCopy(statusMsg, "NTP OK (No CalDAV URL)", sizeof(statusMsg));
    }

    disconnectWiFi();
    syncing = false;
    return true;
}

} // namespace NetSync
