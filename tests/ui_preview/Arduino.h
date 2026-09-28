#pragma once
// Minimal host surface for compiling the actual Adafruit_GFX drawing code.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <math.h>
#define PROGMEM
class __FlashStringHelper;
class String : public std::string { public: using std::string::string; };
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
    size_t write(const uint8_t* text, size_t length) {
        for (size_t i = 0; i < length; ++i) write(text[i]);
        return length;
    }
    void print(const char* text) { while (*text) write(uint8_t(*text++)); }
    void print(unsigned value) { char text[16]; snprintf(text, sizeof(text), "%u", value); print(text); }
};
inline float radians(float degrees) { return degrees * 0.017453292519943f; }
