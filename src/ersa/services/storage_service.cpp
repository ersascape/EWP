#include "ersa/services/storage_service.h"
#include <unordered_map>

#if defined(ARDUINO)
#include <Preferences.h>
#endif

namespace ersa {
namespace services {

static StorageService* s_storageInstance = nullptr;

StorageService& StorageService::instance() {
    return *s_storageInstance;
}

void StorageService::setInstance(StorageService* instance) {
    s_storageInstance = instance;
}

#if defined(ARDUINO)
class Esp32StorageService : public StorageService {
public:
    Result<void> init() override {
        prefs_.begin("ersa_nvs", false);
        return Result<void>();
    }

    bool setString(std::string_view key, std::string_view value) override {
        std::string k(key);
        std::string v(value);
        return prefs_.putString(k.c_str(), v.c_str()) > 0;
    }

    std::string getString(std::string_view key, std::string_view defaultValue) override {
        std::string k(key);
        std::string def(defaultValue);
        String val = prefs_.getString(k.c_str(), def.c_str());
        return std::string(val.c_str());
    }

    bool setInt(std::string_view key, int32_t value) override {
        std::string k(key);
        return prefs_.putInt(k.c_str(), value) > 0;
    }

    int32_t getInt(std::string_view key, int32_t defaultValue) override {
        std::string k(key);
        return prefs_.getInt(k.c_str(), defaultValue);
    }

    bool setBool(std::string_view key, bool value) override {
        std::string k(key);
        return prefs_.putBool(k.c_str(), value) > 0;
    }

    bool getBool(std::string_view key, bool defaultValue) override {
        std::string k(key);
        return prefs_.getBool(k.c_str(), defaultValue);
    }

    bool remove(std::string_view key) override {
        std::string k(key);
        return prefs_.remove(k.c_str());
    }

    void clear() override {
        prefs_.clear();
    }

private:
    Preferences prefs_;
};

static Esp32StorageService s_defaultEspStorage;
#else
class MemoryStorageService : public StorageService {
public:
    Result<void> init() override {
        map_.clear();
        return Result<void>();
    }

    bool setString(std::string_view key, std::string_view value) override {
        map_[std::string(key)] = std::string(value);
        return true;
    }

    std::string getString(std::string_view key, std::string_view defaultValue) override {
        auto it = map_.find(std::string(key));
        if (it != map_.end()) return it->second;
        return std::string(defaultValue);
    }

    bool setInt(std::string_view key, int32_t value) override {
        map_[std::string(key)] = std::to_string(value);
        return true;
    }

    int32_t getInt(std::string_view key, int32_t defaultValue) override {
        auto it = map_.find(std::string(key));
        if (it != map_.end()) {
            try { return std::stoi(it->second); } catch (...) {}
        }
        return defaultValue;
    }

    bool setBool(std::string_view key, bool value) override {
        map_[std::string(key)] = value ? "1" : "0";
        return true;
    }

    bool getBool(std::string_view key, bool defaultValue) override {
        auto it = map_.find(std::string(key));
        if (it != map_.end()) return it->second == "1";
        return defaultValue;
    }

    bool remove(std::string_view key) override {
        return map_.erase(std::string(key)) > 0;
    }

    void clear() override {
        map_.clear();
    }

private:
    std::unordered_map<std::string, std::string> map_;
};

static MemoryStorageService s_defaultMemStorage;
#endif

// Auto-register default instance on startup
struct StorageAutoInit {
    StorageAutoInit() {
#if defined(ARDUINO)
        StorageService::setInstance(&s_defaultEspStorage);
#else
        StorageService::setInstance(&s_defaultMemStorage);
#endif
    }
} s_autoInit;

} // namespace services
} // namespace ersa
