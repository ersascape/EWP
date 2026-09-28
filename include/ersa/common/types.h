#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ersa {

enum class ErrorCode : int {
    Ok = 0,
    InvalidParam = -1,
    OutOfMemory = -2,
    NotFound = -3,
    Busy = -4,
    Timeout = -5,
    NotSupported = -6,
    HardwareFault = -7,
    IoError = -8
};

struct Error {
    ErrorCode code{ErrorCode::Ok};
    const char* message{""};

    constexpr Error() = default;
    constexpr Error(ErrorCode c, const char* msg = "") : code(c), message(msg) {}

    constexpr bool isOk() const { return code == ErrorCode::Ok; }
    constexpr bool isError() const { return code != ErrorCode::Ok; }
};

template <typename T>
class Result {
public:
    Result(const T& val) : value_(val), error_(ErrorCode::Ok) {}
    Result(T&& val) : value_(static_cast<T&&>(val)), error_(ErrorCode::Ok) {}
    Result(const Error& err) : error_(err) {}
    Result(ErrorCode code, const char* msg = "") : error_(code, msg) {}

    bool isOk() const { return error_.isOk(); }
    bool isError() const { return error_.isError(); }

    const T& value() const { return value_; }
    T& value() { return value_; }

    const Error& error() const { return error_; }

private:
    T value_{};
    Error error_{ErrorCode::Ok};
};

// Void specialization for Result
template <>
class Result<void> {
public:
    Result() : error_(ErrorCode::Ok) {}
    Result(const Error& err) : error_(err) {}
    Result(ErrorCode code, const char* msg = "") : error_(code, msg) {}

    bool isOk() const { return error_.isOk(); }
    bool isError() const { return error_.isError(); }
    const Error& error() const { return error_; }

private:
    Error error_{ErrorCode::Ok};
};

struct Point {
    int16_t x{0};
    int16_t y{0};

    constexpr Point() = default;
    constexpr Point(int16_t px, int16_t py) : x(px), y(py) {}
};

struct Rect {
    int16_t x{0};
    int16_t y{0};
    int16_t w{0};
    int16_t h{0};

    constexpr Rect() = default;
    constexpr Rect(int16_t rx, int16_t ry, int16_t rw, int16_t rh)
        : x(rx), y(ry), w(rw), h(rh) {}

    constexpr bool isEmpty() const { return w <= 0 || h <= 0; }
    constexpr bool contains(int16_t px, int16_t py) const {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

} // namespace ersa
