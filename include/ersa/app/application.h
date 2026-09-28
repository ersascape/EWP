#pragma once

#include "ersa/common/types.h"
#include "ersa/events/event.h"
#include "ersa/events/event_bus.h"
#include "ersa/hal/display.h"
#include "ersa/ui/canvas.h"
#include <stdint.h>

namespace ersa {
namespace app {

class Application : public events::IEventListener {
public:
    virtual ~Application() = default;

    virtual const char* getId() const = 0;
    virtual const char* getTitle() const = 0;

    // Full lifecycle callbacks (Section 5)
    virtual void onCreate() {}
    virtual void onStart() {}
    virtual void onResume() {}
    virtual void onPause() {}
    virtual void onStop() {}
    virtual void onDestroy() {}

    // Compatibility lifecycle hooks
    virtual void onEnter() {}
    virtual void onExit() {}

    // Event handling
    void onEvent(const events::Event& event) override { (void)event; }

    // Rendering pipeline
    virtual void render(hal::IDisplay& display, bool fullRefresh) { (void)display; (void)fullRefresh; }
    virtual void render(ui::Canvas& canvas) { (void)canvas; }

    // Dynamic partial refresh bounds (sub-window for fast flicker-free updates)
    virtual Rect getPartialBounds() const { return Rect{0, 0, 200, 200}; }

    virtual void tick() {}
};

} // namespace app
} // namespace ersa
