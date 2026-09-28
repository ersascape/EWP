#pragma once

#include <stdint.h>
#include <stddef.h>

#if __cplusplus >= 201703L
#include <string_view>
#endif

namespace ersa {
namespace ui {

enum class Color : uint8_t {
    Black = 0,
    White = 1,
    Inverse = 2
};

class Canvas {
public:
    virtual ~Canvas() = default;

    virtual int width() const = 0;
    virtual int height() const = 0;

    virtual void clear(Color color = Color::Black) = 0;
    virtual void drawPixel(int x, int y, Color color = Color::White) = 0;
    virtual void drawLine(int x1, int y1, int x2, int y2, Color color = Color::White) = 0;
    virtual void drawRect(int x, int y, int w, int h, Color color = Color::White) = 0;
    virtual void fillRect(int x, int y, int w, int h, Color color = Color::White) = 0;
    virtual void drawText(int x, int y, const char* text, Color color = Color::White) = 0;

#if __cplusplus >= 201703L
    virtual void drawText(int x, int y, std::string_view text, Color color = Color::White) {
        // string_view may not be null-terminated; draw up to length
        char buf[128];
        size_t len = text.length() < sizeof(buf) - 1 ? text.length() : sizeof(buf) - 1;
        for (size_t i = 0; i < len; ++i) buf[i] = text[i];
        buf[len] = '\0';
        drawText(x, y, buf, color);
    }
#endif
};

} // namespace ui
} // namespace ersa
