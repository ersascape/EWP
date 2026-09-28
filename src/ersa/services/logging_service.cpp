#include "ersa/services/logging_service.h"
#include <stdio.h>

#if defined(ARDUINO)
#include "core/debug_log.h"
#endif

namespace ersa {
namespace services {

static LoggingService* s_loggingInstance = nullptr;

LoggingService& LoggingService::instance() {
    return *s_loggingInstance;
}

void LoggingService::setInstance(LoggingService* instance) {
    s_loggingInstance = instance;
}

static const char* levelName(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "LOG";
    }
}

class StandardLoggingService : public LoggingService {
public:
    void log(LogLevel level, const char* tag, const char* format, ...) override {
        if (level < minLevel_) return;
        va_list args;
        va_start(args, format);
        logv(level, tag, format, args);
        va_end(args);
    }

    void logv(LogLevel level, const char* tag, const char* format, va_list args) override {
        if (level < minLevel_) return;
        char msgBuf[192];
        vsnprintf(msgBuf, sizeof(msgBuf), format, args);

#if defined(ARDUINO)
        DebugLog::log("[%s] [%s] %s", levelName(level), tag ? tag : "Ersa", msgBuf);
#else
        printf("[%s] [%s] %s\n", levelName(level), tag ? tag : "Ersa", msgBuf);
#endif
    }
};

static StandardLoggingService s_defaultLogger;

struct LoggerAutoInit {
    LoggerAutoInit() {
        LoggingService::setInstance(&s_defaultLogger);
    }
} s_loggerInit;

} // namespace services
} // namespace ersa
