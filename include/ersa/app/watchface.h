#pragma once

#include "ersa/app/application.h"

namespace ersa {
namespace app {

class Watchface : public Application {
public:
    ~Watchface() override = default;

    const char* getId() const override { return "watchface"; }
    const char* getTitle() const override { return "Watchface"; }

    virtual void render(ui::Canvas& canvas) override = 0;
};

} // namespace app
} // namespace ersa
