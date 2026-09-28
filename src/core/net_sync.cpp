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
#include <ctype.h>
#include <esp_sntp.h>

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
    bool hasInitializedCache = false;
    if (cachePrefs.begin(CACHE_NS, true)) {
        hasInitializedCache = cachePrefs.isKey("ev_cnt");
        if (hasInitializedCache) {
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
        }
        cachePrefs.end();
    }
    // Only load setup guidance if the user has never synced before
    if (!hasInitializedCache) {
        loadDefaultsIfEmpty();
    }
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

uint32_t parseIcsDateTimeToEpoch(const char* dt, int tzOffsetMin) {
    if (!dt || strlen(dt) < 8) return 0;
    char yBuf[5] = {dt[0], dt[1], dt[2], dt[3], '\0'};
    char mBuf[3] = {dt[4], dt[5], '\0'};
    char dBuf[3] = {dt[6], dt[7], '\0'};
    uint16_t y = atoi(yBuf);
    uint8_t m = atoi(mBuf);
    uint8_t d = atoi(dBuf);
    uint8_t h = 0, min = 0, s = 0;
    bool isUtc = false;

    const char* t = strchr(dt, 'T');
    if (t && strlen(t) >= 5) {
        char hBuf[3] = {t[1], t[2], '\0'};
        char minBuf[3] = {t[3], t[4], '\0'};
        h = atoi(hBuf);
        min = atoi(minBuf);
        if (strlen(t) >= 7 && isdigit((unsigned char)t[5]) && isdigit((unsigned char)t[6])) {
            char sBuf[3] = {t[5], t[6], '\0'};
            s = atoi(sBuf);
        }
        if (strchr(t, 'Z')) isUtc = true;
    }

    if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) return 0;

    DateTime dtObj(y, m, d, h, min, s);
    uint32_t epoch = dtObj.unixtime();
    if (isUtc) {
        epoch = static_cast<uint32_t>((int64_t)epoch + ((int64_t)tzOffsetMin * 60));
    }
    return epoch;
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
    char curDtEnd[24] = "";
    char curRrule[48] = "";
    bool isCompleted = false;

    // Buffer tasks so open/pending tasks are prioritized first
    CalTodo openTodos[MAX_TODOS];
    size_t numOpen = 0;
    CalTodo doneTodos[MAX_TODOS];
    size_t numDone = 0;

    const DateTime now = WatchClock::now();
    DateTime dayStart(now.year(), now.month(), now.day(), 0, 0, 0);
    const uint32_t dayStartSec = dayStart.unixtime();
    const uint32_t dayEndSec = dayStartSec + 86400;

    while (https.connected() && stream->available()) {
        String line = stream->readStringUntil('\n');
        line.trim();

        if (line == "BEGIN:VEVENT") {
            inEvent = true;
            inTodo = false;
            curSummary[0] = '\0';
            curDt[0] = '\0';
            curDtEnd[0] = '\0';
            curRrule[0] = '\0';
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
        } else if (inEvent && line.startsWith("DTEND")) {
            int colon = line.indexOf(':');
            if (colon != -1) {
                safeCopy(curDtEnd, line.substring(colon + 1).c_str(), sizeof(curDtEnd));
            }
        } else if (inEvent && line.startsWith("RRULE:")) {
            safeCopy(curRrule, line.c_str(), sizeof(curRrule));
        } else if (inTodo && (line.indexOf("STATUS:COMPLETED") != -1 || line.startsWith("COMPLETED:"))) {
            isCompleted = true;
        } else if (line == "END:VEVENT") {
            if (inEvent && curSummary[0] != '\0' && outEvents < MAX_EVENTS) {
                uint32_t startEpoch = parseIcsDateTimeToEpoch(curDt, cfg.timezoneOffsetMin);
                uint32_t endEpoch = (curDtEnd[0] != '\0') ? parseIcsDateTimeToEpoch(curDtEnd, cfg.timezoneOffsetMin) : (startEpoch + 3600);
                if (endEpoch <= startEpoch) endEpoch = startEpoch + 1800;

                bool isToday = false;
                if (startEpoch < dayEndSec && endEpoch > dayStartSec) {
                    isToday = true;
                }

                // Check recurring rules (e.g. daily, weekly)
                if (!isToday && curRrule[0] != '\0' && startEpoch < dayEndSec) {
                    if (strstr(curRrule, "FREQ=DAILY")) {
                        isToday = true;
                    } else if (strstr(curRrule, "FREQ=WEEKLY")) {
                        static const char* const dowCodes[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
                        const char* todayCode = dowCodes[now.dayOfTheWeek() % 7];
                        const char* byDay = strstr(curRrule, "BYDAY=");
                        if (byDay) {
                            if (strstr(byDay, todayCode)) isToday = true;
                        } else {
                            DateTime origStart(startEpoch);
                            if (origStart.dayOfTheWeek() == now.dayOfTheWeek()) isToday = true;
                        }
                    }
                }

                if (isToday) {
                    safeCopy(events[outEvents].title, curSummary, sizeof(events[0].title));
                    if (strchr(curDt, 'T')) {
                        DateTime localStart(startEpoch);
                        snprintf(events[outEvents].timeStr, sizeof(events[0].timeStr),
                                 "%02u:%02u", localStart.hour(), localStart.minute());
                    } else {
                        safeCopy(events[outEvents].timeStr, "all day", sizeof(events[0].timeStr));
                    }
                    DebugLog::log("NET: Event for TODAY [%u] '%s' @ %s",
                                  unsigned(outEvents), events[outEvents].title, events[outEvents].timeStr);
                    ++outEvents;
                } else {
                    DebugLog::log("NET: Ignored non-today event '%s' (dt=%s)", curSummary, curDt);
                }
            }
            inEvent = false;
            curSummary[0] = '\0';
            curDt[0] = '\0';
            curDtEnd[0] = '\0';
            curRrule[0] = '\0';
        } else if (line == "END:VTODO") {
            if (inTodo && curSummary[0] != '\0') {
                if (!isCompleted) {
                    if (numOpen < MAX_TODOS) {
                        safeCopy(openTodos[numOpen].title, curSummary, sizeof(openTodos[0].title));
                        openTodos[numOpen].completed = false;
                        ++numOpen;
                    }
                } else {
                    if (numDone < MAX_TODOS) {
                        safeCopy(doneTodos[numDone].title, curSummary, sizeof(doneTodos[0].title));
                        doneTodos[numDone].completed = true;
                        ++numDone;
                    }
                }
            }
            inTodo = false;
            curSummary[0] = '\0';
            isCompleted = false;
        }
    }

    https.end();

    // Fill todos array with open tasks first, then completed tasks if space remains
    outTodos = 0;
    for (size_t i = 0; i < numOpen && outTodos < MAX_TODOS; ++i) {
        todos[outTodos++] = openTodos[i];
    }
    for (size_t i = 0; i < numDone && outTodos < MAX_TODOS; ++i) {
        todos[outTodos++] = doneTodos[i];
    }

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

bool fetchNtpUtc(time_t& outUtc, uint32_t timeoutMs = 8000) {
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    esp_sntp_init();

    const uint32_t startMs = millis();
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && (millis() - startMs) < timeoutMs) {
        delay(100);
    }

    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        outUtc = time(nullptr);
        return true;
    }
    return false;
}

bool syncNtp() {
    const auto& cfg = WatchConfig::get();
    syncing = true;

    if (!connectWiFi(cfg)) {
        syncing = false;
        return false;
    }

    safeCopy(statusMsg, "Syncing NTP...", sizeof(statusMsg));
    DebugLog::log("NET: Requesting NTP time (tz offset %d min)", cfg.timezoneOffsetMin);
    time_t utcEpoch = 0;
    bool success = fetchNtpUtc(utcEpoch, 8000);
    if (success) {
        const uint32_t localEpoch = static_cast<uint32_t>((int64_t)utcEpoch + ((int64_t)cfg.timezoneOffsetMin * 60));
        WatchClock::setEpoch(localEpoch);
        safeCopy(statusMsg, "NTP Time Synced", sizeof(statusMsg));
        DebugLog::log("NET: NTP sync SUCCESS utc=%lu local=%lu (tzOffset=%d min)",
                      (unsigned long)utcEpoch, (unsigned long)localEpoch, cfg.timezoneOffsetMin);
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

    // 1. Sync NTP time using guaranteed SNTP completed status
    safeCopy(statusMsg, "Syncing NTP...", sizeof(statusMsg));
    time_t utcEpoch = 0;
    if (fetchNtpUtc(utcEpoch, 8000)) {
        const uint32_t localEpoch = static_cast<uint32_t>((int64_t)utcEpoch + ((int64_t)cfg.timezoneOffsetMin * 60));
        WatchClock::setEpoch(localEpoch);
        DebugLog::log("NET: NTP synced utc=%lu local=%lu (tzOffset=%d min)",
                      (unsigned long)utcEpoch, (unsigned long)localEpoch, cfg.timezoneOffsetMin);
    } else {
        DebugLog::log("NET: NTP sync timeout in syncAll; keeping RTC time");
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
            size_t extraEvents = 0;
            size_t extraTodos = parsedTodos;
            bool ok2 = fetchAndParseIcs(client, https, tasksUrl, cfg, extraEvents, extraTodos);
            if (!ok2 && tasksUrl.indexOf("@") != -1) {
                String retryTasksUrl = buildCalDavUrl(cfg, cfg.caldavTodoPath, true);
                fetchAndParseIcs(client, https, retryTasksUrl, cfg, extraEvents, extraTodos);
            }
            parsedTodos = extraTodos;
        }

        if (ok) {
            numEvents = parsedEvents;
            numTodos = parsedTodos;
            saveCache();
            snprintf(statusMsg, sizeof(statusMsg), "Synced %u ev, %u todo",
                     (unsigned)numEvents, (unsigned)numTodos);
            DebugLog::log("NET: CalDAV sync SUCCESS: %u events for today, %u todos",
                          (unsigned)numEvents, (unsigned)numTodos);
        } else {
            snprintf(statusMsg, sizeof(statusMsg), "CalDAV HTTP Failed");
            DebugLog::log("NET: CalDAV HTTP request failed");
        }
    } else {
        safeCopy(statusMsg, "NTP OK (No CalDAV URL)", sizeof(statusMsg));
    }

    disconnectWiFi();
    syncing = false;
    return true;
}

} // namespace NetSync
