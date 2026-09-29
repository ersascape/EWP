#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace ersa { namespace protocols {
// AMS Entity Update decoding has no dependency on the BLE stack or device.
struct AmsMedia {
    char title[32]{};
    char artist[32]{};
    bool playing{false};
    bool update(const uint8_t* bytes, size_t length) {
        if (length < 3) return false;
        const uint8_t entity = bytes[0], attribute = bytes[1];
        const uint8_t* text = bytes + 3;
        const size_t size = length - 3;
        if (entity == 2 && (attribute == 0 || attribute == 2)) {
            char* target = attribute == 0 ? artist : title;
            const size_t count = size < 31 ? size : 31;
            char next[32]{};
            memcpy(next, text, count);
            if (strcmp(target, next) == 0) return false;
            memcpy(target, next, sizeof(next));
            return true;
        }
        if (entity == 0 && attribute == 1 && size >= 2 && text[1] == ',' && text[0] >= '0' && text[0] <= '3') {
            const bool nextPlaying = text[0] != '0';
            if (playing == nextPlaying) return false;
            playing = nextPlaying;
            return true;
        }
        return false;
    }
};
} }
