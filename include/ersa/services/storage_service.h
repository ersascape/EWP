#pragma once

#include "ersa/common/types.h"
#include <string>
#include <string_view>
#include <stdint.h>

namespace ersa {
namespace services {

class StorageService {
public:
    virtual ~StorageService() = default;

    virtual Result<void> init() = 0;

    virtual bool setString(std::string_view key, std::string_view value) = 0;
    virtual std::string getString(std::string_view key, std::string_view defaultValue = "") = 0;

    virtual bool setInt(std::string_view key, int32_t value) = 0;
    virtual int32_t getInt(std::string_view key, int32_t defaultValue = 0) = 0;

    virtual bool setBool(std::string_view key, bool value) = 0;
    virtual bool getBool(std::string_view key, bool defaultValue = false) = 0;

    virtual bool remove(std::string_view key) = 0;
    virtual void clear() = 0;

    static StorageService& instance();
    static void setInstance(StorageService* instance);
};

} // namespace services
} // namespace ersa
