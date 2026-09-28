#pragma once

#include <stdio.h>

#if defined(ARDUINO)
#include "core/debug_log.h"
#define ERSA_LOG_INFO(fmt, ...)  DebugLog::log(fmt, ##__VA_ARGS__)
#define ERSA_LOG_WARN(fmt, ...)  DebugLog::log("[WARN] " fmt, ##__VA_ARGS__)
#define ERSA_LOG_ERROR(fmt, ...) DebugLog::log("[ERROR] " fmt, ##__VA_ARGS__)
#else
#define ERSA_LOG_INFO(fmt, ...)  printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define ERSA_LOG_WARN(fmt, ...)  printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define ERSA_LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif
