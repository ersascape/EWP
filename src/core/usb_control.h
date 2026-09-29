#pragma once

// Nonblocking USB Serial/JTAG request service. Call begin() once and tick()
// from the firmware's application loop, never from an ISR or BLE callback.
namespace UsbControl {
void begin();
void tick();
}
