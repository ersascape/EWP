#pragma once

#include "ersa/common/types.h"
#include "ersa/events/event.h"
#include "ersa/hal/display.h"
#include <stdint.h>

namespace ersa {
namespace app {

class Application {
public:
    virtual ~Application() = default;

    virtual const char* getId() const = 0;
    virtual const char* getTitle() const = 0;

    virtual void onEnter() {}
    virtual void onExit() {}

    // Process event. Return true if state changed and redraw is needed.
    virtual bool onEvent(const events::Event& event) {
        (void)event;
        return false;
    }

    // Render application UI
    virtual void render(hal::IDisplay& display, bool fullRefresh) = 0;

    // Periodic tick if app needs idle processing
    virtual void tick() {}
};

} // namespace app
} // namespace ersa
