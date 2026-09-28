#pragma once

#include "ersa/common/types.h"
#include "ersa/events/event.h"

namespace ersa {
namespace hal {

class IInput {
public:
    virtual ~IInput() = default;

    virtual Result<void> init() = 0;
    virtual void poll() = 0;
    virtual bool isPressed(events::ButtonId button) const = 0;
};

} // namespace hal
} // namespace ersa
