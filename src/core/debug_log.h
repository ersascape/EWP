#pragma once
#include <stdint.h>
#include <stddef.h>

namespace DebugLog {
struct Record { uint32_t sequence; char text[240]; };
void begin();
void log(const char* format, ...) __attribute__((format(printf, 1, 2)));
void tick();
void setProtocolMode(bool enabled);
// Copies oldest-to-newest records newer than cursor. Returns copied count and
// advances cursor to the newest sequence observed, including records skipped
// because the caller's buffer was smaller than the ring.
size_t readSince(uint32_t cursor, Record* out, size_t capacity, uint32_t* nextCursor);
uint32_t latestSequence();
uint32_t bootCount();
const char* resetReasonName();
void flush();
}
