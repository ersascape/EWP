#include "ersa/services/settings_service.h"

#if defined(ARDUINO)
#include "core/watch_config.h"
#endif

namespace ersa {
namespace services {

static SettingsService* s_settingsInstance = nullptr;

SettingsService& SettingsService::instance() {
    return *s_settingsInstance;
}

void SettingsService::setInstance(SettingsService* instance) {
    s_settingsInstance = instance;
}

#if defined(ARDUINO)
class Esp32SettingsService : public SettingsService {
public:
    Result<void> init() override {
        WatchConfig::begin();
        return Result<void>();
    }

    std::string getWifiSsid() const override {
        return WatchConfig::get().wifiSsid;
    }

    void setWifiSsid(const std::string& ssid) override {
        WatchConfig::setWifi(ssid.c_str(), WatchConfig::get().wifiPass);
    }

    std::string getWifiPass() const override {
        return WatchConfig::get().wifiPass;
    }

    void setWifiPass(const std::string& pass) override {
        WatchConfig::setWifi(WatchConfig::get().wifiSsid, pass.c_str());
    }

    int16_t getTimezoneOffsetMin() const override {
        return WatchConfig::get().timezoneOffsetMin;
    }

    void setTimezoneOffsetMin(int16_t offset) override {
        WatchConfig::setTimezone(offset);
    }

    bool isMilitaryTime() const override {
        return WatchConfig::get().militaryTime;
    }

    void setMilitaryTime(bool military) override {
        WatchConfig::setTimeFormat(military);
    }

    void save() override {
        WatchConfig::save();
    }
};

static Esp32SettingsService s_defaultEspSettings;
#else
class MemorySettingsService : public SettingsService {
public:
    Result<void> init() override { return Result<void>(); }

    std::string getWifiSsid() const override { return ssid_; }
    void setWifiSsid(const std::string& ssid) override { ssid_ = ssid; }

    std::string getWifiPass() const override { return pass_; }
    void setWifiPass(const std::string& pass) override { pass_ = pass; }

    int16_t getTimezoneOffsetMin() const override { return tzOffset_; }
    void setTimezoneOffsetMin(int16_t offset) override { tzOffset_ = offset; }

    bool isMilitaryTime() const override { return military_; }
    void setMilitaryTime(bool military) override { military_ = military; }

    void save() override {}

private:
    std::string ssid_{"TestSSID"};
    std::string pass_{"secret"};
    int16_t tzOffset_{330};
    bool military_{false};
};

static MemorySettingsService s_defaultMemSettings;
#endif

struct SettingsAutoInit {
    SettingsAutoInit() {
#if defined(ARDUINO)
        SettingsService::setInstance(&s_defaultEspSettings);
#else
        SettingsService::setInstance(&s_defaultMemSettings);
#endif
    }
} s_settingsAutoInit;

} // namespace services
} // namespace ersa
