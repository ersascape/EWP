#pragma once
#include <stdint.h>
#include <stddef.h>
#include <RTClib.h>

namespace NetSync {

struct CalEvent {
    char title[32];
    char timeStr[20];
};

struct CalTodo {
    char title[36];
    bool completed;
    char uid[36];
};

constexpr size_t MAX_EVENTS = 6;
constexpr size_t MAX_TODOS = 12;

void begin();
bool syncNtp();
bool syncAll();

size_t eventCount();
const CalEvent& getEvent(size_t index);

size_t todoCount();
const CalTodo& getTodo(size_t index);
void toggleTodo(size_t index);

bool isSyncing();
const char* lastStatus();

} // namespace NetSync
