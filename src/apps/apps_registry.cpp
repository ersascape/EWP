#include "apps/apps_registry.h"
#include "watchfaces/app_watchface.h"
#include "apps/app_drawer.h"
#include "apps/app_calendar.h"
#include "apps/app_agenda.h"
#include "apps/app_todo.h"
#include "apps/app_portal.h"
#include "apps/app_status.h"
#include "core/buttons.h"
#include "core/watch_clock.h"
#include "core/debug_log.h"

#if defined(ARDUINO)
#include "hal/esp32/esp32_display.h"
#endif

namespace ersa {
namespace app {

namespace {

Buttons::Event toLegacyButtonEvent(const events::Event& event) {
    if (event.button.button == events::ButtonId::Button1) {
        if (event.type == events::EventType::ButtonClicked) return Buttons::Event::Next;
        if (event.type == events::EventType::ButtonDoubleClicked) return Buttons::Event::Previous;
        if (event.type == events::EventType::ButtonLongPressed) return Buttons::Event::Home;
    } else if (event.button.button == events::ButtonId::Button2) {
        if (event.type == events::EventType::ButtonClicked) return Buttons::Event::Action;
        if (event.type == events::EventType::ButtonDoubleClicked) return Buttons::Event::ActionAlt;
        if (event.type == events::EventType::ButtonLongPressed) return Buttons::Event::ActionLong;
    }
    return Buttons::Event::None;
}

// 1. AppDrawer Application
class DrawerApp : public Application {
public:
    const char* getId() const override { return "app_drawer"; }
    const char* getTitle() const override { return "App Drawer"; }

    void onEvent(const events::Event& event) override {
        const auto legacy = toLegacyButtonEvent(event);
        if (legacy == Buttons::Event::Home) {
            ApplicationManager::instance().switchTo("watchface_clock");
        } else if (legacy == Buttons::Event::Next) {
            AppDrawer::next();
            ApplicationManager::instance().markDirty(false);
        } else if (legacy == Buttons::Event::Action) {
            const auto item = AppDrawer::selected();
            switch (item) {
                case AppDrawer::Item::Clock:
                    ApplicationManager::instance().switchTo("watchface_clock");
                    break;
                case AppDrawer::Item::Calendar:
                    ApplicationManager::instance().switchTo("app_calendar");
                    break;
                case AppDrawer::Item::Agenda:
                    ApplicationManager::instance().switchTo("app_agenda");
                    break;
                case AppDrawer::Item::Todo:
                    ApplicationManager::instance().switchTo("app_todo");
                    break;
                case AppDrawer::Item::Hotspot:
                    ApplicationManager::instance().switchTo("app_portal");
                    break;
                case AppDrawer::Item::Status:
                    ApplicationManager::instance().switchTo("app_status");
                    break;
                default:
                    break;
            }
        }
    }

    void render(hal::IDisplay& display, bool fullRefresh) override {
#if defined(ARDUINO)
        auto* esp = static_cast<hal::Esp32Display*>(&display);
        if (esp) AppDrawer::render(esp->getGfx(), fullRefresh);
#else
        (void)display; (void)fullRefresh;
#endif
    }
};

// 2. AppCalendar Application
class CalendarApp : public Application {
public:
    const char* getId() const override { return "app_calendar"; }
    const char* getTitle() const override { return "Calendar"; }

    void onEnter() override {
        AppCalendar::resetToCurrentMonth(WatchClock::now());
    }

    void onEvent(const events::Event& event) override {
        const auto legacy = toLegacyButtonEvent(event);
        if (legacy == Buttons::Event::Home) {
            ApplicationManager::instance().switchTo("app_drawer");
            return;
        }
        if (AppCalendar::onButton(legacy, WatchClock::now())) {
            ApplicationManager::instance().markDirty(false);
        }
    }

    void render(hal::IDisplay& display, bool fullRefresh) override {
        (void)fullRefresh;
#if defined(ARDUINO)
        auto* esp = static_cast<hal::Esp32Display*>(&display);
        if (esp) AppCalendar::render(esp->getGfx(), WatchClock::now());
#else
        (void)display;
#endif
    }
};

// 3. AppAgenda Application
class AgendaApp : public Application {
public:
    const char* getId() const override { return "app_agenda"; }
    const char* getTitle() const override { return "Agenda"; }

    void onEvent(const events::Event& event) override {
        const auto legacy = toLegacyButtonEvent(event);
        if (legacy == Buttons::Event::Home) {
            ApplicationManager::instance().switchTo("app_drawer");
            return;
        }
        if (AppAgenda::onButton(legacy)) {
            ApplicationManager::instance().markDirty(false);
        }
    }

    void render(hal::IDisplay& display, bool fullRefresh) override {
#if defined(ARDUINO)
        auto* esp = static_cast<hal::Esp32Display*>(&display);
        if (esp) AppAgenda::render(esp->getGfx(), WatchClock::now(), fullRefresh);
#else
        (void)display; (void)fullRefresh;
#endif
    }
};

// 4. AppTodo Application
class TodoApp : public Application {
public:
    const char* getId() const override { return "app_todo"; }
    const char* getTitle() const override { return "Todo"; }

    void onEvent(const events::Event& event) override {
        const auto legacy = toLegacyButtonEvent(event);
        if (legacy == Buttons::Event::Home) {
            ApplicationManager::instance().switchTo("app_drawer");
            return;
        }
        if (AppTodo::onButton(legacy)) {
            ApplicationManager::instance().markDirty(false);
        }
    }

    void render(hal::IDisplay& display, bool fullRefresh) override {
#if defined(ARDUINO)
        auto* esp = static_cast<hal::Esp32Display*>(&display);
        if (esp) AppTodo::render(esp->getGfx(), fullRefresh);
#else
        (void)display; (void)fullRefresh;
#endif
    }
};

// 5. AppPortal Application
class PortalApp : public Application {
public:
    const char* getId() const override { return "app_portal"; }
    const char* getTitle() const override { return "Hotspot Portal"; }

    void onEvent(const events::Event& event) override {
        const auto legacy = toLegacyButtonEvent(event);
        if (legacy == Buttons::Event::Home) {
            ApplicationManager::instance().switchTo("app_drawer");
            return;
        }
        if (AppPortal::onButton(legacy)) {
            ApplicationManager::instance().markDirty(false);
        }
    }

    void render(hal::IDisplay& display, bool fullRefresh) override {
        (void)fullRefresh;
#if defined(ARDUINO)
        auto* esp = static_cast<hal::Esp32Display*>(&display);
        if (esp) AppPortal::render(esp->getGfx());
#else
        (void)display;
#endif
    }

    void tick() override {
        AppPortal::tick();
    }
};

// 6. AppStatus Application
class StatusApp : public Application {
public:
    const char* getId() const override { return "app_status"; }
    const char* getTitle() const override { return "Status"; }

    void onEvent(const events::Event& event) override {
        const auto legacy = toLegacyButtonEvent(event);
        if (legacy == Buttons::Event::Home) {
            ApplicationManager::instance().switchTo("app_drawer");
            return;
        }
        if (AppStatus::onButton(legacy)) {
            ApplicationManager::instance().markDirty(false);
        }
    }

    void render(hal::IDisplay& display, bool fullRefresh) override {
        (void)fullRefresh;
#if defined(ARDUINO)
        auto* esp = static_cast<hal::Esp32Display*>(&display);
        if (esp) AppStatus::render(esp->getGfx());
#else
        (void)display;
#endif
    }
};

static DrawerApp s_drawerApp;
static CalendarApp s_calendarApp;
static AgendaApp s_agendaApp;
static TodoApp s_todoApp;
static PortalApp s_portalApp;
static StatusApp s_statusApp;

} // namespace

void registerAllApps(ApplicationManager& manager) {
    AppDrawer::begin();
    AppCalendar::begin();
    AppAgenda::begin();
    AppTodo::begin();
    AppPortal::begin();

    manager.registerApp(&watchface::AppWatchface::instance());
    manager.registerApp(&s_drawerApp);
    manager.registerApp(&s_calendarApp);
    manager.registerApp(&s_agendaApp);
    manager.registerApp(&s_todoApp);
    manager.registerApp(&s_portalApp);
    manager.registerApp(&s_statusApp);
}

} // namespace app
} // namespace ersa
