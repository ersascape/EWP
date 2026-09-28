#include "net_sync.h"
#include "watch_config.h"
#include "watch_clock.h"
#include "debug_log.h"
#include "ersa/config/system_defaults.h"
#include "ersa/config/ui_strings.h"
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
constexpr const char* CACHE_NS = ersa::config::PREFS_NS_CACHE;

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
        const DateTime now = WatchClock::now();
        cachePrefs.putBytes("events", events, sizeof(events));
        cachePrefs.putUChar("ev_cnt", (uint8_t)numEvents);
        cachePrefs.putUShort("ev_yr", now.year());
        cachePrefs.putUChar("ev_mo", now.month());
        cachePrefs.putUChar("ev_dy", now.day());
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
            const DateTime now = WatchClock::now();
            uint16_t cachedYr = cachePrefs.getUShort("ev_yr", 0);
            uint8_t cachedMo = cachePrefs.getUChar("ev_mo", 0);
            uint8_t cachedDy = cachePrefs.getUChar("ev_dy", 0);

            // Only restore cached events if they are strictly for today!
            if (cachedYr == now.year() && cachedMo == now.month() && cachedDy == now.day()) {
                numEvents = cachePrefs.getUChar("ev_cnt", 0);
                if (numEvents > MAX_EVENTS) numEvents = 0;
                if (numEvents > 0) {
                    cachePrefs.getBytes("events", events, sizeof(events));
                }
            } else {
                numEvents = 0;
                DebugLog::log("NET: Cached events expired (cached %04u-%02u-%02u vs today %04u-%02u-%02u)",
                              cachedYr, cachedMo, cachedDy, now.year(), now.month(), now.day());
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
        safeCopy(statusMsg, ersa::strings::MSG_WIFI_NO_SSID, sizeof(statusMsg));
        DebugLog::log("NET: WiFi SSID empty; configure via Hotspot");
        return false;
    }

    safeCopy(statusMsg, ersa::strings::MSG_WIFI_CONNECTING, sizeof(statusMsg));
    DebugLog::log("NET: Connecting to '%s'", cfg.wifiSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < ersa::config::WIFI_CONNECT_TIMEOUT_MS) {
        delay(200);
    }

    if (WiFi.status() != WL_CONNECTED) {
        safeCopy(statusMsg, ersa::strings::MSG_WIFI_FAILED, sizeof(statusMsg));
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
                    bool expired = false;
                    const char* untilPtr = strstr(curRrule, "UNTIL=");
                    if (untilPtr) {
                        uint32_t untilEpoch = parseIcsDateTimeToEpoch(untilPtr + 6, cfg.timezoneOffsetMin);
                        if (untilEpoch > 0 && untilEpoch < dayStartSec) {
                            expired = true;
                        }
                    }

                    if (!expired) {
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

time_t parseHttpDateToEpoch(const char* str) {
    if (!str || strlen(str) < 16) return 0;

    // Format: "Mon, 28 Sep 2026 06:21:00 GMT" or "28 Sep 2026 06:21:00 GMT"
    const char* p = strchr(str, ',');
    p = p ? (p + 1) : str;

    while (*p == ' ') p++;
    int day = atoi(p);
    if (day < 1 || day > 31) return 0;

    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;

    static const char* const months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int month = 0;
    for (int m = 0; m < 12; ++m) {
        if (strncasecmp(p, months[m], 3) == 0) {
            month = m + 1;
            break;
        }
    }
    if (month == 0) return 0;

    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;

    int year = atoi(p);
    if (year < 2024 || year > 2099) return 0;

    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;

    int hour = atoi(p);
    p = strchr(p, ':');
    if (!p) return 0;
    int min = atoi(p + 1);
    p = strchr(p + 1, ':');
    if (!p) return 0;
    int sec = atoi(p + 1);

    DateTime dt(year, month, day, hour, min, sec);
    return dt.unixtime();
}

bool fetchHttpUtc(time_t& outUtc, uint32_t timeoutMs = ersa::config::HTTP_TIME_TIMEOUT_MS) {
    HTTPClient http;
    const char* headerKeys[] = {"Date"};

    for (size_t i = 0; i < ersa::config::NUM_HTTP_TIME_ENDPOINTS; ++i) {
        const char* endpoint = ersa::config::DEFAULT_HTTP_TIME_ENDPOINTS[i];
        DebugLog::log("NET: Trying HTTP time fallback [%u]: %s", unsigned(i), endpoint);

        if (!http.begin(endpoint)) continue;
        http.setTimeout(timeoutMs);
        http.collectHeaders(headerKeys, 1);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        const int httpCode = http.GET();
        if (httpCode > 0) {
            if (http.hasHeader("Date")) {
                String dateHdr = http.header("Date");
                DebugLog::log("NET: HTTP %s returned code %d, Date: '%s'", endpoint, httpCode, dateHdr.c_str());
                time_t parsedUtc = parseHttpDateToEpoch(dateHdr.c_str());
                if (parsedUtc >= 1700000000) {
                    outUtc = parsedUtc;
                    http.end();
                    return true;
                }
            }

            // Check JSON for unixtime
            if (httpCode == 200 && http.getSize() > 0) {
                String body = http.getString();
                int idx = body.indexOf("\"unixtime\":");
                if (idx != -1) {
                    const char* numPtr = body.c_str() + idx + 11;
                    while (*numPtr == ' ') numPtr++;
                    uint32_t unixTime = strtoul(numPtr, nullptr, 10);
                    if (unixTime >= 1700000000) {
                        outUtc = unixTime;
                        http.end();
                        return true;
                    }
                }
            }
        }
        http.end();
    }
    return false;
}

static volatile bool s_sntpSynced = false;
void sntpTimeSyncNotification(struct timeval* tv) {
    (void)tv;
    s_sntpSynced = true;
    DebugLog::log("NET: SNTP packet received and processed");
}

bool fetchNtpUtc(time_t& outUtc, uint32_t timeoutMs = ersa::config::NTP_SYNC_TIMEOUT_MS) {
    s_sntpSynced = false;
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    for (size_t i = 0; i < ersa::config::NUM_NTP_SERVERS && i < 3; ++i) {
        esp_sntp_setservername(i, ersa::config::DEFAULT_NTP_SERVERS[i]);
        DebugLog::log("NET: Set NTP server[%u] = %s", unsigned(i), ersa::config::DEFAULT_NTP_SERVERS[i]);
    }
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    sntp_set_time_sync_notification_cb(sntpTimeSyncNotification);
    esp_sntp_init();

    const uint32_t startMs = millis();
    while ((millis() - startMs) < timeoutMs) {
        if (s_sntpSynced || sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            outUtc = time(nullptr);
            if (outUtc >= 1700000000) return true;
        }
        time_t tNow = time(nullptr);
        if (tNow >= 1700000000) {
            outUtc = tNow;
            return true;
        }
        delay(100);
    }
    return false;
}

bool fetchTimeWithFallbacks(time_t& outUtc, bool& isHttpFallback) {
    isHttpFallback = false;
    safeCopy(statusMsg, ersa::strings::MSG_SYNCING_NTP, sizeof(statusMsg));

    DebugLog::log("NET: Step 1: Trying SNTP pool sync (UDP port 123)...");
    if (fetchNtpUtc(outUtc, ersa::config::NTP_SYNC_TIMEOUT_MS)) {
        DebugLog::log("NET: Primary SNTP sync SUCCESS (utc=%lu)", (unsigned long)outUtc);
        return true;
    }

    DebugLog::log("NET: SNTP sync timed out/blocked; Step 2: Falling back to HTTP Time endpoints (TCP port 80)...");
    if (fetchHttpUtc(outUtc, ersa::config::HTTP_TIME_TIMEOUT_MS)) {
        isHttpFallback = true;
        DebugLog::log("NET: HTTP Time fallback SUCCESS (utc=%lu)", (unsigned long)outUtc);
        return true;
    }

    DebugLog::log("NET: All network time synchronization methods failed");
    return false;
}

bool syncNtp() {
    const auto& cfg = WatchConfig::get();
    syncing = true;

    if (!connectWiFi(cfg)) {
        syncing = false;
        return false;
    }

    time_t utcEpoch = 0;
    bool isHttpFallback = false;
    bool success = fetchTimeWithFallbacks(utcEpoch, isHttpFallback);
    if (success) {
        const uint32_t localEpoch = static_cast<uint32_t>((int64_t)utcEpoch + ((int64_t)cfg.timezoneOffsetMin * 60));
        WatchClock::setEpoch(localEpoch);
        safeCopy(statusMsg, isHttpFallback ? ersa::strings::MSG_HTTP_TIME_SYNCED : ersa::strings::MSG_NTP_SYNCED, sizeof(statusMsg));
        DebugLog::log("NET: Time sync SUCCESS (method=%s, utc=%lu, local=%lu, tzOffset=%d min)",
                      isHttpFallback ? "HTTP" : "SNTP",
                      (unsigned long)utcEpoch, (unsigned long)localEpoch, cfg.timezoneOffsetMin);
    } else {
        safeCopy(statusMsg, ersa::strings::MSG_TIME_SYNC_FAILED, sizeof(statusMsg));
        DebugLog::log("NET: Time sync failed");
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

    // 1. Sync time with multi-tier fallbacks
    time_t utcEpoch = 0;
    bool isHttpFallback = false;
    if (fetchTimeWithFallbacks(utcEpoch, isHttpFallback)) {
        const uint32_t localEpoch = static_cast<uint32_t>((int64_t)utcEpoch + ((int64_t)cfg.timezoneOffsetMin * 60));
        WatchClock::setEpoch(localEpoch);
        DebugLog::log("NET: Time synced (method=%s, utc=%lu, local=%lu, tzOffset=%d min)",
                      isHttpFallback ? "HTTP" : "SNTP",
                      (unsigned long)utcEpoch, (unsigned long)localEpoch, cfg.timezoneOffsetMin);
    } else {
        DebugLog::log("NET: Time sync failed in syncAll; keeping RTC time");
    }

    // 2. Sync CalDAV (if server URL configured)
    if (cfg.caldavServer[0] != '\0') {
        safeCopy(statusMsg, ersa::strings::MSG_QUERYING_CALDAV, sizeof(statusMsg));
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
            safeCopy(statusMsg, ersa::strings::MSG_CALDAV_FAILED, sizeof(statusMsg));
            DebugLog::log("NET: CalDAV HTTP request failed");
        }
    } else {
        safeCopy(statusMsg, ersa::strings::MSG_SYNC_COMPLETE, sizeof(statusMsg));
    }

    disconnectWiFi();
    syncing = false;
    return true;
}

} // namespace NetSync
