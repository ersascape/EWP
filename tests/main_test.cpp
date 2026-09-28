#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "ersa/common/types.h"
#include "ersa/events/event_bus.h"
#include "ersa/app/application_manager.h"
#include "ersa/services/time_service.h"
#include "ersa/services/power_manager.h"
#include "ersa/services/display_manager.h"
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
// Test EventBus
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

    // 1. Subscribe to input events
    events::SubscriptionId sub1 = bus.subscribe(events::EventCategory::Input, onTestEvent);
    TEST_ASSERT(sub1 != 0, "Subscription failed");

    // 2. Publish Time event (should NOT trigger sub1)
    events::TimeData td{100, 2026, 9, 28, 12, 0, 0};
    bus.publish(events::Event::createMinuteTick(td, 1000));
    TEST_ASSERT(g_subCallCount == 0, "Input subscriber should not receive time event");

    // 3. Publish Input event (should trigger sub1)
    bus.publish(events::Event::createInput(events::ButtonId::Button1, events::ButtonAction::Click, 1050));
    TEST_ASSERT(g_subCallCount == 1, "Input subscriber should receive input event");
    TEST_ASSERT(g_lastReceivedEvent.category == events::EventCategory::Input, "Category match");
    TEST_ASSERT(g_lastReceivedEvent.input.button == events::ButtonId::Button1, "Button match");
    TEST_ASSERT(g_lastReceivedEvent.input.action == events::ButtonAction::Click, "Action match");

    // 4. Test Queue and dispatch
    bus.post(events::Event::createInput(events::ButtonId::Button2, events::ButtonAction::LongPress, 2000));
    TEST_ASSERT(g_subCallCount == 1, "Queued event should not immediately trigger");
    size_t processed = bus.dispatchQueue();
    TEST_ASSERT(processed == 1, "Processed 1 queued event");
    TEST_ASSERT(g_subCallCount == 2, "Subscriber called after dispatchQueue");
    TEST_ASSERT(g_lastReceivedEvent.input.button == events::ButtonId::Button2, "Button 2 match");

    // 5. Unsubscribe
    bool unsub = bus.unsubscribe(sub1);
    TEST_ASSERT(unsub, "Unsubscribe succeeded");
    bus.publish(events::Event::createInput(events::ButtonId::Button1, events::ButtonAction::Click, 3000));
    TEST_ASSERT(g_subCallCount == 2, "Subscriber should not be called after unsubscribing");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test Application & ApplicationManager
// -----------------------------------------------------------------------------
class TestAppA : public app::Application {
public:
    int enterCount{0};
    int exitCount{0};
    int eventCount{0};
    int renderCount{0};

    const char* getId() const override { return "app_a"; }
    const char* getTitle() const override { return "Application A"; }

    void onEnter() override { enterCount++; }
    void onExit() override { exitCount++; }
    bool onEvent(const events::Event& event) override {
        (void)event;
        eventCount++;
        return true;
    }
    void render(hal::IDisplay& display, bool full) override {
        (void)full;
        renderCount++;
        display.fillRect(0, 0, 10, 10, hal::Color::White);
    }
};

class TestAppB : public app::Application {
public:
    int enterCount{0};
    int exitCount{0};

    const char* getId() const override { return "app_b"; }
    const char* getTitle() const override { return "Application B"; }

    void onEnter() override { enterCount++; }
    void onExit() override { exitCount++; }
    void render(hal::IDisplay& display, bool full) override {
        (void)display; (void)full;
    }
};

void test_application_manager() {
    app::ApplicationManager mgr;
    test::MockDisplay display;

    TestAppA appA;
    TestAppB appB;

    // 1. Register apps
    TEST_ASSERT(mgr.registerApp(&appA), "App A registered");
    TEST_ASSERT(mgr.registerApp(&appB), "App B registered");
    TEST_ASSERT(mgr.getAppCount() == 2, "Two apps registered");

    // First registered app becomes active automatically
    TEST_ASSERT(mgr.getActiveApp() == &appA, "App A active");
    TEST_ASSERT(appA.enterCount == 1, "App A onEnter called");

    // 2. Event routing
    events::Event evt = events::Event::createInput(events::ButtonId::Button1, events::ButtonAction::Click);
    TEST_ASSERT(mgr.handleEvent(evt), "App handled event");
    TEST_ASSERT(appA.eventCount == 1, "Event reached active App A");
    TEST_ASSERT(mgr.isDirty(), "App marked dirty after event");

    // 3. Render
    mgr.render(display);
    TEST_ASSERT(appA.renderCount == 1, "App A rendered");
    TEST_ASSERT(!mgr.isDirty(), "Manager not dirty after render");
    TEST_ASSERT(display.getPixel(5, 5) == hal::Color::White, "Pixel drawn by app");

    // 4. Switch app
    TEST_ASSERT(mgr.switchTo("app_b"), "Switched to App B");
    TEST_ASSERT(appA.exitCount == 1, "App A onExit called");
    TEST_ASSERT(appB.enterCount == 1, "App B onEnter called");
    TEST_ASSERT(mgr.getActiveApp() == &appB, "App B is now active");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test TimeService
// -----------------------------------------------------------------------------
static int g_timeEventCount = 0;
static void onTimeEvent(const events::Event& event, void* userData) {
    (void)userData;
    if (event.category == events::EventCategory::Time) {
        g_timeEventCount++;
    }
}

void test_time_service() {
    events::EventBus bus;
    bus.subscribe(events::EventCategory::Time, onTimeEvent);
    g_timeEventCount = 0;

    test::MockRtc rtc(1000000020); // 1000000020 / 60 = 16666667, 1000000020 % 60 = 0 (exact minute boundary)
    services::TimeService timeService(rtc, bus);

    TEST_ASSERT(timeService.init().isOk(), "TimeService init ok");
    TEST_ASSERT(timeService.isRtcHealthy(), "Mock RTC healthy");

    // Initial tick at uptime 0
    timeService.tick(0);
    TEST_ASSERT(g_timeEventCount == 0, "No minute tick yet");

    // Advance 30 seconds: same minute, no event
    rtc.advanceSeconds(30);
    timeService.tick(1500);
    TEST_ASSERT(g_timeEventCount == 0, "Still no minute tick");

    // Advance past minute boundary: should trigger minute tick event on bus!
    rtc.advanceSeconds(35);
    timeService.tick(2500);
    TEST_ASSERT(g_timeEventCount == 1, "Minute tick event dispatched on EventBus");

    // Test timezone offset
    timeService.setTimezoneOffset(330); // UTC+5:30
    TEST_ASSERT(timeService.getTimezoneOffset() == 330, "Timezone offset set");

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Test PowerManager
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

    // Activity tracking
    power.noteActivity(5000);
    TEST_ASSERT(power.getIdleTimeMs(5000) == 0, "Zero idle right after activity");
    TEST_ASSERT(power.getIdleTimeMs(12000) == 7000, "Idle time computed correctly");

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

    printf("\nAll %d test suites passed successfully!\n", testsPassed);
    return 0;
}
