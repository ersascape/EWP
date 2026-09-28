#include "buttons.h"
#include "board_pins.h"
#include "debug_log.h"
#include <Arduino.h>
#include <OneButton.h>

namespace {
OneButton top(Pins::BUTTON_1, true, true);
OneButton bottom(Pins::BUTTON_2, true, true);

// Only the Arduino task touches this queue. The display's busy callback runs
// on that same task, so sampling during a refresh needs no threads or mutexes.
Buttons::Event events[16];
uint8_t head = 0, tail = 0;

void push(Buttons::Event event) {
    static uint32_t lastEventTime = 0;
    static Buttons::Event lastEvent = Buttons::Event::None;
    const uint32_t now = millis();

    // Suppress rapid duplicate events of the same type within 400ms.
    // This prevents switch release bounce or SPI crosstalk from double-triggering navigation.
    if (event == lastEvent && (now - lastEventTime) < 400) {
        DebugLog::log("BUTTON suppressed duplicate %s (%lu ms)",
                      Buttons::name(event), (unsigned long)(now - lastEventTime));
        return;
    }
    lastEventTime = now;
    lastEvent = event;

    const uint8_t next = (head + 1) % 16;
    if (next != tail) {
        events[head] = event;
        head = next;
        DebugLog::log("BUTTON event=%s", Buttons::name(event));
    } else {
        DebugLog::log("BUTTON queue full");
    }
}

void onTopClick() { push(Buttons::Event::Next); }
void onTopLongPress() { push(Buttons::Event::Home); }

void onBottomClick() { push(Buttons::Event::Action); }
void onBottomLongPress() { push(Buttons::Event::ActionLong); }
} // namespace

const char* Buttons::name(Buttons::Event event) {
    switch (event) {
        case Event::Next: return "NEXT";
        case Event::Previous: return "PREVIOUS";
        case Event::Home: return "HOME";
        case Event::Action: return "ACTION";
        case Event::ActionAlt: return "ACTION_ALT";
        case Event::ActionLong: return "ACTION_LONG";
        default: return "NONE";
    }
}

void Buttons::begin() {
    pinMode(Pins::BUTTON_1, INPUT_PULLUP);
    pinMode(Pins::BUTTON_2, INPUT_PULLUP);

    // 50ms solid debounce for wearable tactile buttons
    top.setDebounceMs(50);
    bottom.setDebounceMs(50);
    top.setPressMs(800);
    bottom.setPressMs(800);

    // Fast, crisp single-click without double-click latency/lingering
    top.attachClick(onTopClick);
    top.attachLongPressStart(onTopLongPress);

    bottom.attachClick(onBottomClick);
    bottom.attachLongPressStart(onBottomLongPress);
}

void Buttons::tick() {
    static int previousTop = -1, previousBottom = -1;
    const int rawTop = digitalRead(Pins::BUTTON_1);
    const int rawBottom = digitalRead(Pins::BUTTON_2);
    if (rawTop != previousTop || rawBottom != previousBottom) {
        DebugLog::log("BUTTON raw GPIO%d=%d GPIO%d=%d (0=pressed)",
                      Pins::BUTTON_1, rawTop, Pins::BUTTON_2, rawBottom);
        previousTop = rawTop;
        previousBottom = rawBottom;
    }
    top.tick();
    bottom.tick();
}

Buttons::Event Buttons::takeEvent() {
    if (head == tail) return Event::None;
    const Event event = events[tail];
    tail = (tail + 1) % 16;
    return event;
}

bool Buttons::isPressed() {
    return (digitalRead(Pins::BUTTON_1) == LOW) || (digitalRead(Pins::BUTTON_2) == LOW);
}

bool Buttons::hasPendingEvents() {
    return head != tail;
}
