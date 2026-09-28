#pragma once

#include "ersa/common/types.h"
#include <string>
#include <stdint.h>

namespace ersa {
namespace services {

class SettingsService {
public:
    virtual ~SettingsService() = default;

    virtual Result<void> init() = 0;

    virtual std::string getWifiSsid() const = 0;
    virtual void setWifiSsid(const std::string& ssid) = 0;

    virtual std::string getWifiPass() const = 0;
    virtual void setWifiPass(const std::string& pass) = 0;

    virtual int16_t getTimezoneOffsetMin() const = 0;
    virtual void setTimezoneOffsetMin(int16_t offset) = 0;

    virtual bool isMilitaryTime() const = 0;
    virtual void setMilitaryTime(bool military) = 0;

    virtual void save() = 0;

    static SettingsService& instance();
    static void setInstance(SettingsService* instance);
};

} // namespace services
} // namespace ersa
