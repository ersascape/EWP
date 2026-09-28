#pragma once

#include "application.h"
#include "ersa/events/event.h"
#include "ersa/hal/display.h"
#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace app {

class ApplicationManager {
public:
    static constexpr size_t MAX_APPS = 16;

    ApplicationManager();

    bool registerApp(Application* app);
    bool switchTo(const char* appId);
    bool switchTo(size_t index);

    Application* getActiveApp() const;
    size_t getAppCount() const;
    Application* getApp(size_t index) const;

    // Handle an event: passes to active app and marks dirty if needed
    bool handleEvent(const events::Event& event);

    // Marks the display as needing a render
    void markDirty(bool fullRefresh = false);
    bool isDirty() const;
    bool isFullRefreshNeeded() const;

    // Execute render cycle if dirty
    void render(hal::IDisplay& display);

    void tick();

    static ApplicationManager& instance();

private:
    Application* apps_[MAX_APPS];
    size_t appCount_{0};
    Application* activeApp_{nullptr};

    bool dirty_{true};
    bool fullRefreshNeeded_{true};
    uint32_t lastRenderTime_{0};
};

} // namespace app
} // namespace ersa
