#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "ersa/common/types.h"
#include "ersa/events/event_bus.h"
#include "ersa/app/application_manager.h"
#include "ersa/app/watchface.h"
#include "ersa/app/complication.h"
#include "ersa/services/time_service.h"
#include "ersa/services/power_manager.h"
#include "ersa/services/display_manager.h"
#include "ersa/services/network_manager.h"
#include "ersa/services/storage_service.h"
#include "ersa/services/settings_service.h"
#include "ersa/services/logging_service.h"
#include "ersa/system.h"
#include "mocks/mock_display.h"
#include "mocks/mock_rtc.h"
#include "mocks/mock_battery.h"

using namespace ersa;

static int testsPassed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAILED: %s (line %d): %s\n", __func__, __LINE__, msg); \
            assert(cond); \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        testsPassed++; \
        printf("PASS: %s\n", __func__); \
    } while(0)

// -----------------------------------------------------------------------------
// Test EventBus with EventType
// -----------------------------------------------------------------------------
static int g_subCallCount = 0;
static events::Event g_lastReceivedEvent;

static void onTestEvent(const events::Event& event, void* userData) {
    (void)userData;
    g_subCallCount++;
    g_lastReceivedEvent = event;
}

void test_event_bus() {
    events::EventBus bus;
    g_subCallCount = 0;

    // 1. Subscribe to ButtonClicked
    events::SubscriptionId sub1 = bus.subscribe(events::EventType::ButtonClicked, onTestEvent);
    TEST_ASSERT(sub1 != 0, "Subscription failed");

    // 2. Publish MinuteTick (should NOT trigger sub1)
    events::TimePayload tp{100, 2026, 9, 28, 12, 0, 0};
    bus.publish(events::Event::createMinuteTick(tp, 1000));
    TEST_ASSERT(g_subCallCount == 0, "Button subscriber should not receive time event");

    // 3. Publish ButtonClicked (should trigger sub1)
    bus.publish(events::Event::createButton(events::EventType::ButtonClicked, events::ButtonId::Button1, 1050));
    TEST_ASSERT(g_subCallCount == 1, "Button subscriber should receive event");
    TEST_ASSERT(g_lastReceivedEvent.type == events::EventType::ButtonClicked, "Type match");
    TEST_ASSERT(g_lastReceivedEvent.button.button == events::ButtonId::Button1, "Button match");

    // 4. Test Queue and dispatch
    bus.post(events::Event::createButton(events::EventType::ButtonClicked, events::ButtonId::Button2, 2000));
    TEST_ASSERT(g_subCallCount == 1, "Queued event should not immediately trigger");
    size_t processed = bus.dispatchQueue();
    TEST_ASSERT(processed == 1, "Processed 1 queued event");
    TEST_ASSERT(g_subCallCount == 2, "Subscriber called after dispatchQueue");
    TEST_ASSERT(g_lastReceivedEvent.button.button == events::ButtonId::Button2, "Button 2 match");

    // 5. Unsubscribe
    bool unsub = bus.unsubscribe(sub1);
    TEST_ASSERT(unsub, "Unsubscribe succeeded");
    bus.publish(events::Event::createButton(events::EventType::ButtonClicked, events::ButtonId::Button1, 3000));
    TEST_ASSERT(g_subCallCount == 2, "Subscriber should not be called after unsubscribing");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test Application & ApplicationManager Lifecycle
// -----------------------------------------------------------------------------
class TestAppA : public app::Application {
public:
    int createCount{0};
    int startCount{0};
    int resumeCount{0};
    int pauseCount{0};
    int stopCount{0};
    int eventCount{0};
    int renderCount{0};

    const char* getId() const override { return "app_a"; }
    const char* getTitle() const override { return "Application A"; }

    void onCreate() override { createCount++; }
    void onStart() override { startCount++; }
    void onResume() override { resumeCount++; }
    void onPause() override { pauseCount++; }
    void onStop() override { stopCount++; }

    void onEvent(const events::Event& event) override {
        (void)event;
        eventCount++;
    }

    void render(hal::IDisplay& display, bool full) override {
        (void)full;
        renderCount++;
        display.fillRect(0, 0, 10, 10, hal::Color::White);
    }
};

class TestAppB : public app::Application {
public:
    int createCount{0};
    int startCount{0};
    int resumeCount{0};

    const char* getId() const override { return "app_b"; }
    const char* getTitle() const override { return "Application B"; }

    void onCreate() override { createCount++; }
    void onStart() override { startCount++; }
    void onResume() override { resumeCount++; }
};

void test_application_manager() {
    app::ApplicationManager mgr;
    test::MockDisplay display;

    TestAppA appA;
    TestAppB appB;

    // 1. Register apps (triggers onCreate)
    TEST_ASSERT(mgr.registerApp(&appA), "App A registered");
    TEST_ASSERT(appA.createCount == 1, "App A onCreate called");
    TEST_ASSERT(appA.startCount == 1, "App A onStart called as first app");
    TEST_ASSERT(appA.resumeCount == 1, "App A onResume called as first app");

    TEST_ASSERT(mgr.registerApp(&appB), "App B registered");
    TEST_ASSERT(appB.createCount == 1, "App B onCreate called");
    TEST_ASSERT(appB.startCount == 0, "App B not started yet");
    TEST_ASSERT(mgr.getAppCount() == 2, "Two apps registered");

    // 2. Event routing
    events::Event evt = events::Event::createButton(events::EventType::ButtonClicked, events::ButtonId::Button1);
    TEST_ASSERT(mgr.handleEvent(evt), "App handled event");
    TEST_ASSERT(appA.eventCount == 1, "Event reached active App A");
    TEST_ASSERT(mgr.isDirty(), "App marked dirty after event");

    // 3. Render
    mgr.render(display);
    TEST_ASSERT(appA.renderCount == 1, "App A rendered");
    TEST_ASSERT(!mgr.isDirty(), "Manager not dirty after render");
    TEST_ASSERT(display.getPixel(5, 5) == hal::Color::White, "Pixel drawn by app");

    // 4. Switch app (appA pauses/stops, appB starts/resumes)
    TEST_ASSERT(mgr.switchTo("app_b"), "Switched to App B");
    TEST_ASSERT(appA.pauseCount == 1, "App A paused");
    TEST_ASSERT(appA.stopCount == 1, "App A stopped");
    TEST_ASSERT(appB.startCount == 1, "App B started");
    TEST_ASSERT(appB.resumeCount == 1, "App B resumed");
    TEST_ASSERT(mgr.getActiveApp() == &appB, "App B is now active");
    TEST_ASSERT(mgr.isAppSwitched(), "App switch flag is set");

    // 5. Render after app switch clears the switch flag
    mgr.render(display);
    TEST_ASSERT(!mgr.isAppSwitched(), "App switch flag cleared after render");

    // 6. Test sub-window partial refresh within same app
    mgr.handleEvent(evt); // marks dirty without app switch
    const uint32_t partialsBefore = display.partialRefreshes_;
    mgr.render(display);
    TEST_ASSERT(display.partialRefreshes_ == partialsBefore + 1, "Partial refresh executed");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test TimeService
// -----------------------------------------------------------------------------
static int g_timeEventCount = 0;
static void onTimeEvent(const events::Event& event, void* userData) {
    (void)userData;
    if (event.type == events::EventType::MinuteTick) {
        g_timeEventCount++;
    }
}

void test_time_service() {
    events::EventBus bus;
    bus.subscribe(events::EventType::MinuteTick, onTimeEvent);
    g_timeEventCount = 0;

    test::MockRtc rtc(1000000020); // Aligned minute boundary
    services::TimeService timeService(rtc, bus);

    TEST_ASSERT(timeService.init().isOk(), "TimeService init ok");
    TEST_ASSERT(timeService.isRtcHealthy(), "Mock RTC healthy");

    // Initial tick
    timeService.tick(0);
    TEST_ASSERT(g_timeEventCount == 0, "No minute tick yet");

    // Advance 30 seconds: same minute
    rtc.advanceSeconds(30);
    timeService.tick(1500);
    TEST_ASSERT(g_timeEventCount == 0, "Still no minute tick");

    // Advance past minute boundary: should trigger MinuteTick on EventBus
    rtc.advanceSeconds(35);
    timeService.tick(2500);
    TEST_ASSERT(g_timeEventCount == 1, "Minute tick event dispatched on EventBus");

    // Test timezone offset
    timeService.setTimezoneOffset(330); // UTC+5:30
    TEST_ASSERT(timeService.getTimezoneOffset() == 330, "Timezone offset set");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test PowerManager & WakeLock RAII
// -----------------------------------------------------------------------------
void test_power_manager() {
    events::EventBus bus;
    test::MockBattery battery(3800, 60, true, false);
    services::PowerManager power(battery, bus);

    TEST_ASSERT(power.init().isOk(), "PowerManager init");
    TEST_ASSERT(power.getBatteryMv() == 3800, "Voltage matches");
    TEST_ASSERT(power.getBatteryPercent() == 60, "Percentage matches");
    TEST_ASSERT(power.isBatteryConnected(), "Battery connected");
    TEST_ASSERT(!power.isCharging(), "Not charging");

    // WakeLock RAII tests
    services::PowerManager::setInstance(&power);
    TEST_ASSERT(!power.hasWakeLocks(), "No wake locks initially");

    {
        services::WakeLock lock1 = power.acquireWakeLock("ntp-sync");
        TEST_ASSERT(power.hasWakeLocks(), "Has wake lock inside scope");
        TEST_ASSERT(power.getActiveWakeLockCount() == 1, "1 wake lock active");

        {
            services::WakeLock lock2 = power.acquireWakeLock("screen-render");
            TEST_ASSERT(power.getActiveWakeLockCount() == 2, "2 wake locks active");
        }
        TEST_ASSERT(power.getActiveWakeLockCount() == 1, "lock2 released on scope exit");
    }
    TEST_ASSERT(!power.hasWakeLocks(), "All wake locks released automatically via RAII");

    // Activity tracking
    power.noteActivity(5000);
    TEST_ASSERT(power.getIdleTimeMs(5000) == 0, "Zero idle right after activity");
    TEST_ASSERT(power.getIdleTimeMs(12000) == 7000, "Idle time computed correctly");

    // Sleep tests
    TEST_ASSERT(power.canSleep(), "Can sleep without wake locks");
    {
        services::WakeLock lock = power.acquireWakeLock("wifi");
        TEST_ASSERT(!power.canSleep(), "Cannot sleep while wake lock held");
    }
    TEST_ASSERT(power.canSleep(), "Can sleep after wake lock released");

    power.enterLightSleep(1000000);
    TEST_ASSERT(power.getState() == services::PowerState::LightSleep, "State changed to LightSleep");

    power.enterDeepSleep(0);
    TEST_ASSERT(power.getState() == services::PowerState::DeepSleep, "State changed to DeepSleep");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test DisplayManager
// -----------------------------------------------------------------------------
void test_display_manager() {
    test::MockDisplay display;
    services::DisplayManager displayMgr(display);

    TEST_ASSERT(displayMgr.init().isOk(), "DisplayManager init");
    TEST_ASSERT(displayMgr.isDirty(), "Dirty after init");

    // First refresh should be full refresh
    displayMgr.refresh(true, 100);
    TEST_ASSERT(display.fullRefreshes_ == 1, "First refresh was full");
    TEST_ASSERT(!displayMgr.isDirty(), "Clean after refresh");

    // Subsequent partial refresh
    displayMgr.markDirty(false);
    TEST_ASSERT(displayMgr.isDirty(), "Dirty marked");
    displayMgr.refresh(false, 700);
    TEST_ASSERT(display.partialRefreshes_ == 1, "Partial refresh executed");
    TEST_ASSERT(displayMgr.getPartialFrameCount() == 1, "Partial frame count incremented");

    // Throttling: updateIfDirty fails if interval < 500ms
    displayMgr.markDirty(false);
    bool updatedEarly = displayMgr.updateIfDirty(800); // only 100ms since 700
    TEST_ASSERT(!updatedEarly, "Throttled: did not refresh within 100ms");

    // After 500ms elapsed:
    bool updatedOnTime = displayMgr.updateIfDirty(1300); // 600ms elapsed
    TEST_ASSERT(updatedOnTime, "Updated after min refresh interval");
    TEST_ASSERT(display.partialRefreshes_ == 2, "Second partial refresh count");

    // Idle power off after 8000ms
    displayMgr.noteActivity(1300);
    displayMgr.tick(2000); // 700ms idle
    TEST_ASSERT(display.isPowered(), "Display still powered");

    displayMgr.tick(10000); // 8700ms idle
    TEST_ASSERT(!display.isPowered(), "Display powered off after idle timeout");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test NetworkManager & NetworkHandle RAII
// -----------------------------------------------------------------------------
void test_network_manager() {
    events::EventBus bus;
    services::NetworkManager netMgr(bus);
    services::NetworkManager::setInstance(&netMgr);

    TEST_ASSERT(netMgr.init().isOk(), "NetworkManager init");
    TEST_ASSERT(netMgr.getActiveHandleCount() == 0, "No active handles");

    {
        services::NetworkHandle handle1 = netMgr.requestInternet();
        TEST_ASSERT(handle1.isValid(), "Handle 1 valid");
        TEST_ASSERT(netMgr.getActiveHandleCount() == 1, "Active handles = 1");

        {
            services::NetworkHandle handle2 = netMgr.requestInternet();
            TEST_ASSERT(netMgr.getActiveHandleCount() == 2, "Active handles = 2");
        }
        TEST_ASSERT(netMgr.getActiveHandleCount() == 1, "Handle 2 released on scope exit");
    }
    TEST_ASSERT(netMgr.getActiveHandleCount() == 0, "All network handles released via RAII");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test StorageService
// -----------------------------------------------------------------------------
void test_storage_service() {
    auto& storage = services::StorageService::instance();
    TEST_ASSERT(storage.init().isOk(), "Storage init");

    storage.setString("wifi.ssid", "Ersa_Network");
    TEST_ASSERT(storage.getString("wifi.ssid") == "Ersa_Network", "String match");

    storage.setInt("display.brightness", 85);
    TEST_ASSERT(storage.getInt("display.brightness") == 85, "Int match");

    storage.setBool("power.saver", true);
    TEST_ASSERT(storage.getBool("power.saver") == true, "Bool match");

    storage.remove("display.brightness");
    TEST_ASSERT(storage.getInt("display.brightness", 50) == 50, "Default returned after removal");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test SettingsService & System Facade
// -----------------------------------------------------------------------------
void test_settings_service() {
    auto& settings = system::settings();
    settings.setWifiSsid("MyAccessPoint");
    TEST_ASSERT(settings.getWifiSsid() == "MyAccessPoint", "Ssid match");

    settings.setTimezoneOffsetMin(330);
    TEST_ASSERT(settings.getTimezoneOffsetMin() == 330, "Timezone match");

    settings.setMilitaryTime(true);
    TEST_ASSERT(settings.isMilitaryTime() == true, "Military time match");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test Complications
// -----------------------------------------------------------------------------
class BatteryComplication : public app::ComplicationProvider {
public:
    app::ComplicationData get() override {
        return app::ComplicationData{"85%"};
    }
};

void test_complications() {
    BatteryComplication bComp;
    app::ComplicationData data = bComp.get();
    TEST_ASSERT(data.text == "85%", "Battery complication text match");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test System Config Defaults & HTTP Date Parsing Fallback
// -----------------------------------------------------------------------------
#include "ersa/config/system_defaults.h"
#include "ersa/config/ui_strings.h"

static time_t testParseHttpDateToEpoch(const char* str) {
    if (!str || strlen(str) < 16) return 0;
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

    // Basic days since 1970 calculation for host test verification
    int days = 0;
    for (int y = 1970; y < year; y++) {
        days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }
    static const int daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    days += daysBeforeMonth[month - 1];
    if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
        days += 1;
    }
    days += (day - 1);
    return static_cast<time_t>(days * 86400LL + hour * 3600LL + min * 60LL + sec);
}

void test_config_and_fallbacks() {
    // 1. Validate NTP server pool fallback counts
    TEST_ASSERT(config::NUM_NTP_SERVERS >= 3, "At least 3 NTP fallback servers configured");
    TEST_ASSERT(strcmp(config::DEFAULT_NTP_SERVERS[0], "pool.ntp.org") == 0, "Primary NTP server is pool.ntp.org");
    TEST_ASSERT(strcmp(config::DEFAULT_NTP_SERVERS[1], "time.google.com") == 0, "Secondary NTP server is time.google.com");
    TEST_ASSERT(strcmp(config::DEFAULT_NTP_SERVERS[2], "time.cloudflare.com") == 0, "Tertiary NTP server is time.cloudflare.com");

    // 2. Validate HTTP Time endpoints fallback counts
    TEST_ASSERT(config::NUM_HTTP_TIME_ENDPOINTS >= 3, "At least 3 HTTP fallback endpoints configured");
    TEST_ASSERT(strstr(config::DEFAULT_HTTP_TIME_ENDPOINTS[0], "google.com") != nullptr, "Primary HTTP fallback is Google");

    // 3. Validate centralized strings
    TEST_ASSERT(strlen(strings::MSG_READY) > 0, "MSG_READY non-empty");
    TEST_ASSERT(strlen(strings::MSG_NTP_SYNCED) > 0, "MSG_NTP_SYNCED non-empty");
    TEST_ASSERT(strlen(strings::MSG_HTTP_TIME_SYNCED) > 0, "MSG_HTTP_TIME_SYNCED non-empty");
    TEST_ASSERT(strlen(strings::NAV_DRAWER_FOOTER) > 0, "NAV_DRAWER_FOOTER non-empty");

    // 4. Validate HTTP Date header parser
    const char* testHeader = "Mon, 28 Sep 2026 06:21:00 GMT";
    time_t epoch = testParseHttpDateToEpoch(testHeader);
    TEST_ASSERT(epoch > 1700000000, "Epoch should be in modern range (> 2023)");

    // Test without day of week
    const char* testHeaderNoDow = "28 Sep 2026 06:21:00 GMT";
    time_t epochNoDow = testParseHttpDateToEpoch(testHeaderNoDow);
    TEST_ASSERT(epoch == epochNoDow, "Header without day-of-week should match");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Main Test Runner
// -----------------------------------------------------------------------------
int main() {
    printf("==================================================\n");
    printf("        Ersa Smartwatch Core Architecture Tests   \n");
    printf("==================================================\n");

    test_event_bus();
    test_application_manager();
    test_time_service();
    test_power_manager();
    test_display_manager();
    test_network_manager();
    test_storage_service();
    test_settings_service();
    test_complications();
    test_config_and_fallbacks();

    printf("\nAll %d test suites passed successfully!\n", testsPassed);
    return 0;
}
