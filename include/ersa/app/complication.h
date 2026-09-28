#pragma once

#include <string>

namespace ersa {
namespace app {

struct ComplicationData {
    std::string text;
};

class ComplicationProvider {
public:
    virtual ~ComplicationProvider() = default;
    virtual ComplicationData get() = 0;
};

} // namespace app
} // namespace ersa
