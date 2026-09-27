# Watch Base — Stage 1

A PlatformIO **Arduino** project for the custom XIAO ESP32-C3 watch.
Start by validating the RTC, display and buttons. Wireless and MCU sleep are
deliberately deferred until that hardware baseline works on the PCB.

## Open, build, upload

1. Open this whole folder (`Ersa-W1`), containing `platformio.ini`, in VS Code / Code OSS.
2. Press **Ctrl+Shift+B** to run the default **Watch: Build** task.
3. Use **Terminal > Run Task > Watch: Upload** with the board connected.
4. Use **Terminal > Run Task > Watch: Monitor** if the board crashes. The exception decoder
   uses the ELF from this build; capture the complete output before rebuilding.

These tasks invoke the project-local PlatformIO CLI and do not require the
PlatformIO IDE extension. `scripts/pio.sh` bootstraps it with Python's `venv`
on first use. Python 3 with venv/pip and internet access are needed for initial
setup. Tooling and packages live in ignored `.tools/` and `.pio-core/` folders.

Equivalent commands in a normal terminal, from the project folder:

```sh
bash scripts/pio.sh run
bash scripts/pio.sh run --target upload
bash scripts/pio.sh device monitor
```

The active code is in `src/`. `main/` belongs to a native ESP-IDF project and is
not needed here. That previous starter, its editor configuration, and the earlier
OS scaffold have been preserved under `archive/`; none are compiled by PlatformIO.

## Controls and behavior

- B1 short press: next screen (Clock → Date → Status → Clock).
- B2 short press: previous screen.
- Hold either button for 800 ms: return to Clock.
- Large 24-hour clock and date, with updates when the minute changes.
- Lopaka-style home layout: icons at top right, date at (8, 141), time at
  (8, 157). Wireless icons are crossed out while radios are unimplemented;
  battery shows `?` until real sensing is added. All supplied battery variants
  are retained in `src/watch_icons.h` for later integration.
- Fast partial e-paper refreshes, with a full refresh after 20 partial updates.
- Button sampling continues inside the panel BUSY wait; events are applied after
  refresh. E-paper still has visible latency. Rapid clicks may coalesce into one
  refresh of the final selected screen; the bounded queue drops excess events.
- If RTC communication fails, the clock continues in software from its last good
  value and displays an offline notice. The fallback cannot retain time through
  MCU power loss. I2C is retried once per second.

When the DS3231 loses power or contains an invalid date, it is initialized from
compilation time. This is approximate local wall time, including build/upload
delay. To correct an already-running RTC, set `SET_FROM_BUILD` in
`src/watch_clock.cpp` to true for one upload, then restore false and upload again.
Precise time setting and timezone handling are a later stage.

## Small module boundaries

| File | Responsibility |
| --- | --- |
| `include/board_pins.h` | The PCB's raw GPIO assignments |
| `src/main.cpp` | Setup and cooperative application loop |
| `src/buttons.*` | OneButton sampling and bounded navigation event queue |
| `src/watch_clock.*` | DS3231 access, validation, software fallback |
| `src/watch_ui.*` | Screen navigation, drawing, refresh policy |

There is one application task. Display busy callbacks only sample buttons and
enqueue events; they never redraw or mutate the current screen. RTC and display
I/O stay on the main task. The panel drive supply is powered off after refresh;
controller RAM is retained for differential updates. MCU sleep is not enabled.

UART0 uses GPIO20/21, which this PCB assigns to EPD DC/RST. Debug logging uses
USB Serial/JTAG instead, enforced by build flags and a compile-time check.
Logging never waits for a monitor and drops lines if the USB transmit buffer is
full. GxEPD2's verbose diagnostics remain disabled. If the original crash recurs,
collect the full decoded crash report; its cause has not been established.

The supplied PCB JSON establishes **upper S2 = GPIO4 (D2)** and **lower S1 = GPIO3
(D1)**. This corrects the original GPIO0/1 assumption. The user confirmed that
the buzzer shown in this older design is not fitted on the assembled board.
See `docs/pcb-pin-map.md` for the traced connections.

For diagnostics, upload and run **Watch: Monitor**, then press/release each button:

- `BUTTON raw`: actual GPIO levels (released 1, pressed 0).
- `BUTTON event`: debounced NEXT/PREVIOUS/HOME.
- `RTC raw`: DS3231 date and seconds, validity and lost-power flag, each second.
- `LOOP`: software time and pin levels each second, showing the loop is alive.
- `EPD begin/end`: refresh mode, screen number and elapsed time.
- `EPD waiting`: the display's BUSY pin is still asserted.

The watchface only shows HH:MM; use the seconds in the logs to verify ticking.

For battery/unplug faults, Status shows a boot counter and named reset reason.
The last four reset causes are saved in NVS (one record per boot) and printed
when USB logging reconnects. This helps preserve battery-reset evidence if
reopening USB triggers another reset. `BROWNOUT` indicates a supply-voltage
drop; `PANIC` or watchdog reasons point toward a software fault requiring logs.
`POWERON` can also result from a complete loss of supply. Sudden loss before
the record is saved can leave older history, so this is not a complete trace.

## Hardware acceptance before stage 2

1. Clock appears, and crosses at least two minute boundaries without resetting.
2. Both buttons navigate all three screens, including clicks during refresh.
3. Both long presses return to the clock.
4. Reset preserves RTC time; disconnecting main power also preserves it if the
   DS3231 backup battery is working.
5. Status reports RTC online. A missing RTC should produce an offline clock, not
   a frozen loop.

Verified with `pio run`: successful ESP32-C3 build using Espressif32 6.12.0 /
Arduino-ESP32 2.0.17. Static RAM: 19,420 bytes; flash: 286,796 bytes. This is a
compile/link check, not a claim of runtime stability. These physical checks
require the connected PCB; no upload or hardware run has been performed here.
After this stage is stable: time-setting UI, then standby sleep/wake, then BLE,
then Wi-Fi synchronization. Each step should retain a working hardware baseline.
