#pragma once

#include <stdint.h>
#include <stdarg.h>

namespace ersa {
namespace services {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class LoggingService {
public:
    virtual ~LoggingService() = default;

    virtual void log(LogLevel level, const char* tag, const char* format, ...) __attribute__((format(printf, 4, 5))) = 0;
    virtual void logv(LogLevel level, const char* tag, const char* format, va_list args) = 0;

    virtual void setLevel(LogLevel level) { minLevel_ = level; }
    virtual LogLevel getLevel() const { return minLevel_; }

    static LoggingService& instance();
    static void setInstance(LoggingService* instance);

protected:
    LogLevel minLevel_{LogLevel::Info};
};

} // namespace services
} // namespace ersa

#define ERSA_LOG_TRACE(tag, fmt, ...) ersa::services::LoggingService::instance().log(ersa::services::LogLevel::Trace, tag, fmt, ##__VA_ARGS__)
#define ERSA_LOG_DEBUG(tag, fmt, ...) ersa::services::LoggingService::instance().log(ersa::services::LogLevel::Debug, tag, fmt, ##__VA_ARGS__)
#define ERSA_LOG_INFO(tag, fmt, ...)  ersa::services::LoggingService::instance().log(ersa::services::LogLevel::Info, tag, fmt, ##__VA_ARGS__)
#define ERSA_LOG_WARN(tag, fmt, ...)  ersa::services::LoggingService::instance().log(ersa::services::LogLevel::Warn, tag, fmt, ##__VA_ARGS__)
#define ERSA_LOG_ERROR(tag, fmt, ...) ersa::services::LoggingService::instance().log(ersa::services::LogLevel::Error, tag, fmt, ##__VA_ARGS__)
#define ERSA_LOG_FATAL(tag, fmt, ...) ersa::services::LoggingService::instance().log(ersa::services::LogLevel::Fatal, tag, fmt, ##__VA_ARGS__)
